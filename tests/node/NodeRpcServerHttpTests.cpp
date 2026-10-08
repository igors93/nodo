// Drives NodeRpcServer over real TCP connections: HTTP framing and status
// codes, connection and rate limits, slow-client deadlines, the WebSocket
// event stream, and start/stop behaviour. Uses an Asio client so it runs on
// every platform, Windows included.

#include "node/NodeRpcServer.hpp"

#include "config/GenesisRegistry.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/PeerMessage.hpp"

#include <asio.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using asio::ip::tcp;
using nodo::node::NodeRpcServer;
using nodo::node::NodeRuntime;
using Clock = std::chrono::steady_clock;

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

NodeRuntime startRuntime() {
  const auto started = nodo::node::NodeRuntimeFactory::startFromGenesis(
      nodo::node::NodeRuntimeConfig(
          nodo::config::GenesisRegistry::get("localnet").genesis(),
          nodo::p2p::PeerInfo("rpc-http-test", "127.0.0.1:29990", "nodo/0.7", 0,
                              1900000000),
          16));
  requireCondition(started.started(), "Runtime must start from genesis.");
  return started.runtime();
}

// A runtime plus a server bound to an ephemeral loopback port.
struct Node {
  NodeRuntime runtime;
  std::mutex runtimeMutex;
  NodeRpcServer server;

  explicit Node(const NodeRpcServer::Limits &limits = NodeRpcServer::Limits{},
                const std::string &bindAddress = "127.0.0.1")
      : runtime(startRuntime()), runtimeMutex(),
        server(runtime, runtimeMutex, nullptr, 0, bindAddress) {
    server.setLimits(limits);
    server.start();
    requireCondition(server.port() != 0, "Port 0 must bind an ephemeral port.");
  }
};

// Sends raw bytes and collects everything the server returns until it closes
// the connection or the timeout passes.
struct Exchange {
  std::string response;
  bool closedByServer = false;
  std::chrono::milliseconds elapsed{0};
};

Exchange roundTrip(std::uint16_t port, const std::string &request,
                  std::chrono::milliseconds timeout = std::chrono::seconds(10),
                  const std::string &host = "127.0.0.1") {
  asio::io_context io;
  tcp::socket socket(io);
  Exchange result;
  std::array<char, 4096> buffer{};

  std::function<void()> readMore = [&] {
    socket.async_read_some(
        asio::buffer(buffer),
        [&](const asio::error_code &error, std::size_t received) {
          result.response.append(buffer.data(), received);
          if (error) {
            result.closedByServer = true;
            return;
          }
          readMore();
        });
  };

  tcp::resolver resolver(io);
  const auto endpoints = resolver.resolve(host, std::to_string(port));
  asio::async_connect(socket, endpoints,
                      [&](const asio::error_code &error, const tcp::endpoint &) {
                        if (error) {
                          return;
                        }
                        if (!request.empty()) {
                          asio::async_write(
                              socket, asio::buffer(request),
                              [](const asio::error_code &, std::size_t) {});
                        }
                        readMore();
                      });

  const auto started = Clock::now();
  io.run_for(timeout);
  result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      Clock::now() - started);
  return result;
}

int statusOf(const std::string &response) {
  if (response.rfind("HTTP/1.", 0) != 0 || response.size() < 12) {
    return 0;
  }
  return std::stoi(response.substr(9, 3));
}

std::string bodyOf(const std::string &response) {
  const std::size_t start = response.find("\r\n\r\n");
  return start == std::string::npos ? "" : response.substr(start + 4);
}

std::string get(const std::string &path, const std::string &version = "1.1") {
  return "GET " + path + " HTTP/" + version + "\r\nHost: localhost\r\n\r\n";
}

std::string post(const std::string &path, const std::string &body) {
  return "POST " + path + " HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
         std::to_string(body.size()) + "\r\n\r\n" + body;
}

void testServesRestAndJsonRpc() {
  Node node;
  const std::uint16_t port = node.server.port();

  const Exchange status = roundTrip(port, get("/status"));
  requireCondition(statusOf(status.response) == 200 &&
                       bodyOf(status.response).find("\"height\"") !=
                           std::string::npos &&
                       status.closedByServer,
                   "GET /status must answer 200 and close: " + status.response);

  const Exchange http10 = roundTrip(port, get("/status", "1.0"));
  requireCondition(statusOf(http10.response) == 200,
                   "HTTP/1.0 clients must still be served.");

  const Exchange rpc = roundTrip(
      port, post("/rpc", R"({"jsonrpc":"2.0","method":"nodo_getChainInfo","id":1})"));
  requireCondition(statusOf(rpc.response) == 200 &&
                       rpc.response.find("\"chainId\"") != std::string::npos &&
                       rpc.response.find("\"id\":1") != std::string::npos,
                   "POST /rpc must dispatch JSON-RPC: " + rpc.response);

  requireCondition(statusOf(roundTrip(port, get("/rpc")).response) == 405,
                   "GET /rpc must answer 405.");
  requireCondition(statusOf(roundTrip(port, get("/no-such-route")).response) ==
                       404,
                   "Unknown routes must answer 404.");

  // Many sequential requests: the old thread-per-connection server kept every
  // finished thread until stop(); this must keep working without growth.
  for (int index = 0; index < 100; ++index) {
    requireCondition(statusOf(roundTrip(port, get("/status")).response) == 200,
                     "Sequential request " + std::to_string(index) + " failed.");
  }
}

