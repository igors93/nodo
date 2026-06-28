#include "node/TcpTestnetNodeRuntime.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace nodo;

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nodo_tcp_peer_store_test";
    const std::filesystem::path peersFile = root / "peers.conf";

    std::filesystem::remove_all(root);

    std::vector<node::TcpTestnetPeerFileEntry> peers;
    peers.emplace_back(
        "node-a",
        p2p::PeerEndpoint("127.0.0.1", 30333),
        "fingerprint-a"
    );
    peers.emplace_back(
        "node-b",
        p2p::PeerEndpoint("127.0.0.1", 30334),
        "fingerprint-b",
        1000,
        1050,
        -25,
        true,
        3
    );

    node::TcpTestnetPeerStore::save(peersFile, peers);
    assert(std::filesystem::exists(peersFile));

    const auto loaded = node::TcpTestnetPeerStore::load(peersFile);
    assert(loaded.size() == 2);
    assert(loaded[0].nodeId() == "node-a");
    assert(loaded[0].endpoint().port() == 30333);
    assert(loaded[1].nodeId() == "node-b");
    assert(loaded[1].publicKeyFingerprint() == "fingerprint-b");
    assert(loaded[1].hasPersistentState());
    assert(loaded[1].firstSeenAt() == 1000);
    assert(loaded[1].lastSeenAt() == 1050);
    assert(loaded[1].score() == -25);
    assert(loaded[1].quarantined());
    assert(loaded[1].invalidMessageCount() == 3);
    assert(!loaded[0].hasPersistentState());

    std::filesystem::remove_all(root);

    std::cout << "tcp testnet peer store tests passed\n";
    return 0;
}
