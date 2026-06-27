#include "p2p/OutboundMessageQueue.hpp"

#include <cassert>

int main() {
    nodo::p2p::OutboundMessageQueue queue(1);

    nodo::p2p::NetworkEnvelope envelope(
        "nodo-localnet",
        "nodo-localnet-1",
        "nodo/0.1",
        nodo::p2p::NetworkMessageType::PING,
        "node-A",
        1000,
        60,
        "ping"
    );

    assert(queue.enqueue("node-B", envelope).enqueued());
    assert(!queue.enqueue("node-B", envelope).enqueued());
    assert(queue.sizeForPeer("node-B") == 1);
    assert(queue.totalSize() == 1);

    const auto dequeued = queue.dequeue("node-B");
    assert(dequeued.has_value());
    assert(queue.empty());

    return 0;
}
