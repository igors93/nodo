#ifndef NODO_TESTS_COMMON_REAL_TCP_NODE_TEST_SUPPORT_HPP
#define NODO_TESTS_COMMON_REAL_TCP_NODE_TEST_SUPPORT_HPP

/*
 * Shared harness for real multi-node TCP end-to-end tests.
 *
 * Each of the 3 nodes runs as a real, separate OS process (fork + exec of
 * the current test binary's own daemon-child routine), bound to real
 * loopback TCP ports, communicating exclusively over the same TcpTransport /
 * GossipMesh / NodeRpcServer stack a production node uses. Nothing here
 * simulates the network layer.
 *
 * This header is included (not compiled standalone) by multiple test .cpp
 * files. Everything lives inside an anonymous namespace, so each including
 * translation unit gets its own private copy — safe to include from several
 * test executables without ODR conflicts.
 *
 * Adapted from the proven pattern in
 * tests/node/RealTcpThreeNodeEndToEndTests.cpp, generalized so callers can
 * control static-peer topology (needed for multi-hop propagation tests) and
 * the (height, round) used to look up the scheduled proposer (needed for
 * view-change / proposer-failover tests).
 */

#include "config/NetworkParameters.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/ChainAuditResult.hpp"
#include "node/ChainAuditor.hpp"
#include "node/NodeDaemon.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "utils/Amount.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

using namespace nodo;
using namespace std::chrono_literals;

#ifndef _WIN32

constexpr std::size_t kTestNodeCount = 3;
constexpr std::int64_t kConsensusTickMilliseconds = 1000;
constexpr std::int64_t kDaemonTickMilliseconds = 20;
constexpr std::chrono::seconds kRpcStartupTimeout = 120s;

volatile std::sig_atomic_t gChildStopRequested = 0;

void requestChildStop(int) { gChildStopRequested = 1; }

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::int64_t unixTime() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

