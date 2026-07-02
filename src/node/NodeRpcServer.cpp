#include "node/NodeRpcServer.hpp"

#include "core/LedgerRecord.hpp"
#include "core/StateRootCalculator.hpp"
#include "core/Transaction.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/Address.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "mempool/Mempool.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "p2p/EncryptedPeerTransport.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "utils/Amount.hpp"

#include <cerrno>
#include <iostream>

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
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using platform_ssize_t = int;
namespace {
int close_socket(int fd) {
  return static_cast<int>(::closesocket(static_cast<SOCKET>(fd)));
}
} // namespace
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using platform_ssize_t = ssize_t;
namespace {
int close_socket(int fd) { return ::close(fd); }
} // namespace
#endif

namespace nodo::node {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

const std::string kRpcSubmitSchemaId = "NODO_RPC_TRANSACTION_SUBMISSION_V1";

const std::set<std::string> kRpcSubmitFields = {"transaction"};

std::string jsonString(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 2);
  out += '"';
  for (char c : s) {
    if (c == '"')
      out += "\\\"";
    else if (c == '\\')
      out += "\\\\";
    else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c == '\t')
      out += "\\t";
    else
      out += c;
  }
  out += '"';
  return out;
}

bool sendAll(int fd, const std::string &data) {
#if !defined(_WIN32) && defined(SO_NOSIGPIPE)
  int suppressSigPipe = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &suppressSigPipe,
                   sizeof(suppressSigPipe)) != 0) {
    return false;
  }
#endif

  const char *cursor = data.data();
  std::size_t remaining = data.size();

  while (remaining > 0) {
    const platform_ssize_t sent = ::send(fd, cursor, remaining,
#if !defined(_WIN32) && defined(MSG_NOSIGNAL)
                                         MSG_NOSIGNAL
#else
                                         0
#endif
    );
    if (sent < 0) {
#if !defined(_WIN32)
      if (errno == EINTR) {
        continue;
      }
#endif
      return false;
    }
    if (sent == 0) {
      return false;
    }

    cursor += sent;
    remaining -= static_cast<std::size_t>(sent);
  }

  return true;
}

bool parseUint64Strict(const std::string &value, std::uint64_t &out) {
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

bool asciiCaseEqual(const std::string &value, const std::string &expected) {
  if (value.size() != expected.size()) {
    return false;
  }
  for (std::size_t index = 0; index < value.size(); ++index) {
    char left = value[index];
    char right = expected[index];
    if (left >= 'A' && left <= 'Z')
      left = static_cast<char>(left + ('a' - 'A'));
    if (right >= 'A' && right <= 'Z')
      right = static_cast<char>(right + ('a' - 'A'));
    if (left != right)
      return false;
  }
  return true;
}

std::string jsonStakePosition(const StakePositionView &position) {
  std::ostringstream oss;
  oss << "{"
      << "\"positionId\":" << jsonString(position.positionId)
      << ",\"ownerAddress\":" << jsonString(position.ownerAddress)
      << ",\"validatorAddress\":" << jsonString(position.validatorAddress)
      << ",\"status\":"
      << jsonString(stakePositionStatusToString(position.status))
      << ",\"activeRawUnits\":" << position.activeAmount.rawUnits()
      << ",\"pendingActivationRawUnits\":"
      << position.pendingActivationAmount.rawUnits()
      << ",\"pendingUnbondingRawUnits\":"
      << position.pendingUnbondingAmount.rawUnits()
      << ",\"withdrawnRawUnits\":" << position.withdrawnAmount.rawUnits()
      << ",\"slashedRawUnits\":" << position.slashedAmount.rawUnits()
      << ",\"rewardsPendingRawUnits\":" << position.rewardsPending.rawUnits()
      << ",\"lockHeight\":" << position.lockHeight
      << ",\"activationHeight\":" << position.activationHeight
      << ",\"unbondingStartHeight\":" << position.unbondingStartHeight
      << ",\"withdrawableHeight\":" << position.withdrawableHeight << "}";
  return oss.str();
}

std::string trimHttpWhitespace(const std::string &value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         (value[begin] == ' ' || value[begin] == '\t')) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t')) {
    --end;
  }
  return value.substr(begin, end - begin);
}

