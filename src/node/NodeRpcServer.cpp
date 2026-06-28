#include "node/NodeRpcServer.hpp"

#include "core/LedgerRecord.hpp"
#include "core/Transaction.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Address.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/PublicKey.hpp"
#include "mempool/Mempool.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "utils/Amount.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
   using platform_ssize_t = int;
   namespace { int close_socket(int fd) { return static_cast<int>(::closesocket(static_cast<SOCKET>(fd))); } }
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
   using platform_ssize_t = ssize_t;
   namespace { int close_socket(int fd) { return ::close(fd); } }
#endif

namespace nodo::node {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

const std::string kRpcSubmitSchemaId =
    "NODO_RPC_TRANSACTION_SUBMISSION_V1";

const std::set<std::string> kRpcSubmitFields = {
    "transaction"
};

std::string jsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    out += '"';
    return out;
}

bool sendAll(int fd, const std::string& data) {
    const char* cursor = data.data();
    std::size_t remaining = data.size();

    while (remaining > 0) {
        const platform_ssize_t sent = ::send(fd, cursor, remaining, 0);
        if (sent <= 0) {
            return false;
        }

        cursor += sent;
        remaining -= static_cast<std::size_t>(sent);
    }

    return true;
}

bool parseUint64Strict(const std::string& value, std::uint64_t& out) {
    if (value.empty()) {
        return false;
    }

    for (const char c : value) {
        if (c < '0' || c > '9') {
            return false;
        }
    }

    try {
        std::size_t parsedCharacters = 0;
        const unsigned long long parsed = std::stoull(value, &parsedCharacters);
        if (parsedCharacters != value.size()) {
            return false;
        }
        out = static_cast<std::uint64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

core::Transaction parseSignedTransactionSubmission(
    const std::string& body
) {
    const serialization::KeyValueFileDocument fields =
        serialization::KeyValueFileCodec::parse(body, kRpcSubmitSchemaId);

    fields.requireOnlyFields(kRpcSubmitFields);

    const std::string transactionText =
        fields.requireField("transaction");

    return core::Transaction::deserialize(transactionText);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NodeRpcServer::NodeRpcServer(
    NodeRuntime&       runtime,
    std::uint16_t      port,
    const std::string& bindAddr
)
    : m_runtime(runtime)
    , m_gossip(nullptr)
    , m_port(port)
    , m_bindAddr(bindAddr)
    , m_running(false)
    , m_serverFd(-1)
{}

NodeRpcServer::NodeRpcServer(
    NodeRuntime&       runtime,
    p2p::GossipMesh&   gossip,
    std::uint16_t      port,
    const std::string& bindAddr
)
    : m_runtime(runtime)
    , m_gossip(&gossip)
    , m_port(port)
    , m_bindAddr(bindAddr)
    , m_running(false)
    , m_serverFd(-1)
{}

NodeRpcServer::~NodeRpcServer() {
    stop();
}

std::uint16_t NodeRpcServer::port() const { return m_port; }

bool NodeRpcServer::isRunning() const { return m_running.load(); }

// ---------------------------------------------------------------------------
// Start / Stop
// ---------------------------------------------------------------------------

void NodeRpcServer::start() {
    if (m_running.load()) return;

    m_serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) {
        throw std::runtime_error(
            std::string("NodeRpcServer: socket() failed: ") + strerror(errno)
        );
    }

    int yes = 1;
    ::setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(m_port);
    if (::inet_pton(AF_INET, m_bindAddr.c_str(), &addr.sin_addr) <= 0) {
        close_socket(m_serverFd);
        throw std::runtime_error(
            "NodeRpcServer: invalid bind address: " + m_bindAddr
        );
    }

    if (::bind(m_serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_socket(m_serverFd);
        throw std::runtime_error(
            std::string("NodeRpcServer: bind() failed: ") + strerror(errno)
        );
    }

    if (::listen(m_serverFd, 16) < 0) {
        close_socket(m_serverFd);
        throw std::runtime_error(
            std::string("NodeRpcServer: listen() failed: ") + strerror(errno)
        );
    }

    m_running.store(true);
    m_thread = std::thread([this]{ runLoop(); });
}

void NodeRpcServer::stop() {
    m_running.store(false);
    if (m_serverFd >= 0) {
        close_socket(m_serverFd);
        m_serverFd = -1;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void NodeRpcServer::runLoop() {
    while (m_running.load()) {
        struct sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int clientFd = ::accept(
            m_serverFd,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &addrLen
        );
        if (clientFd < 0) {
            if (!m_running.load()) break;
            continue;
        }
        handleClient(clientFd);
        close_socket(clientFd);
    }
}

// ---------------------------------------------------------------------------
// HTTP framing
// ---------------------------------------------------------------------------

void NodeRpcServer::handleClient(int clientFd) {
    char buf[MAX_REQUEST_LEN + 1];
    platform_ssize_t n = ::recv(clientFd, buf, MAX_REQUEST_LEN, 0);
    if (n <= 0) return;
    buf[n] = '\0';

    std::string request(buf, static_cast<std::size_t>(n));

    std::string method, path, body;
    if (!parseRequestLine(request, method, path, body)) {
        const std::string resp = httpResponse(400, jsonError("Bad request"));
        (void)sendAll(clientFd, resp);
        return;
    }

    std::pair<int, std::string> response;
    try {
        response = dispatch(method, path, body);
    } catch (const std::exception& e) {
        response = {
            500,
            jsonError(std::string("Internal RPC error: ") + e.what())
        };
    }

    const auto [statusCode, responseBody] = response;
    const std::string resp = httpResponse(statusCode, responseBody);
    (void)sendAll(clientFd, resp);
}

bool NodeRpcServer::parseRequestLine(
    const std::string& request,
    std::string&       outMethod,
    std::string&       outPath,
    std::string&       outBody
) {
    const std::size_t lineEnd = request.find("\r\n");
    if (lineEnd == std::string::npos) return false;

    const std::string firstLine = request.substr(0, lineEnd);
    std::istringstream iss(firstLine);
    std::string proto;
    if (!(iss >> outMethod >> outPath >> proto)) return false;

    // Body follows the blank line.
    const std::size_t bodyPos = request.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        outBody = request.substr(bodyPos + 4);
    }
    return true;
}

std::string NodeRpcServer::httpResponse(int statusCode, const std::string& body) {
    std::string statusText;
    switch (statusCode) {
        case 200: statusText = "OK";            break;
        case 400: statusText = "Bad Request";   break;
        case 404: statusText = "Not Found";     break;
        case 405: statusText = "Method Not Allowed"; break;
        case 500: statusText = "Internal Server Error"; break;
        default:  statusText = "Unknown";       break;
    }

    std::ostringstream oss;
    oss << "HTTP/1.0 " << statusCode << " " << statusText << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

std::string NodeRpcServer::jsonError(const std::string& message) {
    return "{\"error\":" + jsonString(message) + "}";
}

std::string NodeRpcServer::pathSegment(const std::string& path, int index) {
    std::size_t pos = 0;
    int seg = -1;
    while (pos < path.size()) {
        std::size_t slash = path.find('/', pos);
        if (slash == std::string::npos) slash = path.size();
        if (seg == index) {
            return path.substr(pos, slash - pos);
        }
        ++seg;
        pos = slash + 1;
    }
    return "";
}

// ---------------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------------

std::pair<int, std::string> NodeRpcServer::dispatch(
    const std::string& method,
    const std::string& path,
    const std::string& body
) {
    // Normalize path: strip query string.
    const std::string cleanPath = path.substr(0, path.find('?'));

    // Segment 0 is empty (before first '/'), segment 1 is the route.
    const std::string route = pathSegment(cleanPath, 1);
    const std::string param = pathSegment(cleanPath, 2);

    if (route == "status" && method == "GET") {
        return {200, handleStatus()};
    }
    if (route == "block" && method == "GET") {
        if (param.empty()) return {400, jsonError("Missing block height")};
        return {200, handleBlock(param)};
    }
    if (route == "tx" && method == "GET") {
        if (param.empty()) return {400, jsonError("Missing tx id")};
        return {200, handleTx(param)};
    }
    if (route == "account" && method == "GET") {
        if (param.empty()) return {400, jsonError("Missing address")};
        return {200, handleAccount(param)};
    }
    if (route == "validators" && method == "GET") {
        return {200, handleValidators()};
    }
    if (route == "peers" && method == "GET") {
        return {200, handlePeers()};
    }
    if (route == "mempool" && method == "GET") {
        return {200, handleMempool()};
    }
    if (route == "submit" && method == "POST") {
        return {200, handleSubmit(body)};
    }
    if (route == "submit") {
        return {405, jsonError("Method not allowed. Use POST.")};
    }

    return {404, jsonError("Not found: " + cleanPath)};
}

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------

std::string NodeRpcServer::handleStatus() const {
    const auto& chain   = m_runtime.blockchain();
    const auto& mgr     = m_runtime.consensusRoundManager();
    const auto& peers   = m_runtime.peerManager();
    const auto& mempool = m_runtime.mempool();

    const std::uint64_t height =
        chain.empty() ? 0 : chain.latestBlock().index();
    const std::uint64_t round =
        mgr.currentState().round();
    const bool running = m_runtime.isRunning();

    std::ostringstream oss;
    oss << "{"
        << "\"height\":" << height
        << ",\"round\":" << round
        << ",\"peerCount\":" << peers.size()
        << ",\"mempoolSize\":" << mempool.size()
        << ",\"running\":" << (running ? "true" : "false")
        << "}";
    return oss.str();
}

std::string NodeRpcServer::handleBlock(const std::string& heightStr) const {
    std::uint64_t height = 0;
    if (!parseUint64Strict(heightStr, height)) {
        return jsonError("Invalid height: " + heightStr);
    }

    const auto& blocks = m_runtime.blockchain().blocks();
    if (height >= blocks.size()) {
        return jsonError("Block not found at height " + heightStr);
    }

    const core::Block& block = blocks[static_cast<std::size_t>(height)];
    std::ostringstream oss;
    oss << "{"
        << "\"height\":" << block.index()
        << ",\"hash\":" << jsonString(block.hash())
        << ",\"previousHash\":" << jsonString(block.previousHash())
        << ",\"timestamp\":" << block.timestamp()
        << ",\"recordCount\":" << block.records().size()
        << "}";
    return oss.str();
}

std::string NodeRpcServer::handleTx(const std::string& txId) const {
    for (const auto& block : m_runtime.blockchain().blocks()) {
        for (const auto& record : block.records()) {
            if (record.id() == txId || record.sourceId() == txId) {
                std::ostringstream oss;
                oss << "{"
                    << "\"id\":" << jsonString(record.id())
                    << ",\"sourceId\":" << jsonString(record.sourceId())
                    << ",\"type\":" << jsonString(
                           core::ledgerRecordTypeToString(record.type()))
                    << ",\"blockHeight\":" << block.index()
                    << ",\"timestamp\":" << record.timestamp()
                    << "}";
                return oss.str();
            }
        }
    }
    return jsonError("Transaction not found: " + txId);
}

std::string NodeRpcServer::handleAccount(const std::string& address) const {
    const std::uint64_t minimumFeeRaw =
        m_runtime.effectiveMinimumFeeRawUnits();
    if (minimumFeeRaw > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        return jsonError("Network minimum fee exceeds supported range.");
    }
    const core::AccountState account =
        m_runtime.cachedAccountStateAtTip(
            static_cast<std::int64_t>(minimumFeeRaw)
        ).accountOrDefault(address);

    std::ostringstream oss;
    oss << "{"
        << "\"address\":" << jsonString(address)
        << ",\"balance\":" << account.balance().rawUnits()
        << ",\"nonce\":" << account.nonce()
        << "}";
    return oss.str();
}

std::string NodeRpcServer::handleValidators() const {
    const std::vector<std::string> addresses =
        m_runtime.validatorRegistry().activeValidatorAddresses();

    std::ostringstream oss;
    oss << "{\"validators\":[";
    for (std::size_t i = 0; i < addresses.size(); ++i) {
        if (i > 0) oss << ",";
        oss << jsonString(addresses[i]);
    }
    oss << "],\"count\":" << addresses.size() << "}";
    return oss.str();
}

std::string NodeRpcServer::handlePeers() const {
    const auto peers = m_runtime.peerManager().peers();

    std::ostringstream oss;
    oss << "{\"peers\":[";
    for (std::size_t i = 0; i < peers.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{"
            << "\"peerId\":" << jsonString(peers[i].peerId())
            << ",\"endpoint\":" << jsonString(peers[i].endpoint())
            << ",\"latestKnownHeight\":" << peers[i].latestKnownHeight()
            << "}";
    }
    oss << "],\"count\":" << peers.size() << "}";
    return oss.str();
}

std::string NodeRpcServer::handleMempool() const {
    const auto& pool = m_runtime.mempool();
    const auto pending = pool.transactionsForBlock(20);

    std::ostringstream oss;
    oss << "{\"size\":" << pool.size()
        << ",\"transactions\":[";
    for (std::size_t i = 0; i < pending.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& tx = pending[i];
        oss << "{"
            << "\"id\":" << jsonString(tx.id())
            << ",\"from\":" << jsonString(tx.fromAddress())
            << ",\"to\":" << jsonString(tx.toAddress())
            << ",\"amount\":" << tx.amount().rawUnits()
            << ",\"fee\":" << tx.fee().rawUnits()
            << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string NodeRpcServer::handleSubmit(const std::string& body) {
    if (body.empty()) {
        return jsonError("Empty request body");
    }

    std::optional<core::Transaction> parsedTx;
    try {
        parsedTx = [&]() {
            if (body.rfind(kRpcSubmitSchemaId, 0) == 0) {
                return parseSignedTransactionSubmission(body);
            }

            if (body.rfind("Transaction{", 0) == 0) {
                throw std::invalid_argument(
                    "Raw Transaction serialization is not self-contained for RPC submit; "
                    "send a NODO_RPC_TRANSACTION_SUBMISSION_V1 envelope with public key material."
                );
            }

            throw std::invalid_argument("Unsupported submit payload schema.");
        }();
    } catch (const std::exception& e) {
        return jsonError(std::string("Invalid submit payload: ") + e.what());
    }

    const core::Transaction& tx = parsedTx.value();
    if (tx.id().empty() || !tx.hasSignatureBundle()) {
        return jsonError("Failed to deserialize signed transaction");
    }

    const std::int64_t now =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    const config::GenesisConfig& genesisConfig =
        m_runtime.config().genesisConfig();
    const config::NetworkParameters& networkParameters =
        genesisConfig.networkParameters();

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            networkParameters.networkName()
        );

    if (!cryptoContext.isValid()) {
        return jsonError(
            "Runtime crypto context is invalid: " +
            cryptoContext.rejectionReason()
        );
    }

    if (networkParameters.minimumFeeRawUnits() >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return jsonError("Network minimum fee exceeds supported Amount range.");
    }

    const core::AccountStateView accountState =
        RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesisConfig,
            m_runtime.blockchain(),
            static_cast<std::int64_t>(
                networkParameters.minimumFeeRawUnits()
            )
        );

    const TransactionAdmissionResult validation =
        TransactionAdmissionValidator::validateNetworkSubmission(
            tx,
            networkParameters,
            accountState,
            m_runtime.mempool(),
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            cryptoContext.userSignatureProvider(),
            m_runtime.effectiveMinimumFeeRawUnits()
        );

    if (!validation.accepted()) {
        return jsonError(
            "Transaction rejected: " +
            transactionAdmissionStatusToString(validation.status()) +
            ": " +
            validation.reason()
        );
    }

    const mempool::MempoolAdmissionResult result =
        m_runtime.mutableMempool().admitTransaction(
            tx,
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            now
        );

    // Broadcast to peers if gossip is wired in and the transaction was accepted.
    if (m_gossip != nullptr &&
        result.status() == mempool::MempoolAdmissionStatus::ACCEPTED) {
        m_gossip->broadcast(
            p2p::NetworkMessageType::TRANSACTION_ANNOUNCE,
            body,
            now
        );
    }

    std::ostringstream oss;
    oss << "{"
        << "\"status\":" << jsonString(
               mempool::mempoolAdmissionStatusToString(result.status()))
        << ",\"txId\":" << jsonString(result.transactionId())
        << ",\"reason\":" << jsonString(result.reason())
        << "}";
    return oss.str();
}

} // namespace nodo::node