void testMalformedRequestsGetStatusCodes() {
  Node node;
  const std::uint16_t port = node.server.port();

  const std::vector<std::pair<std::string, int>> cases{
      {"GARBAGE\r\n\r\n", 400},
      {"GET /status HTTP/2.0\r\n\r\n", 400},
      {"GET status HTTP/1.1\r\n\r\n", 400},
      {"GET /status HTTP/1.1\r\nNoColonHere\r\n\r\n", 400},
      {"GET /status HTTP/1.1\r\nContent-Length : 0\r\n\r\n", 400},
      {"GET /status HTTP/1.1\r\n folded: header\r\n\r\n", 400},
      {"POST /rpc HTTP/1.1\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}",
       400},
      {"POST /rpc HTTP/1.1\r\nContent-Length: -1\r\n\r\n", 400},
      {"POST /rpc HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", 501},
      {"POST /rpc HTTP/1.1\r\nContent-Length: 70000\r\n\r\n", 413},
      {"GET /status HTTP/1.1\r\nX-Big: " + std::string(20000, 'a') + "\r\n\r\n",
       431},
      {"GET /status HTTP/1.1\r\nX-Big: " + std::string(20000, 'a'), 431},
  };

  for (const auto &[request, expected] : cases) {
    const Exchange result = roundTrip(port, request);
    requireCondition(statusOf(result.response) == expected,
                     "Expected HTTP " + std::to_string(expected) + " for '" +
                         request.substr(0, 60) +
                         "', got: " + result.response.substr(0, 80));
  }
}

// A client that never finishes its request is dropped at the deadline.
void testSlowClientIsDisconnected() {
  NodeRpcServer::Limits limits;
  limits.requestTimeout = std::chrono::milliseconds(300);
  Node node(limits);

  const Exchange result = roundTrip(node.server.port(),
                                   "GET /status HTTP/1.1\r\nHost: x\r\n",
                                   std::chrono::seconds(5));
  requireCondition(result.closedByServer && result.response.empty() &&
                       result.elapsed < std::chrono::seconds(3),
                   "An incomplete request must be closed at the deadline.");
}

void testConnectionCapAndRelease() {
  NodeRpcServer::Limits limits;
  limits.maxConnections = 2;
  limits.maxWebSocketSessions = 1;
  Node node(limits);
  const std::uint16_t port = node.server.port();

  asio::io_context io;
  const tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), port);
  std::vector<std::unique_ptr<tcp::socket>> idle;
  for (int index = 0; index < 2; ++index) {
    idle.push_back(std::make_unique<tcp::socket>(io));
    idle.back()->connect(endpoint);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  const Exchange refused = roundTrip(port, get("/status"), std::chrono::seconds(5));
  requireCondition(refused.closedByServer && refused.response.empty(),
                   "A connection beyond the cap must be closed unanswered.");

  idle.clear(); // closing the idle connections frees their slots
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  requireCondition(statusOf(roundTrip(port, get("/status")).response) == 200,
                   "Released connection slots must be reusable.");
}

void testRateLimit() {
  NodeRpcServer::Limits limits;
  limits.maxRequestsPerWindow = 2;
  Node node(limits);
  const std::uint16_t port = node.server.port();

  requireCondition(statusOf(roundTrip(port, get("/status")).response) == 200 &&
                       statusOf(roundTrip(port, get("/status")).response) == 200,
                   "Requests within the budget must be served.");
  requireCondition(statusOf(roundTrip(port, get("/status")).response) == 429,
                   "Requests over the budget must get 429.");
}

std::string maskedFrame(std::uint8_t opcode, const std::string &payload) {
  std::string frame;
  frame += static_cast<char>(0x80 | opcode);
  frame += static_cast<char>(0x80 | payload.size());
  const std::array<char, 4> mask{0x11, 0x22, 0x33, 0x44};
  frame.append(mask.data(), mask.size());
  for (std::size_t index = 0; index < payload.size(); ++index) {
    frame += static_cast<char>(payload[index] ^ mask[index % 4]);
  }
  return frame;
}