bool receiveHttpRequest(int fd, std::string &request) {
  request.clear();
  std::optional<std::size_t> expectedSize;

  while (request.size() < NodeRpcServer::MAX_REQUEST_LEN) {
    char buffer[4096];
    const std::size_t remaining =
        NodeRpcServer::MAX_REQUEST_LEN - request.size();
    const std::size_t capacity =
        remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    const platform_ssize_t received = ::recv(fd, buffer, capacity, 0);
    if (received < 0) {
#if !defined(_WIN32)
      if (errno == EINTR)
        continue;
#endif
      return false;
    }
    if (received == 0) {
      return false;
    }
    request.append(buffer, static_cast<std::size_t>(received));

    if (!expectedSize.has_value()) {
      const std::size_t headerEnd = request.find("\r\n\r\n");
      if (headerEnd == std::string::npos) {
        continue;
      }

      std::uint64_t contentLength = 0;
      bool contentLengthSeen = false;
      const std::size_t requestLineEnd = request.find("\r\n");
      if (requestLineEnd == std::string::npos || requestLineEnd > headerEnd) {
        return false;
      }

      std::size_t cursor = requestLineEnd + 2;
      while (cursor < headerEnd) {
        const std::size_t lineEnd = request.find("\r\n", cursor);
        if (lineEnd == std::string::npos || lineEnd > headerEnd) {
          return false;
        }
        const std::string line = request.substr(cursor, lineEnd - cursor);
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
          return false;
        }
        const std::string name = trimHttpWhitespace(line.substr(0, colon));
        const std::string value = trimHttpWhitespace(line.substr(colon + 1));

        if (asciiCaseEqual(name, "content-length")) {
          if (contentLengthSeen || !parseUint64Strict(value, contentLength)) {
            return false;
          }
          contentLengthSeen = true;
        } else if (asciiCaseEqual(name, "transfer-encoding")) {
          // Chunked requests are intentionally unsupported. Rejecting
          // them also prevents ambiguous request framing.
          return false;
        }
        cursor = lineEnd + 2;
      }

      const std::uint64_t prefixSize =
          static_cast<std::uint64_t>(headerEnd + 4);
      if (contentLength > NodeRpcServer::MAX_REQUEST_LEN ||
          prefixSize > NodeRpcServer::MAX_REQUEST_LEN - contentLength) {
        return false;
      }
      expectedSize = static_cast<std::size_t>(prefixSize + contentLength);
    }

    if (request.size() >= expectedSize.value()) {
      request.resize(expectedSize.value());
      return true;
    }
  }
  return false;
}

core::Transaction parseSignedTransactionSubmission(const std::string &body) {
  const serialization::KeyValueFileDocument fields =
      serialization::KeyValueFileCodec::parse(body, kRpcSubmitSchemaId);

  fields.requireOnlyFields(kRpcSubmitFields);

  const std::string transactionText = fields.requireField("transaction");

  return core::Transaction::deserialize(transactionText);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NodeRpcServer::NodeRpcServer(NodeRuntime &runtime, std::uint16_t port,
                             const std::string &bindAddr)
    : m_runtime(runtime), m_gossip(nullptr), m_port(port), m_bindAddr(bindAddr),
      m_running(false), m_serverFd(-1),
      m_rateLimiter(MAX_REQUESTS_PER_WINDOW, RATE_LIMIT_WINDOW_SECONDS) {}

NodeRpcServer::NodeRpcServer(NodeRuntime &runtime, p2p::GossipMesh &gossip,
                             std::uint16_t port, const std::string &bindAddr)
    : m_runtime(runtime), m_gossip(&gossip), m_port(port), m_bindAddr(bindAddr),
      m_running(false), m_serverFd(-1),
      m_rateLimiter(MAX_REQUESTS_PER_WINDOW, RATE_LIMIT_WINDOW_SECONDS) {}

NodeRpcServer::~NodeRpcServer() { stop(); }

std::uint16_t NodeRpcServer::port() const { return m_port; }

bool NodeRpcServer::isRunning() const { return m_running.load(); }

// ---------------------------------------------------------------------------
// Start / Stop
// ---------------------------------------------------------------------------

void NodeRpcServer::start() {
  if (m_running.load())
    return;

  const int serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (serverFd < 0) {
    throw std::runtime_error(std::string("NodeRpcServer: socket() failed: ") +
                             strerror(errno));
  }

  int yes = 1;
  ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&yes), sizeof(yes));

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(m_port);
  if (::inet_pton(AF_INET, m_bindAddr.c_str(), &addr.sin_addr) <= 0) {
    close_socket(serverFd);
    throw std::runtime_error("NodeRpcServer: invalid bind address: " +
                             m_bindAddr);
  }

  if (::bind(serverFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    close_socket(serverFd);
    throw std::runtime_error(std::string("NodeRpcServer: bind() failed: ") +
                             strerror(errno));
  }

  if (::listen(serverFd, 16) < 0) {
    close_socket(serverFd);
    throw std::runtime_error(std::string("NodeRpcServer: listen() failed: ") +
                             strerror(errno));
  }

  m_serverFd.store(serverFd);
  m_running.store(true);
  try {
    m_thread = std::thread([this] { runLoop(); });
  } catch (...) {
    m_running.store(false);
    const int fd = m_serverFd.exchange(-1);
    if (fd >= 0)
      close_socket(fd);
    throw;
  }
}

