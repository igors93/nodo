#include "node/ChainSyncMessages.hpp"

#include <cassert>
#include <vector>

int main() {
    nodo::node::ChainStatusMessage status(
        "nodo-localnet",
        "nodo-localnet-1",
        "nodo/0.1",
        20,
        "hash-tip",
        19,
        "hash-finalized"
    );

    assert(status.isValid());
    assert(status.peerIsAheadOf(10));
    assert(!status.peerIsAheadOf(20));

    nodo::node::BlockLocator locator(
        11,
        32,
        {"hash-10", "hash-5", "hash-0"}
    );

    assert(locator.isValid());

    nodo::node::NetworkBlockSyncRequest request(
        "node-A",
        locator,
        1000
    );

    assert(request.isValid());
    assert(request.serialize().find("NetworkBlockSyncRequest") != std::string::npos);

    return 0;
}