std::string upgradeRequest(const std::string &key) {
  return "GET /events HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
         "Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
         "Sec-WebSocket-Key: " +
         key + "\r\n\r\n";
}

void testWebSocketEventStream() {
  Node node;
  const std::uint16_t port = node.server.port();

  // RFC 6455 section 1.3 example key and its expected accept value. The
  // handshake, a ping and a close frame all arrive in one write.
  const Exchange session = roundTrip(
      port,
      upgradeRequest("dGhlIHNhbXBsZSBub25jZQ==") +
          maskedFrame(0x9, "hi") + maskedFrame(0x8, ""),
      std::chrono::seconds(5));
  requireCondition(
      statusOf(session.response) == 101 &&
          session.response.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") !=
              std::string::npos &&
          session.response.find("rpc_subscription") != std::string::npos,
      "Upgrade must answer 101 with the RFC accept key and a hello event: " +
          session.response.substr(0, 400));
  requireCondition(session.response.find(std::string("\x8a\x02hi")) !=
                           std::string::npos &&
                       session.response.find(std::string("\x88")) !=
                           std::string::npos &&
                       session.closedByServer,
                   "Ping must be answered with pong, and close with close.");

  // RFC 6455 section 5.1: an unmasked client frame closes the connection.
  const Exchange unmasked = roundTrip(
      port, upgradeRequest("dGhlIHNhbXBsZSBub25jZQ==") + std::string("\x89\x00", 2),
      std::chrono::seconds(5));
  requireCondition(unmasked.closedByServer &&
                       unmasked.response.find(std::string("\x88")) !=
                           std::string::npos,
                   "An unmasked client frame must close the stream.");

  requireCondition(
      statusOf(roundTrip(port, upgradeRequest("short")).response) == 400,
      "A malformed Sec-WebSocket-Key must be rejected.");
}

void testWebSocketCapAndPromptStop() {
  NodeRpcServer::Limits limits;
  limits.maxWebSocketSessions = 1;
  auto node = std::make_unique<Node>(limits);
  const std::uint16_t port = node->server.port();

  asio::io_context io;
  tcp::socket subscriber(io);
  subscriber.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
  asio::write(subscriber, asio::buffer(upgradeRequest("dGhlIHNhbXBsZSBub25jZQ==")));
  std::array<char, 512> buffer{};
  const std::size_t received = subscriber.read_some(asio::buffer(buffer));
  requireCondition(std::string(buffer.data(), received).rfind("HTTP/1.1 101", 0) ==
                       0,
                   "The first subscriber must be upgraded.");

  requireCondition(
      statusOf(roundTrip(port, upgradeRequest("dGhlIHNhbXBsZSBub25jZQ==")).response) ==
          503,
      "Subscribers beyond the WebSocket cap must get 503.");

  // The old server needed a receive timeout plus a sleep (up to three
  // seconds) before a WebSocket thread noticed stop().
  const auto started = Clock::now();
  node->server.stop();
  requireCondition(Clock::now() - started < std::chrono::seconds(1),
                   "stop() must return promptly with a subscriber connected.");
  requireCondition(!node->server.isRunning(), "stop() must stop the server.");
}

void testBindingAndLifecycle() {
  Node node(NodeRpcServer::Limits{}, "localhost");
  requireCondition(
      statusOf(roundTrip(node.server.port(), get("/status"), std::chrono::seconds(10),
                        "localhost")
                   .response) == 200,
      "A host-name bind address must resolve and serve.");

  bool threw = false;
  try {
    node.server.setLimits(NodeRpcServer::Limits{});
  } catch (const std::logic_error &) {
    threw = true;
  }
  requireCondition(threw, "Limits must not change while running.");

  node.server.stop();
  node.server.stop(); // idempotent
  node.server.start();
  requireCondition(node.server.isRunning() &&
                       statusOf(roundTrip(node.server.port(), get("/status"),
                                         std::chrono::seconds(10), "localhost")
                                    .response) == 200,
                   "A stopped server must be restartable.");

  NodeRuntime runtime = startRuntime();
  std::mutex mutex;
  NodeRpcServer invalid(runtime, mutex, nullptr, 0, "not a valid host!");
  threw = false;
  try {
    invalid.start();
  } catch (const std::runtime_error &) {
    threw = true;
  }
  requireCondition(threw && !invalid.isRunning(),
                   "An unresolvable bind address must fail start().");
}

} // namespace

int main() {
  try {
    testServesRestAndJsonRpc();
    testMalformedRequestsGetStatusCodes();
    testSlowClientIsDisconnected();
    testConnectionCapAndRelease();
    testRateLimit();
    testWebSocketEventStream();
    testWebSocketCapAndPromptStop();
    testBindingAndLifecycle();

    std::cout << "Nodo NodeRpcServer HTTP tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo NodeRpcServer HTTP tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
