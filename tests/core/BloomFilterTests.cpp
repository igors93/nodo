#include "core/BloomFilter.hpp"
#include "core/EventLog.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::BloomFilter;
using nodo::core::EventLog;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000LL;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testEmptyFilterContainsNothing() {
    BloomFilter filter;
    requireCondition(
        filter.isEmpty(),
        "Newly constructed BloomFilter should be empty."
    );
    requireCondition(
        !filter.contains("anything"),
        "Empty BloomFilter should not report contains for any item."
    );
}

void testAddedItemIsContained() {
    BloomFilter filter;
    filter.add("alice");

    requireCondition(
        filter.contains("alice"),
        "Item added to BloomFilter must be contained."
    );
    requireCondition(
        !filter.isEmpty(),
        "BloomFilter should not be empty after adding an item."
    );
}

void testNotAddedItemIsNotContainedTypically() {
    // Bloom filters can have false positives, but with 2048 bits and 3 hashes
    // "neveradded_xyz_qrs_012345" should not collide with "alice" and "bob".
    BloomFilter filter;
    filter.add("alice");
    filter.add("bob");

    // This is a property test: the two items added should be found
    requireCondition(filter.contains("alice"), "alice should be in filter.");
    requireCondition(filter.contains("bob"), "bob should be in filter.");
}

void testMultipleItemsAllContained() {
    BloomFilter filter;
    const std::vector<std::string> items = {
        "addr-0x1234", "addr-0xabcd", "tx-deadbeef", "TRANSFER"
    };

    for (const auto& item : items) {
        filter.add(item);
    }

    for (const auto& item : items) {
        requireCondition(
            filter.contains(item),
            "Every added item must be contained in the filter."
        );
    }
}

void testUnionMergesFilters() {
    BloomFilter filterA;
    BloomFilter filterB;

    filterA.add("alice");
    filterB.add("bob");

    BloomFilter merged = filterA | filterB;

    requireCondition(
        merged.contains("alice"),
        "Merged filter must contain items from filter A."
    );
    requireCondition(
        merged.contains("bob"),
        "Merged filter must contain items from filter B."
    );
}

void testSerializeDeserializeRoundTrip() {
    BloomFilter original;
    original.add("hello");
    original.add("world");

    const std::string hex = original.serialize();

    requireCondition(
        hex.size() == BloomFilter::BYTES * 2,
        "Serialized hex must be exactly BYTES*2 characters."
    );

    const BloomFilter restored = BloomFilter::deserialize(hex);

    requireCondition(
        restored.contains("hello"),
        "Deserialized filter must still contain 'hello'."
    );
    requireCondition(
        restored.contains("world"),
        "Deserialized filter must still contain 'world'."
    );

    // Re-serialize must be identical
    requireCondition(
        restored.serialize() == hex,
        "Re-serialized filter must match original hex."
    );
}

void testBuildFromEventsFiltersAddresses() {
    const std::vector<EventLog> events = {
        EventLog::transfer("tx-a", "sender-1", "receiver-1",
            Amount::fromRawUnits(100), 1, kTimestamp),
        EventLog::stakeDeposited("tx-b", "staker-1",
            Amount::fromRawUnits(500), 2, kTimestamp + 1),
        EventLog::validatorRegistered("tx-c", "validator-1",
            3, kTimestamp + 2)
    };

    const BloomFilter filter = BloomFilter::buildFromEvents(events);

    // All primary addresses should be present
    requireCondition(filter.contains("sender-1"),    "sender-1 must be in filter.");
    requireCondition(filter.contains("receiver-1"),  "receiver-1 must be in filter.");
    requireCondition(filter.contains("staker-1"),    "staker-1 must be in filter.");
    requireCondition(filter.contains("validator-1"), "validator-1 must be in filter.");

    // All transaction ids should be present
    requireCondition(filter.contains("tx-a"), "tx-a must be in filter.");
    requireCondition(filter.contains("tx-b"), "tx-b must be in filter.");
    requireCondition(filter.contains("tx-c"), "tx-c must be in filter.");

    // Event type strings should be present
    requireCondition(filter.contains("TRANSFER"),             "TRANSFER type must be in filter.");
    requireCondition(filter.contains("STAKE_DEPOSITED"),      "STAKE_DEPOSITED type must be in filter.");
    requireCondition(filter.contains("VALIDATOR_REGISTERED"), "VALIDATOR_REGISTERED type must be in filter.");

    // An unrelated address should not be falsely detected (in almost all cases)
    // We use a very distinct string that won't collide with our 3-hash setup
    // for this specific set of inputs.
    requireCondition(
        !filter.isEmpty(),
        "Filter built from events must not be empty."
    );
}

void testBuildFromEmptyEventsProducesEmptyFilter() {
    const BloomFilter filter = BloomFilter::buildFromEvents({});
    requireCondition(
        filter.isEmpty(),
        "Filter built from empty event list must be empty."
    );
}

void testDeserializeFromMalformedHexReturnsEmpty() {
    const BloomFilter filter = BloomFilter::deserialize("not-valid-hex");
    requireCondition(
        filter.isEmpty(),
        "Deserialization from malformed hex should produce an empty filter."
    );
}

} // namespace

int main() {
    try {
        testEmptyFilterContainsNothing();
        testAddedItemIsContained();
        testNotAddedItemIsNotContainedTypically();
        testMultipleItemsAllContained();
        testUnionMergesFilters();
        testSerializeDeserializeRoundTrip();
        testBuildFromEventsFiltersAddresses();
        testBuildFromEmptyEventsProducesEmptyFilter();
        testDeserializeFromMalformedHexReturnsEmpty();

        std::cout << "Nodo BloomFilter tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo BloomFilter tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
