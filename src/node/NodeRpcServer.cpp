#include "utils/Logger.hpp"
#include "node/NodeRpcServer.hpp"

#include "node/HealthCheckService.hpp"
#include "node/NodeMetrics.hpp"
#include "node/PrometheusExporter.hpp"

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
#include "node/LightClientService.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "node/WebSocketFrameCodec.hpp"
#include "p2p/EncryptedPeerTransport.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "utils/Amount.hpp"
#include "utils/JsonText.hpp"

#include <asio.hpp>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace nodo::node {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

using asio::ip::tcp;

// WebSocket subscribers only send control frames; buffering more than this
// from one is treated as abuse.
constexpr std::size_t kMaxWebSocketInboundBytes = 8192;
// A subscriber that stops reading is dropped instead of letting queued
// events grow without bound.
constexpr std::size_t kMaxQueuedWrites = 256;
constexpr std::size_t kEventsPerPoll = 100;
// After a response, unread request bytes are drained for up to this long
// before closing, so the client receives the response instead of a reset.
constexpr std::chrono::milliseconds kLingerTimeout{1000};
// Accept errors are usually descriptor exhaustion; retry after a pause
// instead of spinning.
constexpr std::chrono::milliseconds kAcceptRetryDelay{100};
// RFC 6455: Sec-WebSocket-Key is 16 random bytes, base64 encoded.
constexpr std::size_t kWebSocketKeyLength = 24;

using utils::jsonString;

std::int64_t nowUnixSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
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