bool canBindPort(std::uint16_t port, int socketType) {
  const int fd = ::socket(AF_INET, socketType, 0);
  if (fd < 0) {
    return false;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  const bool available =
      ::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
  ::close(fd);
  return available;
}

std::uint16_t findTcpPort(std::uint16_t start,
                          const std::vector<std::uint16_t> &reserved) {
  for (std::uint32_t candidate = start; candidate < 60000; ++candidate) {
    const auto port = static_cast<std::uint16_t>(candidate);
    if (std::find(reserved.begin(), reserved.end(), port) == reserved.end() &&
        canBindPort(port, SOCK_STREAM)) {
      return port;
    }
  }
  throw std::runtime_error("Unable to allocate a local TCP port for E2E test.");
}

std::uint16_t findP2pPort(std::uint16_t start,
                          const std::vector<std::uint16_t> &reserved) {
  for (std::uint32_t candidate = start; candidate < 59999; candidate += 2) {
    const auto tcpPort = static_cast<std::uint16_t>(candidate);
    const auto udpPort = static_cast<std::uint16_t>(candidate + 1);
    const bool reservedPort =
        std::find(reserved.begin(), reserved.end(), tcpPort) !=
            reserved.end() ||
        std::find(reserved.begin(), reserved.end(), udpPort) != reserved.end();
    if (!reservedPort && canBindPort(tcpPort, SOCK_STREAM) &&
        canBindPort(udpPort, SOCK_DGRAM)) {
      return tcpPort;
    }
  }
  throw std::runtime_error("Unable to allocate local P2P ports for E2E test.");
}

struct NodeSpec {
  std::string nodeId;
  std::filesystem::path dataDirectory;
  std::uint16_t p2pPort;
  std::uint16_t rpcPort;
  crypto::KeyPair validatorKey;
  crypto::KeyPair identityKey;
};

using NodeSpecs = std::array<NodeSpec, kTestNodeCount>;

// Static-peer topology: topology[i] lists the indices node i dials as static
// peers. A connection is bidirectional once established, so a full mesh only
// needs each pair connected once (see fullMeshTopology()).
using Topology = std::array<std::vector<std::size_t>, kTestNodeCount>;

Topology fullMeshTopology() {
  Topology topology;
  for (std::size_t i = 0; i < kTestNodeCount; ++i) {
    for (std::size_t j = 0; j < i; ++j) {
      topology[i].push_back(j);
    }
  }
  return topology;
}

std::filesystem::path childStatusPath(const NodeSpec &spec) {
  return spec.dataDirectory.parent_path() / (spec.nodeId + ".status");
}

void writeChildStatus(const NodeSpec &spec, const std::string &status) {
  std::error_code directoryError;
  std::filesystem::create_directories(spec.dataDirectory.parent_path(),
                                      directoryError);
  std::ofstream output(childStatusPath(spec), std::ios::trunc);
  if (output) {
    output << status;
  }
}

std::string readChildStatus(const NodeSpec &spec) {
  std::ifstream input(childStatusPath(spec));
  if (!input) {
    return "NO_STATUS";
  }
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

NodeSpecs makeNodeSpecs(const std::filesystem::path &root,
                        const std::string &seedPrefix) {
  std::vector<std::uint16_t> reserved;
  const std::uint16_t seed = static_cast<std::uint16_t>(
      24000 + (static_cast<unsigned long>(::getpid()) % 1000UL) * 20UL);

  NodeSpecs specs;
  for (std::size_t index = 0; index < specs.size(); ++index) {
    const std::uint16_t p2pPort =
        findP2pPort(static_cast<std::uint16_t>(seed + index * 4), reserved);
    reserved.push_back(p2pPort);
    reserved.push_back(static_cast<std::uint16_t>(p2pPort + 1));
    const std::uint16_t rpcPort =
        findTcpPort(static_cast<std::uint16_t>(seed + 100 + index), reserved);
    reserved.push_back(rpcPort);

    const std::string suffix(1, static_cast<char>('a' + index));
    specs[index] = NodeSpec{seedPrefix + "-node-" + suffix,
                            root / ("node-" + suffix),
                            p2pPort,
                            rpcPort,
                            crypto::KeyPair::createDeterministicBls12381KeyPair(
                                seedPrefix + "-validator-" + suffix),
                            crypto::KeyPair::createDeterministicEd25519KeyPair(
                                seedPrefix + "-identity-" + suffix)};
  }
  return specs;
}

crypto::KeyPair testUserKey(const std::string &seedPrefix) {
  return crypto::KeyPair::createDeterministicEd25519KeyPair(seedPrefix +
                                                            "-funded-user");
}

config::GenesisConfig makeGenesis(const NodeSpecs &specs,
                                  std::int64_t genesisTimestamp,
                                  const std::string &seedPrefix) {
  std::vector<config::BootstrapValidatorConfig> validators;
  validators.reserve(specs.size());
  for (const NodeSpec &spec : specs) {
    validators.emplace_back(spec.validatorKey.publicKey(), 1, 1,
                            seedPrefix + "-" + spec.nodeId);
  }

  return config::GenesisConfig(
      config::NetworkParameters::developmentLocal(), genesisTimestamp,
      validators,
      {config::GenesisAccountConfig(
          testUserKey(seedPrefix).address().value(),
          utils::Amount::fromRawUnits(2'000'000'000'000LL), 0)},
      seedPrefix + "-genesis");
}

p2p::PeerInfo peerInfo(const NodeSpec &spec, std::int64_t timestamp) {
  return p2p::PeerInfo(spec.nodeId, "127.0.0.1:" + std::to_string(spec.p2pPort),
                       "nodo/0.1", 0, timestamp);
}

int runDaemonChild(std::size_t nodeIndex, const NodeSpecs &specs,
                   const config::GenesisConfig &genesis,
                   const Topology &topology) {
  gChildStopRequested = 0;
  std::signal(SIGTERM, requestChildStop);
  std::signal(SIGINT, requestChildStop);

  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider validatorProvider;
  const NodeSpec &local = specs.at(nodeIndex);
  writeChildStatus(local, "STARTING");

  node::NodeOrchestratorConfig orchestratorConfig(
      genesis, node::NodeDataDirectoryConfig(local.dataDirectory),
      peerInfo(local, genesis.genesisTimestamp()),
      local.validatorKey.address().value(), local.rpcPort, "127.0.0.1",
      kConsensusTickMilliseconds,
      static_cast<std::size_t>(
          genesis.networkParameters().maxTransactionsPerBlock()));

  node::NodeDaemonConfig daemonConfig;
  daemonConfig.orchestratorConfig = orchestratorConfig;
  for (const std::size_t peerIndex : topology.at(nodeIndex)) {
    daemonConfig.staticPeers.push_back(node::NodeDaemonPeerEntry{
        specs[peerIndex].nodeId, "127.0.0.1", specs[peerIndex].p2pPort});
  }

  node::NodeDaemon daemon(daemonConfig, policy, validatorProvider);
  daemon.setLocalSigner(crypto::Signer(local.validatorKey, validatorProvider));
  daemon.setLocalNodeIdentity(local.identityKey);

  const node::NodeOrchestratorStartResult started = daemon.start();
  if (!started.running()) {
    writeChildStatus(local, "FAILED: daemon start: " + started.reason);
    std::cerr << local.nodeId << " failed to start: " << started.reason << '\n';
    return 2;
  }
  if (!daemon.orchestrator().rpcRunning()) {
    writeChildStatus(local, "FAILED: RPC server on port " +
                                std::to_string(local.rpcPort) + ": " +
                                daemon.orchestrator().rpcStartError());
    daemon.stop();
    return 3;
  }
  writeChildStatus(local, "RUNNING");

  while (!gChildStopRequested && daemon.isRunning()) {
    daemon.tick(unixTime());
    std::this_thread::sleep_for(
        std::chrono::milliseconds(kDaemonTickMilliseconds));
  }

  daemon.stop();
  return 0;
}

// Spawns a single daemon child directly, independent of ChildProcesses'
// single-shared-genesis assumption. Useful when one node in the test
// topology must run under a different genesis/config than the rest (e.g.
// a peer with a mismatched genesis that is expected to be rejected).
pid_t forkDaemonChild(std::size_t nodeIndex, const NodeSpecs &specs,
                      const config::GenesisConfig &genesis,
                      const Topology &topology) {
  const pid_t pid = ::fork();
  if (pid < 0) {
    throw std::runtime_error("fork() failed for daemon child.");
  }
  if (pid == 0) {
    const int result = runDaemonChild(nodeIndex, specs, genesis, topology);
    ::_exit(result);
  }
  return pid;
}

// Stops a process spawned with forkDaemonChild(). Sends SIGCONT first in
// case the process is currently SIGSTOPped, then SIGTERM, and escalates to
// SIGKILL if it does not exit gracefully within the timeout.
void stopPid(pid_t pid, const std::string &label) {
  if (pid <= 0) {
    return;
  }
  (void)::kill(pid, SIGCONT);
  (void)::kill(pid, SIGTERM);
  for (int attempt = 0; attempt < 100; ++attempt) {
    int status = 0;
    if (::waitpid(pid, &status, WNOHANG) == pid) {
      require(WIFEXITED(status) && WEXITSTATUS(status) == 0,
              label + " exited with an error.");
      return;
    }
    std::this_thread::sleep_for(50ms);
  }
  (void)::kill(pid, SIGKILL);
  int status = 0;
  (void)::waitpid(pid, &status, 0);
}

class ChildProcesses {
public:
  ChildProcesses(const NodeSpecs &specs, const config::GenesisConfig &genesis,
                 Topology topology)
      : m_specs(specs), m_genesis(genesis), m_topology(std::move(topology)),
        m_pids{} {}

  ~ChildProcesses() { stopAll(); }

  void start(std::size_t index) {
    require(index < m_pids.size(), "Child process index is invalid.");
    require(m_pids[index] == 0, "Child process is already running.");

    const pid_t pid = ::fork();
    if (pid < 0) {
      throw std::runtime_error("fork() failed for daemon child.");
    }
    if (pid == 0) {
      const int result = runDaemonChild(index, m_specs, m_genesis, m_topology);
      ::_exit(result);
    }
    m_pids[index] = pid;
  }

  void startAll() {
    for (std::size_t index = 0; index < m_pids.size(); ++index) {
      start(index);
    }
  }

  // Pause/resume without severing TCP connections or losing peer
  // authentication state, unlike stop()/start() (which restarts the
  // process and re-handshakes). Useful for simulating a validator that
  // stops making progress without the rest of the network observing a
  // disconnect.
  void pause(std::size_t index) {
    if (index < m_pids.size() && m_pids[index] != 0) {
      (void)::kill(m_pids[index], SIGSTOP);
    }
  }

  void resume(std::size_t index) {
    if (index < m_pids.size() && m_pids[index] != 0) {
      (void)::kill(m_pids[index], SIGCONT);
    }
  }

  void stop(std::size_t index) {
    if (index >= m_pids.size() || m_pids[index] == 0) {
      return;
    }

    const pid_t pid = m_pids[index];
    (void)::kill(pid, SIGTERM);
    for (int attempt = 0; attempt < 100; ++attempt) {
      int status = 0;
      const pid_t waited = ::waitpid(pid, &status, WNOHANG);
      if (waited == pid) {
        m_pids[index] = 0;
        require(WIFEXITED(status) && WEXITSTATUS(status) == 0,
                m_specs[index].nodeId + " exited with an error.");
        return;
      }
      std::this_thread::sleep_for(50ms);
    }

    (void)::kill(pid, SIGKILL);
    int status = 0;
    (void)::waitpid(pid, &status, 0);
    m_pids[index] = 0;
    throw std::runtime_error(m_specs[index].nodeId +
                             " did not stop gracefully.");
  }

  void stopAll() noexcept {
    for (std::size_t index = 0; index < m_pids.size(); ++index) {
      if (m_pids[index] == 0) {
        continue;
      }
      const pid_t pid = m_pids[index];
      // Resume in case the process is currently SIGSTOPped: a stopped
      // process does not act on SIGTERM until continued.
      (void)::kill(pid, SIGCONT);
      (void)::kill(pid, SIGTERM);
      for (int attempt = 0; attempt < 40; ++attempt) {
        int status = 0;
        if (::waitpid(pid, &status, WNOHANG) == pid) {
          m_pids[index] = 0;
          break;
        }
        std::this_thread::sleep_for(50ms);
      }
      if (m_pids[index] != 0) {
        (void)::kill(pid, SIGKILL);
        int status = 0;
        (void)::waitpid(pid, &status, 0);
        m_pids[index] = 0;
      }
    }
  }

private:
  const NodeSpecs &m_specs;
  const config::GenesisConfig &m_genesis;
  Topology m_topology;
  std::array<pid_t, kTestNodeCount> m_pids;
};

struct HttpResponse {
  int statusCode = 0;
  std::string body;
};

bool sendAll(int fd, const std::string &request) {
  std::size_t sent = 0;
  while (sent < request.size()) {
    const ssize_t result =
        ::send(fd, request.data() + sent, request.size() - sent,
#ifdef MSG_NOSIGNAL
               MSG_NOSIGNAL
#else
               0
#endif
        );
    if (result <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(result);
  }
  return true;
}

std::optional<HttpResponse> httpRequest(std::uint16_t port,
                                        const std::string &method,
                                        const std::string &path,
                                        const std::string &body = "") {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return std::nullopt;
  }

  timeval timeout{};
  timeout.tv_sec = 5;
  (void)::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  (void)::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  (void)::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) !=
      0) {
    ::close(fd);
    return std::nullopt;
  }

  std::string request = method + " " + path +
                        " HTTP/1.0\r\n"
                        "Host: 127.0.0.1\r\n"
                        "Connection: close\r\n";
  if (!body.empty()) {
    request += "Content-Type: text/plain\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\n";
  }
  request += "\r\n" + body;

  if (!sendAll(fd, request)) {
    ::close(fd);
    return std::nullopt;
  }

  std::string response;
  std::array<char, 4096> buffer{};
  while (true) {
    const ssize_t received = ::recv(fd, buffer.data(), buffer.size(), 0);
    if (received == 0) {
      break;
    }
    if (received < 0) {
      ::close(fd);
      return std::nullopt;
    }
    response.append(buffer.data(), static_cast<std::size_t>(received));
  }
  ::close(fd);

  const std::size_t lineEnd = response.find("\r\n");
  const std::size_t bodyStart = response.find("\r\n\r\n");
  if (lineEnd == std::string::npos || bodyStart == std::string::npos) {
    return std::nullopt;
  }

  HttpResponse parsed;
  const std::string statusLine = response.substr(0, lineEnd);
  const std::size_t firstSpace = statusLine.find(' ');
  const std::size_t secondSpace = statusLine.find(' ', firstSpace + 1);
  if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
    return std::nullopt;
  }
  parsed.statusCode = std::stoi(
      statusLine.substr(firstSpace + 1, secondSpace - firstSpace - 1));
  parsed.body = response.substr(bodyStart + 4);
  return parsed;
}

