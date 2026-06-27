#include "node/ChainSyncMessages.hpp"
#include "node/BlockSyncHandler.hpp"

#include <cassert>
#include <string>
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

    assert(nodo::node::BlockSyncHandler::deserializeBlockList(
        "NODO_BLOCK_LIST_V1\ncount=1abc\nBlock{}\n"
    ).empty());

    assert(nodo::node::BlockSyncHandler::deserializeBlockList(
        "NODO_BLOCK_LIST_V1\ncount=2\nBlock{}\n"
    ).empty());

    return 0;
}
