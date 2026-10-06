#include "../common/TestFramework.hpp"

#include "config/NetworkParameters.hpp"
#include "config/ProtocolVersion.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeDaemon.hpp"
#include "utils/Amount.hpp"

#include <asio.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace nodo;
using namespace std::chrono_literals;
using nodo::test::require;

std::int64_t unixTime() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

bool portsAvailable(std::uint16_t p2p, std::uint16_t rpc) {
  asio::io_context io;
  asio::error_code error;
  asio::ip::tcp::acceptor tcp(io);
  tcp.open(asio::ip::tcp::v4(), error);
  if (error) return false;
  tcp.bind({asio::ip::address_v4::loopback(), p2p}, error);
  if (error) return false;
  asio::ip::udp::socket udp(io);
  udp.open(asio::ip::udp::v4(), error);
  if (error) return false;
  udp.bind({asio::ip::address_v4::loopback(),
            static_cast<std::uint16_t>(p2p + 1)}, error);
  if (error) return false;
  asio::ip::tcp::acceptor rpcProbe(io);
  rpcProbe.open(asio::ip::tcp::v4(), error);
  if (error) return false;
  rpcProbe.bind({asio::ip::address_v4::loopback(), rpc}, error);
  return !error;
}

std::uint16_t findPortBase() {
  for (std::uint32_t base = 36000; base < 59000; base += 8) {
    const auto port = static_cast<std::uint16_t>(base);
    if (portsAvailable(port, static_cast<std::uint16_t>(base + 2)) &&
        portsAvailable(static_cast<std::uint16_t>(base + 4),
                       static_cast<std::uint16_t>(base + 6))) {
      return port;
    }
  }
  throw std::runtime_error("No free local port range for TCP test.");
}

void testAuthenticatedRealTcpBetweenTwoDaemons() {
  const std::int64_t genesisTime = unixTime() - 5;
  const std::uint16_t port = findPortBase();
  const auto root = std::filesystem::temp_directory_path() /
                    ("nodo-cross-platform-tcp-" +
                     std::to_string(std::chrono::steady_clock::now()
                                        .time_since_epoch()
                                        .count()));
  const crypto::KeyPair validatorA =
      crypto::KeyPair::createDeterministicBls12381KeyPair("tcp-cross-val-a");
  const crypto::KeyPair validatorB =
      crypto::KeyPair::createDeterministicBls12381KeyPair("tcp-cross-val-b");
  const crypto::KeyPair identityA =
      crypto::KeyPair::createDeterministicEd25519KeyPair("tcp-cross-id-a");
  const crypto::KeyPair identityB =
      crypto::KeyPair::createDeterministicEd25519KeyPair("tcp-cross-id-b");
  const crypto::KeyPair user =
      crypto::KeyPair::createDeterministicEd25519KeyPair("tcp-cross-user");

  const config::GenesisConfig genesis(
      config::NetworkParameters::developmentLocal(), genesisTime,
      {config::BootstrapValidatorConfig(validatorA.publicKey(), 1, 1,
                                        "tcp-cross-a", identityA.address().value()),
       config::BootstrapValidatorConfig(validatorB.publicKey(), 1, 1,
                                        "tcp-cross-b", identityB.address().value())},
      {config::GenesisAccountConfig(user.address().value(),
                                    utils::Amount::fromRawUnits(2'000'000'000'000LL), 0)},
      "cross-platform-tcp-genesis");

  const auto configFor = [&](const std::string &id,
                             const crypto::KeyPair &validator,
                             std::uint16_t p2pPort,
                             std::uint16_t rpcPort,
                             const std::string &peerId,
                             std::uint16_t peerPort) {
    const p2p::PeerInfo peer(id, "127.0.0.1:" + std::to_string(p2pPort),
                             config::kProtocolVersion, 0, genesisTime);
    node::NodeDaemonConfig daemon;
    daemon.orchestratorConfig = node::NodeOrchestratorConfig(
        genesis, node::NodeDataDirectoryConfig(root / id), peer,
        validator.address().value(), rpcPort, "127.0.0.1", 1000,
        static_cast<std::size_t>(
            genesis.networkParameters().maxTransactionsPerBlock()), false);
    daemon.maxFractionPerSubnet = 1.0;
    if (!peerId.empty()) {
      daemon.staticPeers.push_back(
          node::NodeDaemonPeerEntry{peerId, "127.0.0.1", peerPort});
    }
    return daemon;
  };

  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider provider;
  node::NodeDaemon first(configFor("tcp-a", validatorA, port,
                                   static_cast<std::uint16_t>(port + 2), "", 0),
                         policy, provider);
  node::NodeDaemon second(
      configFor("tcp-b", validatorB, static_cast<std::uint16_t>(port + 4),
                static_cast<std::uint16_t>(port + 6), "tcp-a", port),
      policy, provider);
  first.setLocalSigner(crypto::Signer(validatorA, provider));
  second.setLocalSigner(crypto::Signer(validatorB, provider));
  first.setLocalNodeIdentity(identityA);
  second.setLocalNodeIdentity(identityB);

  std::error_code cleanupError;
  try {
    const auto firstStart = first.start();
    require(firstStart.running(), "First TCP daemon failed: " + firstStart.reason);
    const auto secondStart = second.start();
    require(secondStart.running(), "Second TCP daemon failed: " + secondStart.reason);
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    bool connected = false;
    while (std::chrono::steady_clock::now() < deadline) {
      const std::int64_t now = unixTime();
      first.tick(now);
      second.tick(now);
      connected = first.orchestrator().tcpRuntime().hasAuthenticatedSession("tcp-b") &&
                  second.orchestrator().tcpRuntime().hasAuthenticatedSession("tcp-a");
      if (connected) break;
      std::this_thread::sleep_for(20ms);
    }
    require(connected, "The daemons did not establish authenticated TCP sessions.");
    second.stop();
    first.stop();
    std::filesystem::remove_all(root, cleanupError);
  } catch (...) {
    second.stop();
    first.stop();
    std::filesystem::remove_all(root, cleanupError);
    throw;
  }
}

} // namespace

int main() {
  try {
    testAuthenticatedRealTcpBetweenTwoDaemons();
    std::cout << "Cross-platform authenticated TCP handshake passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Cross-platform TCP handshake failed: " << error.what() << '\n';
    return 1;
  }
}