std::optional<std::uint64_t> jsonUnsigned(const std::string &json,
                                          const std::string &field) {
  const std::string marker = "\"" + field + "\":";
  const std::size_t start = json.find(marker);
  if (start == std::string::npos) {
    return std::nullopt;
  }
  std::size_t cursor = start + marker.size();
  std::size_t end = cursor;
  while (end < json.size() && json[end] >= '0' && json[end] <= '9') {
    ++end;
  }
  if (end == cursor) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(
      std::stoull(json.substr(cursor, end - cursor)));
}

std::optional<std::string> jsonString(const std::string &json,
                                      const std::string &field) {
  const std::string marker = "\"" + field + "\":\"";
  const std::size_t start = json.find(marker);
  if (start == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t valueStart = start + marker.size();
  const std::size_t end = json.find('"', valueStart);
  if (end == std::string::npos) {
    return std::nullopt;
  }
  return json.substr(valueStart, end - valueStart);
}

std::optional<bool> jsonBool(const std::string &json,
                             const std::string &field) {
  const std::string trueMarker = "\"" + field + "\":true";
  const std::string falseMarker = "\"" + field + "\":false";
  if (json.find(trueMarker) != std::string::npos) {
    return true;
  }
  if (json.find(falseMarker) != std::string::npos) {
    return false;
  }
  return std::nullopt;
}

template <typename Predicate>
bool waitUntil(std::chrono::seconds timeout, Predicate predicate) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    // 100ms keeps polling responsive while staying well under the RPC
    // server's per-source-IP rate limit even when a predicate issues
    // several requests per node per tick (e.g. checking multiple specs).
    std::this_thread::sleep_for(100ms);
  }
  return false;
}