void NodeRpcServer::stop() {
  m_running.store(false);
  const int serverFd = m_serverFd.exchange(-1);
  if (serverFd >= 0) {
#ifdef _WIN32
    (void)::shutdown(static_cast<SOCKET>(serverFd), SD_BOTH);
#else
    (void)::shutdown(serverFd, SHUT_RDWR);
#endif
    close_socket(serverFd);
  }
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void NodeRpcServer::runLoop() {
  while (m_running.load()) {
    const int serverFd = m_serverFd.load();
    if (serverFd < 0)
      break;
    struct sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int clientFd =
        ::accept(serverFd, reinterpret_cast<sockaddr *>(&clientAddr), &addrLen);
    if (clientFd < 0) {
      if (!m_running.load())
        break;
      continue;
    }

    char ipBuffer[INET_ADDRSTRLEN] = {0};
    const char *ipStr =
        ::inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuffer, sizeof(ipBuffer));
    const std::string clientIp =
        ipStr != nullptr ? std::string(ipStr) : std::string("unknown");

    const std::int64_t now =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    if (!m_rateLimiter.shouldAllow(clientIp, now)) {
      const std::string resp =
          httpResponse(429, jsonError("Rate limit exceeded. Try again later."));
      (void)sendAll(clientFd, resp);
      close_socket(clientFd);
      continue;
    }

#ifdef _WIN32
    const DWORD timeoutMs = 2000;
    (void)::setsockopt(static_cast<SOCKET>(clientFd), SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char *>(&timeoutMs),
                       sizeof(timeoutMs));
    (void)::setsockopt(static_cast<SOCKET>(clientFd), SOL_SOCKET, SO_SNDTIMEO,
                       reinterpret_cast<const char *>(&timeoutMs),
                       sizeof(timeoutMs));
#else
    timeval timeout{};
    timeout.tv_sec = 2;
    (void)::setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                       sizeof(timeout));
    (void)::setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                       sizeof(timeout));
#endif

    try {
      handleClient(clientFd);
    } catch (...) {
      // A malformed or aborted client must not terminate the RPC loop.
    }
    close_socket(clientFd);
  }
}

// ---------------------------------------------------------------------------
// HTTP framing
// ---------------------------------------------------------------------------

void NodeRpcServer::handleClient(int clientFd) {
  std::string request;
  if (!receiveHttpRequest(clientFd, request))
    return;

  std::string method, path, body;
  if (!parseRequestLine(request, method, path, body)) {
    const std::string resp = httpResponse(400, jsonError("Bad request"));
    (void)sendAll(clientFd, resp);
    return;
  }

  std::pair<int, std::string> response;
  try {
    response = dispatch(method, path, body);
  } catch (const std::exception &e) {
    response = {500, jsonError(std::string("Internal RPC error: ") + e.what())};
  }

  const auto [statusCode, responseBody] = response;
  const std::string resp = httpResponse(statusCode, responseBody);
  (void)sendAll(clientFd, resp);
}

bool NodeRpcServer::parseRequestLine(const std::string &request,
                                     std::string &outMethod,
                                     std::string &outPath,
                                     std::string &outBody) {
  const std::size_t lineEnd = request.find("\r\n");
  if (lineEnd == std::string::npos)
    return false;

  const std::string firstLine = request.substr(0, lineEnd);
  std::istringstream iss(firstLine);
  std::string proto;
  if (!(iss >> outMethod >> outPath >> proto))
    return false;

  // Body follows the blank line.
  const std::size_t bodyPos = request.find("\r\n\r\n");
  if (bodyPos != std::string::npos) {
    outBody = request.substr(bodyPos + 4);
  }
  return true;
}