std::string websocketAcceptKey(const std::string &clientKey) {
  static constexpr const char *kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  const std::string material = clientKey + kGuid;
  unsigned char digest[SHA_DIGEST_LENGTH] = {0};
  SHA1(reinterpret_cast<const unsigned char *>(material.data()),
       material.size(), digest);

  std::string encoded;
  encoded.resize(4 * ((SHA_DIGEST_LENGTH + 2) / 3));
  const int written =
      EVP_EncodeBlock(reinterpret_cast<unsigned char *>(&encoded[0]), digest,
                      SHA_DIGEST_LENGTH);
  encoded.resize(static_cast<std::size_t>(written));
  return encoded;
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

// RFC 9110 token characters, used for methods and header names.
bool isHttpToken(const std::string &value) {
  if (value.empty()) {
    return false;
  }
  for (const char c : value) {
    const bool alphanumeric = (c >= 'a' && c <= 'z') ||
                              (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    if (!alphanumeric && std::string("!#$%&'*+-.^_`|~").find(c) ==
                             std::string::npos) {
      return false;
    }
  }
  return true;
}

struct HttpRequest {
  std::string method;
  std::string target;
  std::string body;
  std::string upgrade;
  std::string websocketKey;
  std::size_t consumedBytes = 0; // request line through the end of the body
};

enum class HttpParseStatus {
  INCOMPLETE,
  COMPLETE,
  BAD_REQUEST,
  PAYLOAD_TOO_LARGE,
  HEADERS_TOO_LARGE,
  NOT_IMPLEMENTED
};

// Frames one HTTP/1.x request from the bytes received so far. A body is
// framed only by Content-Length; chunked transfer coding, duplicate
// Content-Length headers, folded header lines and whitespace before a colon
// are all refused, so the request cannot be read two different ways.
HttpParseStatus parseHttpRequest(const std::string &data, HttpRequest &out) {
  const std::size_t headerEnd = data.find("\r\n\r\n");
  if (headerEnd == std::string::npos) {
    return data.size() > NodeRpcServer::MAX_HEADER_LEN
               ? HttpParseStatus::HEADERS_TOO_LARGE
               : HttpParseStatus::INCOMPLETE;
  }
  const std::size_t bodyStart = headerEnd + 4;
  if (bodyStart > NodeRpcServer::MAX_HEADER_LEN) {
    return HttpParseStatus::HEADERS_TOO_LARGE;
  }

  HttpRequest request;
  const std::size_t requestLineEnd = data.find("\r\n");
  const std::string requestLine = data.substr(0, requestLineEnd);
  const std::size_t firstSpace = requestLine.find(' ');
  const std::size_t secondSpace = firstSpace == std::string::npos
                                      ? std::string::npos
                                      : requestLine.find(' ', firstSpace + 1);
  if (secondSpace == std::string::npos ||
      requestLine.find(' ', secondSpace + 1) != std::string::npos) {
    return HttpParseStatus::BAD_REQUEST;
  }
  request.method = requestLine.substr(0, firstSpace);
  request.target =
      requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
  const std::string version = requestLine.substr(secondSpace + 1);
  if (!isHttpToken(request.method) || request.target.empty() ||
      request.target.front() != '/' ||
      (version != "HTTP/1.0" && version != "HTTP/1.1")) {
    return HttpParseStatus::BAD_REQUEST;
  }
  for (const char c : request.target) {
    const auto byte = static_cast<unsigned char>(c);
    if (byte <= 0x20 || byte >= 0x7f) {
      return HttpParseStatus::BAD_REQUEST;
    }
  }

  std::optional<std::uint64_t> contentLength;
  std::size_t cursor = requestLineEnd + 2;
  while (cursor < headerEnd) {
    const std::size_t lineEnd = data.find("\r\n", cursor);
    const std::string line = data.substr(cursor, lineEnd - cursor);
    cursor = lineEnd + 2;

    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
      return HttpParseStatus::BAD_REQUEST;
    }
    const std::string name = line.substr(0, colon);
    if (!isHttpToken(name)) {
      return HttpParseStatus::BAD_REQUEST;
    }
    const std::string value = trimHttpWhitespace(line.substr(colon + 1));

    if (asciiCaseEqual(name, "content-length")) {
      std::uint64_t parsed = 0;
      if (contentLength.has_value() || !parseUint64Strict(value, parsed)) {
        return HttpParseStatus::BAD_REQUEST;
      }
      contentLength = parsed;
    } else if (asciiCaseEqual(name, "transfer-encoding")) {
      return HttpParseStatus::NOT_IMPLEMENTED;
    } else if (asciiCaseEqual(name, "upgrade")) {
      request.upgrade = value;
    } else if (asciiCaseEqual(name, "sec-websocket-key")) {
      request.websocketKey = value;
    }
  }

  const std::uint64_t bodyLength = contentLength.value_or(0);
  if (bodyLength > NodeRpcServer::MAX_REQUEST_LEN - bodyStart) {
    return HttpParseStatus::PAYLOAD_TOO_LARGE;
  }
  if (data.size() - bodyStart < bodyLength) {
    return HttpParseStatus::INCOMPLETE;
  }

  request.body = data.substr(bodyStart, static_cast<std::size_t>(bodyLength));
  request.consumedBytes = bodyStart + static_cast<std::size_t>(bodyLength);
  out = std::move(request);
  return HttpParseStatus::COMPLETE;
}

// Reads ?after=N from a /events target; 0 when absent or malformed.
std::uint64_t afterSequenceFromTarget(const std::string &target) {
  std::uint64_t afterSequence = 0;
  const std::size_t query = target.find('?');
  if (query == std::string::npos) {
    return afterSequence;
  }
  const std::string q = target.substr(query + 1);
  const std::string marker = "after=";
  std::size_t pos = 0;
  while (pos < q.size()) {
    std::size_t end = q.find('&', pos);
    if (end == std::string::npos) {
      end = q.size();
    }
    if (q.compare(pos, marker.size(), marker) == 0) {
      (void)parseUint64Strict(
          q.substr(pos + marker.size(), end - (pos + marker.size())),
          afterSequence);
    }
    pos = end + 1;
  }
  return afterSequence;
}

tcp::endpoint resolveBindEndpoint(asio::io_context &io,
                                  const std::string &host,
                                  std::uint16_t port) {
  asio::error_code error;
  const asio::ip::address address = asio::ip::make_address(host, error);
  if (!error) {
    return tcp::endpoint(address, port);
  }

  tcp::resolver resolver(io);
  const tcp::resolver::results_type results = resolver.resolve(
      host, std::to_string(port), tcp::resolver::passive, error);
  if (error || results.empty()) {
    throw std::runtime_error("NodeRpcServer: invalid bind address: " + host);
  }
  return results.begin()->endpoint();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Transport
// ---------------------------------------------------------------------------

struct NodeRpcServer::Transport {
  asio::io_context io;
  tcp::acceptor acceptor{io};
  asio::steady_timer acceptRetry{io};
  std::vector<std::thread> workers;
};

// One accepted connection. Every handler runs on the socket's strand, so a
// session's state is never touched by two threads at once. The session lives
// as long as one of its asynchronous operations holds a reference to it.
class NodeRpcServer::Session
    : public std::enable_shared_from_this<NodeRpcServer::Session> {
public:
  Session(NodeRpcServer &server, tcp::socket socket)
      : m_server(server), m_socket(std::move(socket)),
        m_deadline(m_socket.get_executor()),
        m_pollTimer(m_socket.get_executor()) {
    ++m_server.m_activeConnections;
  }

  ~Session() {
    if (m_isWebSocket) {
      --m_server.m_activeWebSockets;
    }
    --m_server.m_activeConnections;
  }

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  // Reads one request and answers it.
  void start() {
    asio::dispatch(m_socket.get_executor(), [self = shared_from_this()] {
      self->armDeadline(self->m_server.m_limits.requestTimeout);
      self->readRequest();
    });
  }

  // Answers without serving the request (rate limiting).
  void reject(int statusCode, std::string body) {
    asio::dispatch(m_socket.get_executor(),
                   [self = shared_from_this(), statusCode,
                    body = std::move(body)] {
                     self->respondAndClose(statusCode, body);
                   });
  }

private:
  NodeRpcServer &m_server;
  tcp::socket m_socket;
  asio::steady_timer m_deadline;
  asio::steady_timer m_pollTimer;
  std::array<char, 4096> m_chunk{};
  std::string m_buffer;
  std::deque<std::string> m_writeQueue;
  bool m_writing = false;
  bool m_closeAfterWrites = false;
  bool m_closed = false;
  bool m_isWebSocket = false;
  std::uint64_t m_afterSequence = 0;

  void armDeadline(std::chrono::milliseconds timeout) {
    m_deadline.expires_after(timeout);
    m_deadline.async_wait(
        [self = shared_from_this()](const asio::error_code &error) {
          if (!error) {
            self->close();
          }
        });
  }

  void readRequest() {
    m_socket.async_read_some(
        asio::buffer(m_chunk),
        [self = shared_from_this()](const asio::error_code &error,
                                    std::size_t received) {
          self->onRequestBytes(error, received);
        });
  }

  void onRequestBytes(const asio::error_code &error, std::size_t received) {
    if (error || m_closed) {
      close();
      return;
    }
    m_buffer.append(m_chunk.data(), received);

    HttpRequest request;
    switch (parseHttpRequest(m_buffer, request)) {
    case HttpParseStatus::INCOMPLETE:
      readRequest();
      return;
    case HttpParseStatus::COMPLETE:
      m_buffer.erase(0, request.consumedBytes);
      serve(request);
      return;
    case HttpParseStatus::BAD_REQUEST:
      respondAndClose(400, jsonError("Bad request"));
      return;
    case HttpParseStatus::PAYLOAD_TOO_LARGE:
      respondAndClose(413,
                      jsonError("Request too large; the limit is " +
                                std::to_string(MAX_REQUEST_LEN) + " bytes."));
      return;
    case HttpParseStatus::HEADERS_TOO_LARGE:
      respondAndClose(431, jsonError("Request headers too large."));
      return;
    case HttpParseStatus::NOT_IMPLEMENTED:
      respondAndClose(
          501, jsonError(
                   "Transfer-Encoding is not supported; send Content-Length."));
      return;
    }
  }

  void serve(const HttpRequest &request) {
    const std::string cleanPath =
        request.target.substr(0, request.target.find('?'));
    if ((cleanPath == "/events" || cleanPath == "/events/ws") &&
        asciiCaseEqual(request.upgrade, "websocket")) {
      upgradeToWebSocket(request);
      return;
    }

    HttpDispatchResponse response(200, "{}");
    try {
      response =
          m_server.dispatch(request.method, request.target, request.body);
    } catch (const std::exception &e) {
      response = HttpDispatchResponse(
          500, jsonError(std::string("Internal RPC error: ") + e.what()));
    } catch (...) {
      response = HttpDispatchResponse(500, jsonError("Internal RPC error."));
    }
    respondAndClose(response.statusCode, response.body, response.contentType);
  }

  void respondAndClose(int statusCode, const std::string &body,
                       const std::string &contentType = "application/json") {
    armDeadline(m_server.m_limits.writeTimeout);
    m_closeAfterWrites = true;
    enqueue(httpResponse(statusCode, body, contentType));
  }

  void upgradeToWebSocket(const HttpRequest &request) {
    if (request.websocketKey.size() != kWebSocketKeyLength) {
      respondAndClose(400, jsonError("Missing or invalid Sec-WebSocket-Key."));
      return;
    }
    if (m_server.m_activeWebSockets.fetch_add(1) >=
        m_server.m_limits.maxWebSocketSessions) {
      --m_server.m_activeWebSockets;
      respondAndClose(503, jsonError("Too many WebSocket subscribers."));
      return;
    }
    m_isWebSocket = true;
    m_deadline.cancel(); // an event stream has no request deadline

    enqueue("HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " +
            websocketAcceptKey(request.websocketKey) + "\r\n\r\n");

    const auto hello = m_server.m_eventBus->publish(
        NodeEventType::RPC_SUBSCRIPTION, 0, "",
        "{\"transport\":\"websocket\",\"endpoint\":\"/events\"}",
        nowUnixSeconds());
    enqueue(WebSocketFrameCodec::textFrame(hello.serializeJson()));
    m_afterSequence =
        std::max(afterSequenceFromTarget(request.target), hello.sequence());

    pollEvents();
    processWebSocketInput(); // frames may have arrived with the handshake
  }

  void pollEvents() {
    if (m_closed || m_closeAfterWrites) {
      return;
    }
    for (const ChainEvent &event :
         m_server.m_eventBus->recent(m_afterSequence, kEventsPerPoll)) {
      enqueue(WebSocketFrameCodec::textFrame(event.serializeJson()));
      m_afterSequence = std::max(m_afterSequence, event.sequence());
    }
    m_pollTimer.expires_after(m_server.m_limits.eventPollInterval);
    m_pollTimer.async_wait(
        [self = shared_from_this()](const asio::error_code &error) {
          if (!error) {
            self->pollEvents();
          }
        });
  }

  void processWebSocketInput() {
    while (!m_closed && !m_closeAfterWrites) {
      const std::optional<WebSocketFrame> frame =
          WebSocketFrameCodec::decodeClientFrame(m_buffer);
      if (!frame.has_value()) {
        break;
      }
      m_buffer.erase(0, frame->consumedBytes);
      // RFC 6455 section 5.1: a server closes on an unmasked client frame.
      if (!frame->masked || frame->opcode == WebSocketOpcode::CLOSE) {
        m_closeAfterWrites = true;
        enqueue(WebSocketFrameCodec::closeFrame());
        return;
      }
      if (frame->opcode == WebSocketOpcode::PING) {
        enqueue(WebSocketFrameCodec::pongFrame(frame->payload));
      }
    }
    if (m_closed || m_closeAfterWrites) {
      return;
    }
    if (m_buffer.size() > kMaxWebSocketInboundBytes) {
      close();
      return;
    }

    m_socket.async_read_some(
        asio::buffer(m_chunk),
        [self = shared_from_this()](const asio::error_code &error,
                                    std::size_t received) {
          if (error) {
            self->close();
            return;
          }
          self->m_buffer.append(self->m_chunk.data(), received);
          self->processWebSocketInput();
        });
  }

  void enqueue(std::string data) {
    if (m_closed) {
      return;
    }
    if (m_writeQueue.size() >= kMaxQueuedWrites) {
      close();
      return;
    }
    m_writeQueue.push_back(std::move(data));
    if (!m_writing) {
      m_writing = true;
      writeNext();
    }
  }

  void writeNext() {
    asio::async_write(
        m_socket, asio::buffer(m_writeQueue.front()),
        [self = shared_from_this()](const asio::error_code &error,
                                    std::size_t) {
          if (error) {
            self->close();
            return;
          }
          self->m_writeQueue.pop_front();
          if (!self->m_writeQueue.empty()) {
            self->writeNext();
            return;
          }
          self->m_writing = false;
          if (self->m_closeAfterWrites) {
            self->finish();
          }
        });
  }

  // Ends the connection once everything queued has been written.
  void finish() {
    if (m_isWebSocket) {
      close();
      return;
    }
    // Lingering close: stop sending, then discard whatever the client still
    // sends, so closing never resets the connection before the client has
    // read the response.
    asio::error_code ignored;
    m_socket.shutdown(tcp::socket::shutdown_send, ignored);
    armDeadline(kLingerTimeout);
    drain();
  }

  void drain() {
    m_socket.async_read_some(
        asio::buffer(m_chunk),
        [self = shared_from_this()](const asio::error_code &error,
                                    std::size_t) {
          if (error) {
            self->close();
            return;
          }
          self->drain();
        });
  }

  void close() {
    if (m_closed) {
      return;
    }
    m_closed = true;
    m_deadline.cancel();
    m_pollTimer.cancel();
    asio::error_code ignored;
    m_socket.shutdown(tcp::socket::shutdown_both, ignored);
    m_socket.close(ignored);
  }
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NodeRpcServer::HttpDispatchResponse::HttpDispatchResponse(
    int code, std::string responseBody, std::string responseContentType)
    : statusCode(code), body(std::move(responseBody)),
      contentType(std::move(responseContentType)) {}

NodeRpcServer::NodeRpcServer(NodeRuntime &runtime, std::mutex &runtimeMutex,
                             NodeEventBus *eventBus, std::uint16_t port,
                             const std::string &bindAddr)
    : m_runtime(runtime), m_runtimeMutex(runtimeMutex), m_gossip(nullptr),
      m_configuredPort(port), m_port(port), m_bindAddr(bindAddr),
      m_running(false), m_limits(),
      m_transport(), m_activeConnections(0), m_activeWebSockets(0),
      m_ownedEventBus(),
      m_eventBus(eventBus == nullptr ? &m_ownedEventBus : eventBus),
      m_rateLimiter(MAX_REQUESTS_PER_WINDOW, RATE_LIMIT_WINDOW_SECONDS),
      m_syncHealth(nullptr) {
  registerJsonRpcMethods();
}

NodeRpcServer::NodeRpcServer(NodeRuntime &runtime, std::mutex &runtimeMutex,
                             p2p::GossipMesh &gossip, NodeEventBus *eventBus,
                             std::uint16_t port, const std::string &bindAddr)
    : m_runtime(runtime), m_runtimeMutex(runtimeMutex), m_gossip(&gossip),
      m_configuredPort(port), m_port(port), m_bindAddr(bindAddr),
      m_running(false), m_limits(),
      m_transport(), m_activeConnections(0), m_activeWebSockets(0),
      m_ownedEventBus(),
      m_eventBus(eventBus == nullptr ? &m_ownedEventBus : eventBus),
      m_rateLimiter(MAX_REQUESTS_PER_WINDOW, RATE_LIMIT_WINDOW_SECONDS),
      m_syncHealth(nullptr) {
  registerJsonRpcMethods();
}

NodeRpcServer::~NodeRpcServer() { stop(); }

std::uint16_t NodeRpcServer::port() const { return m_port.load(); }

bool NodeRpcServer::isRunning() const { return m_running.load(); }

void NodeRpcServer::attachSyncHealth(const SyncHealth *syncHealth) {
  m_syncHealth = syncHealth;
}

void NodeRpcServer::attachDataDirectory(
    const NodeDataDirectoryConfig *directory) {
  m_dataDirectory = directory;
}

void NodeRpcServer::setLimits(const Limits &limits) {
  if (m_transport != nullptr) {
    throw std::logic_error("NodeRpcServer limits cannot change while running.");
  }
  if (limits.workerThreads == 0 || limits.maxConnections == 0 ||
      limits.maxWebSocketSessions > limits.maxConnections ||
      limits.requestTimeout.count() <= 0 || limits.writeTimeout.count() <= 0 ||
      limits.eventPollInterval.count() <= 0 ||
      limits.maxRequestsPerWindow == 0 || limits.rateLimitWindowSeconds == 0) {
    throw std::invalid_argument("NodeRpcServer limits are invalid.");
  }
  m_limits = limits;
  m_rateLimiter = p2p::PeerRateLimiter(limits.maxRequestsPerWindow,
                                       limits.rateLimitWindowSeconds);
}

const NodeRpcServer::Limits &NodeRpcServer::limits() const { return m_limits; }

// ---------------------------------------------------------------------------
// Start / Stop
// ---------------------------------------------------------------------------

void NodeRpcServer::start() {
  if (m_transport != nullptr) {
    return;
  }

  auto transport = std::make_unique<Transport>();
  const tcp::endpoint endpoint =
      resolveBindEndpoint(transport->io, m_bindAddr, m_configuredPort);

  asio::error_code error;
  transport->acceptor.open(endpoint.protocol(), error);
  if (error) {
    throw std::runtime_error("NodeRpcServer: cannot open socket: " +
                             error.message());
  }
#ifndef _WIN32
  // On Windows SO_REUSEADDR lets another process bind the same port, so it
  // is only set where it means "reuse a port in TIME_WAIT".
  transport->acceptor.set_option(tcp::acceptor::reuse_address(true), error);
#endif
  transport->acceptor.bind(endpoint, error);
  if (error) {
    throw std::runtime_error("NodeRpcServer: bind() failed on " + m_bindAddr +
                             ":" + std::to_string(endpoint.port()) + ": " +
                             error.message());
  }
  transport->acceptor.listen(asio::socket_base::max_listen_connections, error);
  if (error) {
    throw std::runtime_error("NodeRpcServer: listen() failed: " +
                             error.message());
  }

  m_port.store(transport->acceptor.local_endpoint().port());
  m_transport = std::move(transport);
  m_running.store(true);
  acceptNext();

  try {
    for (std::size_t index = 0; index < m_limits.workerThreads; ++index) {
      m_transport->workers.emplace_back([io = &m_transport->io] {
        for (;;) {
          try {
            io->run();
            return;
          } catch (...) {
            // A throwing handler must not stop the server; keep serving.
          }
        }
      });
    }
  } catch (...) {
    stop();
    throw;
  }
}

void NodeRpcServer::stop() {
  m_running.store(false);
  if (m_transport == nullptr) {
    return;
  }
  m_transport->io.stop();
  for (std::thread &worker : m_transport->workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  // Destroying the io_context destroys every pending handler, and with them
  // the sessions they own, which closes their sockets.
  m_transport.reset();
}

void NodeRpcServer::acceptNext() {
  Transport &transport = *m_transport;
  transport.acceptor.async_accept(
      asio::make_strand(transport.io),
      [this](const asio::error_code &error, tcp::socket socket) {
        if (!m_running.load() || error == asio::error::operation_aborted) {
          return;
        }
        if (error) {
          m_transport->acceptRetry.expires_after(kAcceptRetryDelay);
          m_transport->acceptRetry.async_wait(
              [this](const asio::error_code &waitError) {
                if (!waitError && m_running.load()) {
                  acceptNext();
                }
              });
          return;
        }

        // Only this handler runs at a time (one accept is ever pending), so
        // the connection cap and the rate limiter need no further locking.
        if (m_activeConnections.load() >= m_limits.maxConnections) {
          asio::error_code ignored;
          socket.close(ignored);
        } else {
          asio::error_code endpointError;
          const tcp::endpoint remote = socket.remote_endpoint(endpointError);
          const std::string clientIp = endpointError
                                           ? std::string("unknown")
                                           : remote.address().to_string();
          const auto session = std::make_shared<Session>(*this, std::move(socket));
          if (m_rateLimiter.shouldAllow(clientIp, nowUnixSeconds())) {
            session->start();
          } else {
            session->reject(429,
                            jsonError("Rate limit exceeded. Try again later."));
          }
        }
        acceptNext();
      });
}

std::string NodeRpcServer::httpResponse(int statusCode, const std::string &body,
                                        const std::string &contentType) {
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
  case 413:
    statusText = "Content Too Large";
    break;
  case 429:
    statusText = "Too Many Requests";
    break;
  case 431:
    statusText = "Request Header Fields Too Large";
    break;
  case 500:
    statusText = "Internal Server Error";
    break;
  case 501:
    statusText = "Not Implemented";
    break;
  case 503:
    statusText = "Service Unavailable";
    break;
  default:
    statusText = "Unknown";
    break;
  }

  std::ostringstream oss;
  oss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
      << "Content-Type: " << contentType << "\r\n"
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
// JSON-RPC runtime binding
// ---------------------------------------------------------------------------

void NodeRpcServer::registerJsonRpcMethods() {
  m_jsonRpcDispatcher.registerStandardMethods(
      [this](std::uint64_t height) {
        return handleBlock(std::to_string(height));
      },
      [this](const std::string &hash) { return handleBlockByHash(hash); },
      [this](const std::string &txId) { return handleTx(txId); },
      [this](const std::string &address) { return handleAccount(address); },
      [this](const std::string &txEnvelope) {
        return handleSubmit(txEnvelope);
      },
      [this]() { return handleMempool(); },
      [this](const std::string &urgency) { return handleEstimateFee(urgency); },
      [this]() { return handleChainInfo(); },
      [this]() { return handleValidators(); });

  m_jsonRpcDispatcher.registerGovernanceMethods(
      [this]() { return handleGovernanceProposals(); },
      [this](const std::string &proposalId) {
        return handleGovernanceProposal(proposalId);
      },
      [this](const std::string &proposalId) {
        return handleGovernanceVotes(proposalId);
      },
      [this](const std::string &proposalId) {
        return handleGovernanceTally(proposalId);
      },
      [this](const std::string &proposalId) {
        return handleGovernanceDecision(proposalId);
      },
      [this](const std::string &proposalId) {
        return handleGovernanceExecution(proposalId);
      },
      [this](const std::string &txEnvelope) {
        return handleSubmit(txEnvelope);
      },
      [this](const std::string &txEnvelope) {
        return handleSubmit(txEnvelope);
      },
      [this]() { return handleGovernanceStatus(); });

  m_jsonRpcDispatcher.registerStakingMethods(
      [this](const std::string &validator) {
        return handleStakeStatus(validator);
      },
      [this](const std::string &owner) { return handleStakePositions(owner); },
      [this](const std::string &positionId) {
        return handleStakePosition(positionId);
      },
      [this](const std::string &txEnvelope) {
        return handleSubmit(txEnvelope);
      },
      [this](const std::string &validator) {
        return handleStakePendingUnbonding(validator);
      },
      [this](const std::string &validator) {
        return handleStakeValidator(validator);
      },
      [this]() { return handleStakeAudit(); });

  // Official aliases and discovery helpers. These methods keep the public API
  // protocol-shaped while avoiding a second, divergent implementation path.
  m_jsonRpcDispatcher.registerHandler(
      "rpc_methods", [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handleJsonRpcMethods());
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getStatus", [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handleStatus());
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getHealth", [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handleHealth());
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getMetrics", [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handleMetrics());
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getPrometheusMetrics",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id,
                                        jsonString(handlePrometheusMetrics()));
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getPeers", [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handlePeers());
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getAccountProof",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        const std::string address =
            JsonRpcDispatcher::extractParam(req.params, "address");
        if (address.empty()) {
          return JsonRpcResponse::makeError(
              req.id, JsonRpcError::INVALID_PARAMS, "Missing param: address");
        }
        return JsonRpcResponse::success(req.id, handleAccountProof(address));
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_sendRawTransaction",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        const std::string tx =
            JsonRpcDispatcher::extractParam(req.params, "tx");
        if (tx.empty()) {
          return JsonRpcResponse::makeError(
              req.id, JsonRpcError::INVALID_PARAMS, "Missing param: tx");
        }
        return JsonRpcResponse::success(req.id, handleSubmit(tx));
      });

  m_jsonRpcDispatcher.registerHandler(
      "governance_submitExecution",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        const std::string tx =
            JsonRpcDispatcher::extractParam(req.params, "tx");
        if (tx.empty()) {
          return JsonRpcResponse::makeError(
              req.id, JsonRpcError::INVALID_PARAMS, "Missing param: tx");
        }
        return JsonRpcResponse::success(req.id, handleSubmit(tx));
      });

  m_jsonRpcDispatcher.registerHandler(
      "light_getCheckpoint",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handleLightCheckpoint());
      });

  m_jsonRpcDispatcher.registerHandler(
      "light_getHeaders", [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        const std::string fromHeight =
            JsonRpcDispatcher::extractParam(req.params, "fromHeight");
        const std::string maxHeaders =
            JsonRpcDispatcher::extractParam(req.params, "maxHeaders");
        return JsonRpcResponse::success(
            req.id, handleLightHeaders(fromHeight, maxHeaders));
      });

  m_jsonRpcDispatcher.registerHandler(
      "light_getAccountProof",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        const std::string address =
            JsonRpcDispatcher::extractParam(req.params, "address");
        if (address.empty()) {
          return JsonRpcResponse::makeError(
              req.id, JsonRpcError::INVALID_PARAMS, "Missing param: address");
        }
        return JsonRpcResponse::success(req.id,
                                        handleLightAccountProof(address));
      });

  m_jsonRpcDispatcher.registerHandler(
      "light_getTransactionProof",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        const std::string txId =
            JsonRpcDispatcher::extractParam(req.params, "transactionId");
        if (txId.empty()) {
          return JsonRpcResponse::makeError(req.id,
                                            JsonRpcError::INVALID_PARAMS,
                                            "Missing param: transactionId");
        }
        return JsonRpcResponse::success(req.id,
                                        handleLightTransactionProof(txId));
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getStorageStatus",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handleStorageStatus());
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getCheckpoint", [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(
            req.id, handleCheckpoint(
                        JsonRpcDispatcher::extractParam(req.params, "height")));
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getPruningStatus",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handlePruningStatus());
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getArchiveStatus",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handleArchiveStatus());
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getArchiveReplication",
      [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(req.id, handleArchiveReplication());
      });

  m_jsonRpcDispatcher.registerHandler(
      "nodo_getEvents", [this](const JsonRpcRequest &req) -> JsonRpcResponse {
        return JsonRpcResponse::success(
            req.id,
            handleEvents(
                JsonRpcDispatcher::extractParam(req.params, "afterSequence"),
                JsonRpcDispatcher::extractParam(req.params, "type"),
                JsonRpcDispatcher::extractParam(req.params, "limit")));
      });
}