std::optional<HttpResponse> statusResponse(const NodeSpec &spec) {
  return httpRequest(spec.rpcPort, "GET", "/status");
}

bool rpcReady(const NodeSpec &spec) {
  const auto response = statusResponse(spec);
  return response.has_value() && response->statusCode == 200;
}

void waitForRpc(const NodeSpec &spec) {
  const auto deadline = std::chrono::steady_clock::now() + kRpcStartupTimeout;
  std::string status = "NO_STATUS";
  while (std::chrono::steady_clock::now() < deadline) {
    if (rpcReady(spec)) {
      return;
    }
    status = readChildStatus(spec);
    if (status.rfind("FAILED:", 0) == 0) {
      throw std::runtime_error(spec.nodeId + " " + status);
    }
    std::this_thread::sleep_for(25ms);
  }
  throw std::runtime_error(spec.nodeId + " RPC did not become ready within " +
                           std::to_string(kRpcStartupTimeout.count()) +
                           " seconds. Child status: " + status);
}

std::uint64_t authenticatedPeerCount(const NodeSpec &spec) {
  const auto response = statusResponse(spec);
  if (!response.has_value()) {
    return 0;
  }
  const auto peers = jsonUnsigned(response->body, "authenticatedPeerCount");
  const auto sessions = jsonUnsigned(response->body, "encryptedSessionCount");
  if (!peers.has_value() || !sessions.has_value()) {
    return 0;
  }
  return std::min(peers.value(), sessions.value());
}