std::string NodeRpcServer::httpResponse(int statusCode,
                                        const std::string &body) {
  std::string statusText;
  switch (statusCode) {
  case 200:
    statusText = "OK";
    break;
  case 400:
    statusText = "Bad Request";
    break;
  case 404:
    statusText = "Not Found";
    break;
  case 405:
    statusText = "Method Not Allowed";
    break;
  case 500:
    statusText = "Internal Server Error";
    break;
  default:
    statusText = "Unknown";
    break;
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

std::string NodeRpcServer::jsonError(const std::string &message) {
  return "{\"error\":" + jsonString(message) + "}";
}

std::string NodeRpcServer::pathSegment(const std::string &path, int index) {
  if (index < 0)
    return "";

  std::size_t pos = 0;
  int seg = 0;
  while (pos <= path.size()) {
    std::size_t slash = path.find('/', pos);
    if (slash == std::string::npos)
      slash = path.size();
    if (seg == index) {
      return path.substr(pos, slash - pos);
    }
    if (slash == path.size())
      break;
    ++seg;
    pos = slash + 1;
  }
  return "";
}

// ---------------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------------

std::pair<int, std::string> NodeRpcServer::dispatch(const std::string &method,
                                                    const std::string &path,
                                                    const std::string &body) {
  // Normalize path: strip query string.
  const std::string cleanPath = path.substr(0, path.find('?'));

  // Segment 0 is empty (before first '/'), segment 1 is the route.
  const std::string route = pathSegment(cleanPath, 1);
  const std::string param = pathSegment(cleanPath, 2);

  if (route == "status" && method == "GET") {
    return {200, handleStatus()};
  }
  if (route == "block" && method == "GET") {
    if (param.empty())
      return {400, jsonError("Missing block height")};
    return {200, handleBlock(param)};
  }
  if (route == "tx" && method == "GET") {
    if (param.empty())
      return {400, jsonError("Missing tx id")};
    return {200, handleTx(param)};
  }
  if (route == "account" && method == "GET") {
    if (param.empty())
      return {400, jsonError("Missing address")};
    if (pathSegment(cleanPath, 3) == "proof") {
      return {200, handleAccountProof(param)};
    }
    return {200, handleAccount(param)};
  }
  if (route == "validators" && method == "GET") {
    return {200, handleValidators()};
  }
  if (route == "stake" && method == "GET") {
    if (param == "status") {
      const std::string validator = pathSegment(cleanPath, 3);
      if (validator.empty())
        return {400, jsonError("Missing validator address")};
      return {200, handleStakeStatus(validator)};
    }
    if (param == "positions") {
      return {200, handleStakePositions(pathSegment(cleanPath, 3))};
    }
    if (param == "position") {
      const std::string positionId = pathSegment(cleanPath, 3);
      if (positionId.empty())
        return {400, jsonError("Missing stake position id")};
      return {200, handleStakePosition(positionId)};
    }
    if (param == "pending-unbonding") {
      const std::string validator = pathSegment(cleanPath, 3);
      if (validator.empty())
        return {400, jsonError("Missing validator address")};
      return {200, handleStakePendingUnbonding(validator)};
    }
    if (param == "validator") {
      const std::string validator = pathSegment(cleanPath, 3);
      if (validator.empty())
        return {400, jsonError("Missing validator address")};
      return {200, handleStakeValidator(validator)};
    }
    if (param == "audit" || param.empty()) {
      return {200, handleStakeAudit()};
    }
    if (param == "deposit" || param == "topUp" || param == "top-up" ||
        param == "unlock" || param == "withdraw") {
      return {200, handleStakeMutationInfo(param)};
    }
    return {404, jsonError("Unknown stake route: " + param)};
  }
  if (route == "peers" && method == "GET") {
    return {200, handlePeers()};
  }
  if (route == "mempool" && method == "GET") {
    return {200, handleMempool()};
  }
  if (route == "governance" && method == "GET") {
    if (param == "status" || param.empty()) {
      return {200, handleGovernanceStatus()};
    }
    if (param == "proposals") {
      return {200, handleGovernanceProposals()};
    }
    const std::string proposalId = pathSegment(cleanPath, 3);
    if (proposalId.empty()) {
      return {400, jsonError("Missing governance proposal id")};
    }
    if (param == "proposal") {
      return {200, handleGovernanceProposal(proposalId)};
    }
    if (param == "votes") {
      return {200, handleGovernanceVotes(proposalId)};
    }
    if (param == "tally") {
      return {200, handleGovernanceTally(proposalId)};
    }
    if (param == "decision") {
      return {200, handleGovernanceDecision(proposalId)};
    }
    if (param == "execution") {
      return {200, handleGovernanceExecution(proposalId)};
    }
    return {404, jsonError("Unknown governance route: " + param)};
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
  const auto &chain = m_runtime.blockchain();
  const auto &mgr = m_runtime.consensusRoundManager();
  const auto &peers = m_runtime.peerManager();
  const auto &mempool = m_runtime.mempool();

  const std::uint64_t height = chain.empty() ? 0 : chain.latestBlock().index();
  const std::uint64_t round = mgr.currentState().round();
  const bool running = m_runtime.isRunning();
  const std::uint64_t finalizedHeight =
      m_runtime.finalizationRegistry().highestFinalizedHeight();
  const auto *encryptedTransport =
      m_gossip != nullptr ? dynamic_cast<const p2p::EncryptedPeerTransport *>(
                                &m_gossip->transport())
                          : nullptr;
  const std::size_t authenticatedSessionCount =
      encryptedTransport != nullptr ? encryptedTransport->sessionCount() : 0;
  const std::string latestHash =
      chain.empty() ? "" : chain.latestBlock().hash();

  std::ostringstream oss;
  oss << "{"
      << "\"height\":" << height << ",\"finalizedHeight\":" << finalizedHeight
      << ",\"latestHash\":" << jsonString(latestHash) << ",\"round\":" << round
      << ",\"peerCount\":" << peers.size()
      << ",\"authenticatedPeerCount\":" << authenticatedSessionCount
      << ",\"encryptedSessionCount\":" << authenticatedSessionCount
      << ",\"mempoolSize\":" << mempool.size()
      << ",\"running\":" << (running ? "true" : "false") << "}";
  return oss.str();
}

std::string NodeRpcServer::handleBlock(const std::string &heightStr) const {
  std::uint64_t height = 0;
  if (!parseUint64Strict(heightStr, height)) {
    return jsonError("Invalid height: " + heightStr);
  }

  const auto &blocks = m_runtime.blockchain().blocks();
  if (height >= blocks.size()) {
    return jsonError("Block not found at height " + heightStr);
  }

  const core::Block &block = blocks[static_cast<std::size_t>(height)];
  std::ostringstream oss;
  oss << "{"
      << "\"height\":" << block.index()
      << ",\"hash\":" << jsonString(block.hash())
      << ",\"previousHash\":" << jsonString(block.previousHash())
      << ",\"timestamp\":" << block.timestamp()
      << ",\"recordCount\":" << block.records().size() << "}";
  return oss.str();
}

std::string NodeRpcServer::handleTx(const std::string &txId) const {
  for (const auto &block : m_runtime.blockchain().blocks()) {
    for (const auto &record : block.records()) {
      if (record.id() == txId || record.sourceId() == txId) {
        std::ostringstream oss;
        oss << "{"
            << "\"id\":" << jsonString(record.id())
            << ",\"sourceId\":" << jsonString(record.sourceId()) << ",\"type\":"
            << jsonString(core::ledgerRecordTypeToString(record.type()))
            << ",\"blockHeight\":" << block.index()
            << ",\"timestamp\":" << record.timestamp() << "}";
        return oss.str();
      }
    }
  }
  return jsonError("Transaction not found: " + txId);
}

std::string NodeRpcServer::handleAccount(const std::string &address) const {
  const std::uint64_t minimumFeeRaw = m_runtime.effectiveMinimumFeeRawUnits();
  if (minimumFeeRaw >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return jsonError("Network minimum fee exceeds supported range.");
  }
  const core::AccountState account =
      m_runtime
          .cachedAccountStateAtTip(static_cast<std::int64_t>(minimumFeeRaw))
          .accountOrDefault(address);

  std::ostringstream oss;
  oss << "{"
      << "\"address\":" << jsonString(address)
      << ",\"balance\":" << account.balance().rawUnits()
      << ",\"nonce\":" << account.nonce() << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleAccountProof(const std::string &address) const {
  const std::uint64_t minimumFeeRaw = m_runtime.effectiveMinimumFeeRawUnits();
  if (minimumFeeRaw >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return jsonError("Network minimum fee exceeds supported range.");
  }
  const core::AccountStateView &view = m_runtime.cachedAccountStateAtTip(
      static_cast<std::int64_t>(minimumFeeRaw));

  if (!view.hasAccount(address)) {
    return jsonError("Address not present in current account state: " +
                     address);
  }

  const core::MerkleProof proof =
      core::StateRootCalculator::accountInclusionProof(view, address);
  if (!proof.isValid()) {
    return jsonError("Failed to build inclusion proof for address: " + address);
  }

  std::ostringstream oss;
  oss << "{"
      << "\"address\":" << jsonString(address)
      << ",\"accountStateRoot\":" << jsonString(proof.reconstructRoot())
      << ",\"leafHash\":" << jsonString(proof.leafHash()) << ",\"path\":[";
  const auto &steps = proof.steps();
  for (std::size_t i = 0; i < steps.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << "{\"siblingHash\":" << jsonString(steps[i].siblingHash)
        << ",\"siblingIsLeft\":" << (steps[i].siblingIsLeft ? "true" : "false")
        << "}";
  }
  oss << "]}";
  return oss.str();
}

std::string NodeRpcServer::handleValidators() const {
  const std::vector<std::string> addresses =
      m_runtime.validatorRegistry().activeValidatorAddresses();

  std::ostringstream oss;
  oss << "{\"validators\":[";
  for (std::size_t i = 0; i < addresses.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << jsonString(addresses[i]);
  }
  oss << "],\"count\":" << addresses.size() << ",\"totalConsensusWeight\":"
      << m_runtime.validatorRegistry().totalConsensusWeight()
      << ",\"validatorSetRoot\":"
      << jsonString(m_runtime.validatorRegistry().validatorSetRoot())
      << ",\"validatorDetails\":[";
  for (std::size_t i = 0; i < addresses.size(); ++i) {
    if (i > 0)
      oss << ",";
    const core::ValidatorRegistryEntry *entry =
        m_runtime.validatorRegistry().entryForAddress(addresses[i]);
    oss << "{"
        << "\"address\":" << jsonString(addresses[i]) << ",\"status\":"
        << jsonString(
               entry == nullptr
                   ? "UNKNOWN"
                   : core::validatorRegistrationStatusToString(entry->status()))
        << ",\"eligible\":"
        << (entry != nullptr && entry->eligibleForConsensus() ? "true"
                                                              : "false")
        << ",\"stakeRawUnits\":"
        << (entry == nullptr ? 0 : entry->stakeAmount())
        << ",\"consensusWeight\":"
        << m_runtime.validatorRegistry().consensusWeightFor(addresses[i])
        << "}";
  }
  oss << "]}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakeStatus(const std::string &validatorAddress) const {
  const auto account =
      m_runtime.stakingRegistry().accountOrDefault(validatorAddress);
  const core::ValidatorRegistryEntry *entry =
      m_runtime.validatorRegistry().entryForAddress(validatorAddress);
  std::ostringstream oss;
  oss << "{"
      << "\"validatorAddress\":" << jsonString(validatorAddress)
      << ",\"registered\":" << (entry == nullptr ? "false" : "true")
      << ",\"validatorStatus\":"
      << jsonString(
             entry == nullptr
                 ? "UNKNOWN"
                 : core::validatorRegistrationStatusToString(entry->status()))
      << ",\"bondedRawUnits\":" << account.bondedAmount().rawUnits()
      << ",\"activeRawUnits\":"
      << m_runtime.stakingRegistry().activeStakeFor(validatorAddress).rawUnits()
      << ",\"slashedRawUnits\":" << account.slashedAmount().rawUnits()
      << ",\"jailed\":" << (account.jailed() ? "true" : "false")
      << ",\"tombstoned\":" << (account.tombstoned() ? "true" : "false")
      << ",\"consensusWeight\":"
      << m_runtime.validatorRegistry().consensusWeightFor(validatorAddress)
      << ",\"positions\":[";
  bool first = true;
  for (const auto &position : m_runtime.stakingRegistry().positions()) {
    if (position.validatorAddress != validatorAddress)
      continue;
    if (!first)
      oss << ",";
    oss << jsonStakePosition(position);
    first = false;
  }
  oss << "]}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakePositions(const std::string &ownerAddress) const {
  const std::vector<StakePositionView> positions =
      ownerAddress.empty()
          ? m_runtime.stakingRegistry().positions()
          : m_runtime.stakingRegistry().positionsForOwner(ownerAddress);
  std::ostringstream oss;
  oss << "{\"ownerAddress\":" << jsonString(ownerAddress) << ",\"positions\":[";
  for (std::size_t i = 0; i < positions.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << jsonStakePosition(positions[i]);
  }
  oss << "],\"count\":" << positions.size() << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakePosition(const std::string &positionId) const {
  for (const auto &position : m_runtime.stakingRegistry().positions()) {
    if (position.positionId == positionId) {
      return jsonStakePosition(position);
    }
  }
  return jsonError("Stake position not found: " + positionId);
}

std::string NodeRpcServer::handleStakePendingUnbonding(
    const std::string &validatorAddress) const {
  std::ostringstream oss;
  oss << "{\"validatorAddress\":" << jsonString(validatorAddress)
      << ",\"pendingUnbonding\":[";
  bool first = true;
  std::int64_t total = 0;
  for (const auto &position : m_runtime.stakingRegistry().positions()) {
    if (position.validatorAddress != validatorAddress ||
        !position.pendingUnbondingAmount.isPositive()) {
      continue;
    }
    if (!first)
      oss << ",";
    oss << jsonStakePosition(position);
    total += position.pendingUnbondingAmount.rawUnits();
    first = false;
  }
  oss << "],\"totalPendingUnbondingRawUnits\":" << total << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakeValidator(const std::string &validatorAddress) const {
  return handleStakeStatus(validatorAddress);
}

std::string NodeRpcServer::handleStakeAudit() const {
  std::ostringstream oss;
  oss << "{"
      << "\"valid\":"
      << (m_runtime.stakingRegistry().isValid() ? "true" : "false")
      << ",\"accountCount\":" << m_runtime.stakingRegistry().accounts().size()
      << ",\"positionCount\":" << m_runtime.stakingRegistry().positions().size()
      << ",\"lifecycleRecordCount\":"
      << m_runtime.stakingRegistry().lifecycleRecords().size() << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakeMutationInfo(const std::string &operation) const {
  std::string transactionType = "STAKE_DEPOSIT";
  if (operation == "topUp" || operation == "top-up")
    transactionType = "STAKE_TOP_UP";
  if (operation == "unlock")
    transactionType = "STAKE_UNLOCK";
  if (operation == "withdraw")
    transactionType = "STAKE_WITHDRAW";
  std::ostringstream oss;
  oss << "{"
      << "\"operation\":" << jsonString(operation)
      << ",\"requiresSignedTransaction\":true"
      << ",\"transactionType\":" << jsonString(transactionType)
      << ",\"submitEndpoint\":\"/submit\""
      << "}";
  return oss.str();
}

std::string NodeRpcServer::handlePeers() const {
  const auto peers = m_runtime.peerManager().peers();

  std::ostringstream oss;
  oss << "{\"peers\":[";
  for (std::size_t i = 0; i < peers.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << "{"
        << "\"peerId\":" << jsonString(peers[i].peerId())
        << ",\"endpoint\":" << jsonString(peers[i].endpoint())
        << ",\"latestKnownHeight\":" << peers[i].latestKnownHeight() << "}";
  }
  oss << "],\"count\":" << peers.size() << "}";
  return oss.str();
}

std::string NodeRpcServer::handleMempool() const {
  const auto &pool = m_runtime.mempool();
  const auto pending = pool.transactionsForBlock(20);

  std::ostringstream oss;
  oss << "{\"size\":" << pool.size() << ",\"transactions\":[";
  for (std::size_t i = 0; i < pending.size(); ++i) {
    if (i > 0)
      oss << ",";
    const auto &tx = pending[i];
    oss << "{"
        << "\"id\":" << jsonString(tx.id())
        << ",\"from\":" << jsonString(tx.fromAddress())
        << ",\"to\":" << jsonString(tx.toAddress())
        << ",\"amount\":" << tx.amount().rawUnits()
        << ",\"fee\":" << tx.fee().rawUnits() << "}";
  }
  oss << "]}";
  return oss.str();
}

std::string NodeRpcServer::handleGovernanceStatus() const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  const std::uint64_t nextHeight = m_runtime.blockchain().size();
  std::ostringstream oss;
  oss << "{"
      << "\"activeProposalCount\":" << governance.activeProposalCount()
      << ",\"approvedProposalCount\":" << governance.approvedProposalCount()
      << ",\"executableProposalCount\":"
      << governance.executableProposalCount(nextHeight)
      << ",\"executedProposalCount\":" << governance.executedProposalCount()
      << ",\"effectiveMinimumFeeRawUnits\":"
      << m_runtime.effectiveMinimumFeeRawUnits() << "}";
  return oss.str();
}

std::string NodeRpcServer::handleGovernanceProposals() const {
  const std::vector<std::string> ids =
      m_runtime.governanceExecutor().proposalIds();
  std::ostringstream oss;
  oss << "{\"proposals\":[";
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << jsonString(ids[i]);
  }
  oss << "],\"count\":" << ids.size() << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleGovernanceProposal(const std::string &proposalId) const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  if (!governance.hasProposal(proposalId)) {
    return jsonError("Governance proposal not found: " + proposalId);
  }
  std::ostringstream oss;
  oss << "{"
      << "\"proposalId\":" << jsonString(proposalId)
      << ",\"detail\":" << jsonString(governance.proposalDetail(proposalId))
      << ",\"status\":"
      << jsonString(governanceProposalStatusToString(
             governance.proposalStatus(proposalId)))
      << ",\"votingStartHeight\":"
      << governance.proposalVotingStartHeight(proposalId)
      << ",\"votingEndHeight\":"
      << governance.proposalVotingEndHeight(proposalId) << ",\"approved\":"
      << (governance.proposalApproved(proposalId) ? "true" : "false")
      << ",\"executed\":"
      << (governance.hasBeenExecuted(proposalId) ? "true" : "false") << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleGovernanceVotes(const std::string &proposalId) const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  if (!governance.hasProposal(proposalId)) {
    return jsonError("Governance proposal not found: " + proposalId);
  }
  std::ostringstream oss;
  oss << "{"
      << "\"proposalId\":" << jsonString(proposalId)
      << ",\"votes\":" << jsonString(governance.proposalVotes(proposalId))
      << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleGovernanceTally(const std::string &proposalId) const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  if (!governance.hasProposal(proposalId)) {
    return jsonError("Governance proposal not found: " + proposalId);
  }
  const GovernanceTallySnapshot tally = governance.tallyForProposal(proposalId);
  std::ostringstream oss;
  oss << "{"
      << "\"proposalId\":" << jsonString(proposalId)
      << ",\"yesWeight\":" << tally.yesWeight()
      << ",\"noWeight\":" << tally.noWeight()
      << ",\"abstainWeight\":" << tally.abstainWeight()
      << ",\"participatingWeight\":" << tally.participatingWeight()
      << ",\"totalEligibleWeight\":" << tally.totalEligibleWeight()
      << ",\"quorumMet\":" << (tally.quorumMet() ? "true" : "false")
      << ",\"approvalThresholdMet\":"
      << (tally.approvalThresholdMet() ? "true" : "false")
      << ",\"approved\":" << (tally.approved() ? "true" : "false") << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleGovernanceDecision(const std::string &proposalId) const {
  return handleGovernanceProposal(proposalId);
}

std::string
NodeRpcServer::handleGovernanceExecution(const std::string &proposalId) const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  if (!governance.hasProposal(proposalId)) {
    return jsonError("Governance proposal not found: " + proposalId);
  }
  std::ostringstream oss;
  oss << "{"
      << "\"proposalId\":" << jsonString(proposalId) << ",\"status\":"
      << jsonString(governanceProposalStatusToString(
             governance.proposalStatus(proposalId)))
      << ",\"executed\":"
      << (governance.hasBeenExecuted(proposalId) ? "true" : "false")
      << ",\"detail\":"
      << jsonString(governance.proposalExecutionDetail(proposalId)) << "}";
  return oss.str();
}

std::string NodeRpcServer::handleSubmit(const std::string &body) {
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
        throw std::invalid_argument("Raw Transaction serialization is not "
                                    "self-contained for RPC submit; "
                                    "send a NODO_RPC_TRANSACTION_SUBMISSION_V1 "
                                    "envelope with public key material.");
      }

      throw std::invalid_argument("Unsupported submit payload schema.");
    }();
  } catch (const std::exception &e) {
    return jsonError(std::string("Invalid submit payload: ") + e.what());
  }

  const core::Transaction &tx = parsedTx.value();
  if (tx.id().empty() || !tx.hasSignatureBundle()) {
    return jsonError("Failed to deserialize signed transaction");
  }

  const std::int64_t now =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  const config::GenesisConfig &genesisConfig =
      m_runtime.config().genesisConfig();
  const config::NetworkParameters &networkParameters =
      genesisConfig.networkParameters();

  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(
          networkParameters.networkName());

  if (!cryptoContext.isValid()) {
    return jsonError("Runtime crypto context is invalid: " +
                     cryptoContext.rejectionReason());
  }

  if (networkParameters.minimumFeeRawUnits() >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return jsonError("Network minimum fee exceeds supported Amount range.");
  }

  const core::AccountStateView accountState =
      RuntimeAccountStateBuilder::accountStateViewAtTip(
          genesisConfig, m_runtime.blockchain(),
          static_cast<std::int64_t>(networkParameters.minimumFeeRawUnits()));

  const TransactionAdmissionContext admissionContext(
      accountState, m_runtime.mempool(), m_runtime.stakingRegistry(),
      m_runtime.validatorRegistry(), m_runtime.governanceExecutor(),
      m_runtime.blockchain().size());

  const TransactionAdmissionResult validation =
      TransactionAdmissionValidator::validateNetworkSubmission(
          tx, networkParameters, accountState, m_runtime.mempool(),
          cryptoContext.policy(), crypto::SecurityContext::USER_TRANSACTION,
          cryptoContext.userSignatureProvider(),
          m_runtime.effectiveMinimumFeeRawUnits(), &admissionContext);

  if (!validation.accepted()) {
    std::cout << "[DEBUG] NodeRpcServer handleSubmit validation failed: "
              << validation.reason() << std::endl;
    return jsonError("Transaction rejected: " +
                     transactionAdmissionStatusToString(validation.status()) +
                     ": " + validation.reason());
  }

  std::string gossipPayload;
  if (m_gossip != nullptr) {
    const crypto::SignatureBundle &signatures = tx.signatureBundle();
    if (signatures.signatures().empty()) {
      return jsonError("Transaction signature bundle is empty.");
    }
    gossipPayload = PersistentMempoolStore::serializeForGossip(
        tx, signatures.signatures().front().publicKey(), now);
    if (gossipPayload.empty()) {
      return jsonError("Unable to build canonical transaction gossip payload.");
    }
  }

  const mempool::MempoolAdmissionResult result =
      m_runtime.mutableMempool().admitTransaction(
          tx, cryptoContext.policy(), crypto::SecurityContext::USER_TRANSACTION,
          now);

  // Broadcast the same self-contained schema consumed by NodeDaemon peers.
  if (m_gossip != nullptr && result.success()) {
    m_gossip->broadcast(p2p::NetworkMessageType::TRANSACTION_GOSSIP,
                        gossipPayload, now);
  } else if (!result.success()) {
    std::cout << "[DEBUG] NodeRpcServer handleSubmit admitTransaction failed: "
              << result.reason() << std::endl;
  }

  std::ostringstream oss;
  oss << "{"
      << "\"status\":"
      << jsonString(mempool::mempoolAdmissionStatusToString(result.status()))
      << ",\"txId\":" << jsonString(result.transactionId())
      << ",\"reason\":" << jsonString(result.reason()) << "}";
  return oss.str();
}

} // namespace nodo::node
