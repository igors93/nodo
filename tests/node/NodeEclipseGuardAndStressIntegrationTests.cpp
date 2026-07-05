#include "../common/RealTcpNodeTestSupport.hpp"
#include "node/NodeDaemon.hpp"
#include "node/NodeOrchestrator.hpp"
#include "node/TcpTestnetNodeRuntime.hpp"
#include <asio.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace nodo;

#ifndef _WIN32

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testEclipseGuardAndStress() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "nodo-eclipse-test";
  std::filesystem::remove_all(root);

  const NodeSpecs specs = makeNodeSpecs(root, "eclipse-test");
  const config::GenesisConfig genesis =
      makeGenesis(specs, unixTime() - 5, "eclipse-test");
  const NodeSpec &local = specs[0];

  node::NodeDaemonConfig daemonConfig;
  daemonConfig.orchestratorConfig = node::NodeOrchestratorConfig(
      genesis, node::NodeDataDirectoryConfig(local.dataDirectory),
      peerInfo(local, genesis.genesisTimestamp()),
      local.validatorKey.address().value(), local.rpcPort, "127.0.0.1", 100,
      500);
  daemonConfig.minOutboundConnections = 4;
  daemonConfig.maxFractionPerSubnet = 0.30;

  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider provider;
  node::NodeDaemon daemon(daemonConfig, policy, provider);
  daemon.setLocalSigner(crypto::Signer(local.validatorKey, provider));
  daemon.setLocalNodeIdentity(local.identityKey);

  daemon.start();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  const std::int64_t now = unixTime();

  // We connect 10 peers from 127.0.0.X to simulate the same /24 subnet.
  // EclipseGuard limits maxFractionPerSubnet (30%).
  // It shouldn't crash, and we can also send garbage data.

  std::vector<std::shared_ptr<asio::ip::tcp::socket>> sockets;
  asio::io_context ioc;

  for (int i = 1; i <= 10; ++i) {
    try {
      auto sock = std::make_shared<asio::ip::tcp::socket>(ioc);
      sock->open(asio::ip::tcp::v4());
      // Bind to 127.0.0.X
      asio::ip::tcp::endpoint bindEp(
          asio::ip::make_address("127.0.0." + std::to_string(i)), 0);
      sock->bind(bindEp);
      asio::ip::tcp::endpoint serverEp(asio::ip::make_address("127.0.0.1"),
                                       local.p2pPort);
      sock->connect(serverEp);
      sockets.push_back(sock);

      // Send some garbage
      std::string garbage = "INVALID_HANDSHAKE_OR_GOSSIP_DATA";
      asio::write(*sock, asio::buffer(garbage));
    } catch (const std::exception &e) {
      std::cout << "Peer " << i << " failed to connect: " << e.what()
                << std::endl;
    }
  }

  std::this_thread::sleep_for(std::chrono::seconds(2));
  daemon.stop();
  std::filesystem::remove_all(root);
}

int main() {
  try {
    testEclipseGuardAndStress();
    std::cout << "NodeEclipseGuardAndStressIntegrationTests passed.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAILED: " << e.what() << '\n';
    return 1;
  }
}

#else

int main() {
  std::cout
      << "NodeEclipseGuardAndStressIntegrationTests skipped on Windows.\n";
  return 0;
}

#endif
