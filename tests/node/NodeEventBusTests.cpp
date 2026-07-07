#include "node/NodeEventBus.hpp"

#include <cassert>
#include <iostream>

using nodo::node::NodeEventBus;
using nodo::node::NodeEventType;

void testPublishesSequentialBoundedEvents() {
  NodeEventBus bus(2);
  const auto first = bus.publish(NodeEventType::NEW_BLOCK, 1, "a", "{}", 10);
  const auto second =
      bus.publish(NodeEventType::BLOCK_FINALIZED, 2, "b", "{}", 11);
  const auto third = bus.publish(NodeEventType::TX_ADMITTED, 2, "b", "{}", 12);

  assert(first.sequence() == 1);
  assert(second.sequence() == 2);
  assert(third.sequence() == 3);
  assert(bus.latestSequence() == 3);
  assert(bus.retainedCount() == 2);

  const auto events = bus.recent(0, 10);
  assert(events.size() == 2);
  assert(events[0].sequence() == 2);
  assert(events[1].sequence() == 3);
}

void testFiltersByTypeAndCursor() {
  NodeEventBus bus(8);
  bus.publish(NodeEventType::NEW_BLOCK, 1, "a", "{}", 10);
  bus.publish(NodeEventType::TX_ADMITTED, 1, "a", "{}", 11);
  bus.publish(NodeEventType::TX_ADMITTED, 2, "b", "{}", 12);

  const auto events = bus.recent(1, 10, NodeEventType::TX_ADMITTED);
  assert(events.size() == 2);
  assert(events[0].type() == NodeEventType::TX_ADMITTED);
  assert(bus.recentJson(0).find("tx_admitted") != std::string::npos);
}

int main() {
  try {
    testPublishesSequentialBoundedEvents();
    testFiltersByTypeAndCursor();
    std::cout << "Nodo NodeEventBus tests passed.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Nodo NodeEventBus tests failed: " << e.what() << "\n";
    return 1;
  }
}