bool hasAuthenticatedPeer(const NodeSpec &spec) {
  return authenticatedPeerCount(spec) > 0;
}

std::uint64_t currentRound(const NodeSpec &spec) {
  const auto response = statusResponse(spec);
  if (!response.has_value()) {
    return 0;
  }
  return jsonUnsigned(response->body, "round").value_or(0);
}

std::uint64_t mempoolSize(const NodeSpec &spec) {
  const auto response = statusResponse(spec);
  if (!response.has_value()) {
    return 0;
  }
  return jsonUnsigned(response->body, "mempoolSize").value_or(0);
}

bool reachedFinalizedHeight(const NodeSpec &spec, std::uint64_t height) {
  const auto response = statusResponse(spec);
  if (!response.has_value()) {
    return false;
  }
  const auto current = jsonUnsigned(response->body, "finalizedHeight");
  return current.has_value() && current.value() >= height;
}

std::string blockHashAt(const NodeSpec &spec, std::uint64_t height) {
  std::optional<std::string> hash;
  const bool querySucceeded = waitUntil(10s, [&] {
    const auto response =
        httpRequest(spec.rpcPort, "GET", "/block/" + std::to_string(height));
    if (!response.has_value() || response->statusCode != 200) {
      return false;
    }
    hash = jsonString(response->body, "hash");
    return hash.has_value() && !hash->empty();
  });
  require(querySucceeded,
          "Unable to query finalized block from " + spec.nodeId + ".");
  return hash.value();
}

