#include "p2p/OutboundMessageQueue.hpp"

#include <cassert>

namespace {

nodo::p2p::NetworkEnvelope envelope(nodo::p2p::NetworkMessageType type,
                                    const std::string &payload) {
  return nodo::p2p::NetworkEnvelope("nodo-localnet", "nodo-localnet-1",
                                    "nodo/0.1", type, "node-A", 1000, 60,
                                    payload);
}

void testRejectsWhenFullOfNormalMessages() {
  nodo::p2p::OutboundMessageQueue queue(1);

  assert(queue
             .enqueue("node-B",
                      envelope(nodo::p2p::NetworkMessageType::PING, "ping"))
             .enqueued());
  assert(!queue
              .enqueue("node-B",
                       envelope(nodo::p2p::NetworkMessageType::PING, "ping-2"))
              .enqueued());
  assert(queue.sizeForPeer("node-B") == 1);
  assert(queue.totalSize() == 1);

  const auto dequeued = queue.dequeue("node-B");
  assert(dequeued.has_value());
  assert(dequeued->payload() == "ping");
  assert(queue.empty());
}

void testPriorityMessageEvictsNormalMessageWhenFull() {
  nodo::p2p::OutboundMessageQueue queue(1);

  assert(queue
             .enqueue("node-B",
                      envelope(nodo::p2p::NetworkMessageType::PING, "ping"))
             .enqueued());
  assert(
      queue
          .enqueue("node-B",
                   envelope(nodo::p2p::NetworkMessageType::BLOCK_SYNC_REQUEST,
                            "sync-request"))
          .enqueued());

  assert(queue.sizeForPeer("node-B") == 1);
  const auto dequeued = queue.dequeue("node-B");
  assert(dequeued.has_value());
  assert(dequeued->messageType() ==
         nodo::p2p::NetworkMessageType::BLOCK_SYNC_REQUEST);
  assert(dequeued->payload() == "sync-request");
  assert(queue.empty());
}

void testPriorityMessagesStayBeforeNormalMessagesInFifoOrder() {
  nodo::p2p::OutboundMessageQueue queue(4);

  assert(queue
             .enqueue("node-B",
                      envelope(nodo::p2p::NetworkMessageType::PING, "normal-1"))
             .enqueued());
  assert(
      queue
          .enqueue("node-B",
                   envelope(nodo::p2p::NetworkMessageType::BLOCK_SYNC_REQUEST,
                            "priority-1"))
          .enqueued());
  assert(
      queue
          .enqueue(
              "node-B",
              envelope(nodo::p2p::NetworkMessageType::FINALIZED_BLOCK_ARTIFACT,
                       "priority-2"))
          .enqueued());
  assert(
      queue
          .enqueue("node-B",
                   envelope(nodo::p2p::NetworkMessageType::TRANSACTION_GOSSIP,
                            "normal-2"))
          .enqueued());

  const auto first = queue.dequeue("node-B");
  const auto second = queue.dequeue("node-B");
  const auto third = queue.dequeue("node-B");
  const auto fourth = queue.dequeue("node-B");

  assert(first.has_value() && first->payload() == "priority-1");
  assert(second.has_value() && second->payload() == "priority-2");
  assert(third.has_value() && third->payload() == "normal-1");
  assert(fourth.has_value() && fourth->payload() == "normal-2");
  assert(queue.empty());
}

void testPriorityMessageDoesNotEvictPriorityMessage() {
  nodo::p2p::OutboundMessageQueue queue(1);

  assert(
      queue
          .enqueue("node-B",
                   envelope(nodo::p2p::NetworkMessageType::BLOCK_SYNC_REQUEST,
                            "priority-1"))
          .enqueued());
  assert(
      !queue
           .enqueue("node-B",
                    envelope(nodo::p2p::NetworkMessageType::BLOCK_SYNC_RESPONSE,
                             "priority-2"))
           .enqueued());

  const auto dequeued = queue.dequeue("node-B");
  assert(dequeued.has_value());
  assert(dequeued->payload() == "priority-1");
  assert(queue.empty());
}

} // namespace

int main() {
  testRejectsWhenFullOfNormalMessages();
  testPriorityMessageEvictsNormalMessageWhenFull();
  testPriorityMessagesStayBeforeNormalMessagesInFifoOrder();
  testPriorityMessageDoesNotEvictPriorityMessage();
  return 0;
}
