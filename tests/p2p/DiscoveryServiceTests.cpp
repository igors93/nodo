#include "p2p/DiscoveryService.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace nodo::p2p;

void testRoutingTable() {
  DiscoveryService dht("node-a", 0, 8000);
  dht.addPeer("node-b", "127.0.0.1", 8001, 9001);
  dht.addPeer("node-c", "127.0.0.1", 8002, 9002);

  auto closest = dht.findClosestPeers("node-d", 2);
  assert(closest.size() == 2);
  std::cout << "Kademlia routing table and XOR distance tests passed\n";
}

void testUdpDiscovery() {
  DiscoveryService nodeA("node-a", 0, 8000);
  DiscoveryService nodeB("node-b", 0, 8001);

  nodeA.start();
  nodeB.start();

  // Register discovery callback on node B
  std::string discoveredPeerId;
  std::string discoveredHost;
  std::uint16_t discoveredTcpPort = 0;
  nodeB.registerPeerDiscoveredCallback([&](const std::string &peerId,
                                           const std::string &host,
                                           std::uint16_t tcpPort) {
    discoveredPeerId = peerId;
    discoveredHost = host;
    discoveredTcpPort = tcpPort;
  });

  // Manually add B to A
  nodeA.addPeer("node-b", "127.0.0.1", 8001, nodeB.localUdpPort());

  // A bootstraps / pings B
  std::vector<std::pair<std::string, std::pair<std::string, std::uint16_t>>>
      seedPeers = {{"node-b", {"127.0.0.1", nodeB.localUdpPort()}}};
  nodeA.bootstrap(seedPeers);

  // Wait for UDP packets to be sent and received
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Assert that B discovered A dynamically from the incoming UDP ping
  assert(discoveredPeerId == "node-a");
  assert(discoveredHost == "127.0.0.1");
  assert(discoveredTcpPort == 8000);

  nodeA.stop();
  nodeB.stop();

  std::cout << "Kademlia UDP discovery network tests passed\n";
}

int main() {
  testRoutingTable();
  testUdpDiscovery();
  std::cout << "All DiscoveryService tests passed\n";
  return 0;
}