// Looks up the scheduled proposer for (height, round) without starting a
// networked node: builds an ephemeral in-memory runtime purely to read its
// derived validator registry, matching the same deterministic schedule real
// nodes use.
std::size_t scheduledProposerIndexAt(const NodeSpecs &specs,
                                     const config::GenesisConfig &genesis,
                                     std::uint64_t height,
                                     std::uint64_t round) {
  const auto runtimeStart =
      node::NodeRuntimeFactory::startFromGenesis(node::NodeRuntimeConfig(
          genesis, peerInfo(specs.front(), genesis.genesisTimestamp()), 16));
  require(runtimeStart.started(), "Unable to derive the scheduled proposer.");
  const node::NodeRuntime &runtime = runtimeStart.runtime();
  const std::string address = consensus::ProposerSchedule::selectProposer(
      runtime.validatorRegistry(), genesis.networkParameters().chainId(),
      height, round);
  for (std::size_t index = 0; index < specs.size(); ++index) {
    if (specs[index].validatorKey.address().value() == address) {
      return index;
    }
  }
  throw std::runtime_error("Scheduled proposer is absent from E2E genesis.");
}

std::size_t scheduledProposerIndex(const NodeSpecs &specs,
                                   const config::GenesisConfig &genesis) {
  // Consensus rounds are 1-based: NodeRuntime boots the round manager at
  // round 1 and RuntimeBlockPipeline advances each new height to round 1,
  // so the first scheduled proposer of height 1 is selected at round 1.
  return scheduledProposerIndexAt(specs, genesis, 1, 1);
}

core::Transaction signedTransfer(const config::GenesisConfig &genesis,
                                 const std::string &seedPrefix,
                                 const std::string &recipientAddress,
                                 std::uint64_t nonce, std::int64_t timestamp) {
  const crypto::Ed25519SignatureProvider provider;
  return core::TransactionBuilder::buildSignedTransfer(
      core::TransactionBuildRequest(
          recipientAddress, utils::Amount::fromRawUnits(1'000),
          utils::Amount::fromRawUnits(100), nonce, timestamp),
      crypto::Signer(testUserKey(seedPrefix), provider),
      genesis.networkParameters().chainId());
}

std::optional<HttpResponse>
submitTransaction(const NodeSpec &spec, const core::Transaction &transaction) {
  const std::string submission = serialization::KeyValueFileCodec::serialize(
      "NODO_RPC_TRANSACTION_SUBMISSION_V1",
      {{"transaction", transaction.serialize()}});
  return httpRequest(spec.rpcPort, "POST", "/submit", submission);
}

void verifyTransactionFinalized(const NodeSpec &spec,
                                const std::string &transactionId,
                                std::uint64_t expectedBlockHeight) {
  const auto response =
      httpRequest(spec.rpcPort, "GET", "/tx/" + transactionId);
  require(response.has_value() && response->statusCode == 200,
          "Unable to query transaction from " + spec.nodeId + ".");
  require(response->body.find("\"blockHeight\":" +
                              std::to_string(expectedBlockHeight)) !=
              std::string::npos,
          spec.nodeId + " did not index the transaction at height " +
              std::to_string(expectedBlockHeight) + ".");
}

void verifyAccountNonce(const NodeSpec &spec, const std::string &seedPrefix,
                        std::uint64_t expectedNonce) {
  const auto response =
      httpRequest(spec.rpcPort, "GET",
                  "/account/" + testUserKey(seedPrefix).address().value());
  require(response.has_value() && response->statusCode == 200,
          "Unable to query account state from " + spec.nodeId + ".");
  require(jsonUnsigned(response->body, "nonce") == expectedNonce,
          spec.nodeId + " account nonce does not reflect expected transfers.");
}

// Loads a stopped node's on-disk data directory and audits the finalized
// chain against the exact ad-hoc genesis used by this local test network.
// Epoch reports are intentionally skipped here: these short E2E scenarios
// stop before an epoch report is produced, while the remaining chain audit
// (artifacts, state, supply continuity, rewards, and treasury) still runs.
void requireChainAuditPasses(const NodeSpec &spec,
                             const config::GenesisConfig &genesis) {
  const node::NodeDataDirectoryConfig directoryConfig(spec.dataDirectory);
  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesis, peerInfo(spec, genesis.genesisTimestamp()));
  const node::ChainAuditResult audit =
      node::ChainAuditor::auditLoadedRuntimeDevMode(load);
  require(audit.passed(), spec.nodeId + " failed chain audit: " +
                              audit.toHumanReadableString());
}

#endif // !_WIN32

} // namespace

#endif