std::string NodeRpcServer::handleJsonRpc(const std::string &body) {
  if (body.empty()) {
    return JsonRpcResponse::makeError("", JsonRpcError::INVALID_REQUEST,
                                      "Empty JSON-RPC request body")
        .serialize();
  }
  return m_jsonRpcDispatcher.dispatch(body).serialize();
}

// ---------------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------------

NodeRpcServer::HttpDispatchResponse
NodeRpcServer::dispatch(const std::string &method, const std::string &path,
                        const std::string &body) {
  // Every handler reads or writes NodeRuntime (blockchain, mempool, ...),
  // which is also touched by the daemon's tick/consensus/block-production
  // thread. Hold the shared mutex for the whole request so no handler ever
  // observes a torn read (e.g. mid-reallocation Blockchain::m_blocks) or
  // races admitTransaction against a concurrent block commit.
  std::lock_guard<std::mutex> lock(m_runtimeMutex);

  // Normalize path: strip query string.
  const std::string cleanPath = path.substr(0, path.find('?'));

  if (cleanPath == "/rpc" || cleanPath == "/jsonrpc") {
    if (method != "POST") {
      return {405, jsonError("Method not allowed. Use POST for JSON-RPC.")};
    }
    return {200, handleJsonRpc(body)};
  }

  // Segment 0 is empty (before first '/'), segment 1 is the route.
  const std::string route = pathSegment(cleanPath, 1);
  const std::string param = pathSegment(cleanPath, 2);

  if (route == "status" && method == "GET") {
    return {200, handleStatus()};
  }
  if (route == "health" && method == "GET") {
    return {200, handleHealth()};
  }
  if (route == "metrics" && method == "GET") {
    if (param == "prometheus") {
      return {200, handlePrometheusMetrics(), "text/plain; version=0.0.4"};
    }
    return {200, handleMetrics()};
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
  if (route == "light" && method == "GET") {
    if (param == "checkpoint" || param.empty()) {
      return {200, handleLightCheckpoint()};
    }
    if (param == "headers") {
      return {200, handleLightHeaders(pathSegment(cleanPath, 3),
                                      pathSegment(cleanPath, 4))};
    }
    if (param == "account") {
      const std::string address = pathSegment(cleanPath, 3);
      if (address.empty()) {
        return {400, jsonError("Missing light-client account address")};
      }
      return {200, handleLightAccountProof(address)};
    }
    if (param == "tx") {
      const std::string txId = pathSegment(cleanPath, 3);
      if (txId.empty()) {
        return {400, jsonError("Missing light-client transaction id")};
      }
      return {200, handleLightTransactionProof(txId)};
    }
    return {404, jsonError("Unknown light route: " + param)};
  }
  if (route == "events" && method == "GET") {
    return {200, handleEvents("0", "", "100")};
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
} // namespace nodo::node
