#include "node/SeenTransactionCache.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::node::SeenTransactionCache;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testNewTxIsMarkedSeen() {
    SeenTransactionCache cache;
    const bool isNew = cache.markSeen("tx-abc", 1000);
    requireCondition(isNew, "First markSeen should return true (newly seen).");
    requireCondition(cache.size() == 1, "Cache size should be 1 after first insert.");
}

void testDuplicateTxIsNotNew() {
    SeenTransactionCache cache;
    cache.markSeen("tx-abc", 1000);
    const bool isNew = cache.markSeen("tx-abc", 1001);
    requireCondition(!isNew, "Duplicate markSeen should return false (already seen).");
    requireCondition(cache.size() == 1, "Cache size should remain 1 after duplicate.");
}

void testHasSeen() {
    SeenTransactionCache cache;
    requireCondition(!cache.hasSeen("tx-xyz", 1000), "hasSeen should be false before insert.");
    cache.markSeen("tx-xyz", 1000);
    requireCondition(cache.hasSeen("tx-xyz", 1001), "hasSeen should be true after insert.");
}

void testTtlExpiry() {
    SeenTransactionCache cache(16, 60);

    cache.markSeen("tx-old", 1000);

    // Within TTL: still seen
    requireCondition(cache.hasSeen("tx-old", 1059), "Should be seen within TTL.");

    // After TTL: treated as expired
    requireCondition(!cache.hasSeen("tx-old", 1061), "Should be expired after TTL.");

    // markSeen after TTL: should be treated as new
    const bool isNew = cache.markSeen("tx-old", 1070);
    requireCondition(isNew, "markSeen after TTL should return true (new again).");
}

void testLruEviction() {
    SeenTransactionCache cache(3, 600);

    cache.markSeen("tx-1", 1000);
    cache.markSeen("tx-2", 1001);
    cache.markSeen("tx-3", 1002);

    requireCondition(cache.size() == 3, "Size should be 3 at capacity.");

    // Insert a 4th entry — should evict the LRU (tx-1).
    const bool isNew = cache.markSeen("tx-4", 1003);
    requireCondition(isNew, "tx-4 should be new.");
    requireCondition(cache.size() == 3, "Size should remain capped at maxEntries.");

    // tx-1 should have been evicted
    requireCondition(!cache.hasSeen("tx-1", 1003) || cache.hasSeen("tx-4", 1003),
        "LRU entry (tx-1) should be evicted, tx-4 should be present.");
}

void testEvictExpired() {
    SeenTransactionCache cache(16, 60);

    cache.markSeen("tx-a", 1000);
    cache.markSeen("tx-b", 1000);
    // tx-c inserted at 1270: age at 1300 = 30 < 60 (within TTL)
    cache.markSeen("tx-c", 1270);

    cache.evictExpired(1300); // tx-a and tx-b: age=300>60 expired; tx-c: age=30 survives

    requireCondition(cache.size() == 1, "Only tx-c should remain after eviction.");
    requireCondition(cache.hasSeen("tx-c", 1300), "tx-c should still be present.");
}

void testMaxEntriesDefault() {
    SeenTransactionCache cache;
    requireCondition(
        cache.maxEntries() == SeenTransactionCache::DEFAULT_MAX_ENTRIES,
        "Default max entries should match DEFAULT_MAX_ENTRIES."
    );
}

void testEmptyIdNotInserted() {
    SeenTransactionCache cache;
    const bool isNew = cache.markSeen("", 1000);
    requireCondition(!isNew, "Empty txId should not be marked as new.");
    requireCondition(cache.size() == 0, "Cache should remain empty after empty txId.");
}

void testCacheSizeBound() {
    const std::size_t maxEntries = 100;
    SeenTransactionCache cache(maxEntries, 600);

    for (std::size_t i = 0; i < maxEntries + 50; ++i) {
        cache.markSeen("tx-" + std::to_string(i), static_cast<std::int64_t>(1000 + i));
    }

    requireCondition(
        cache.size() <= maxEntries,
        "Cache size must never exceed maxEntries."
    );
}

} // namespace

int main() {
    try {
        testNewTxIsMarkedSeen();
        testDuplicateTxIsNotNew();
        testHasSeen();
        testTtlExpiry();
        testLruEviction();
        testEvictExpired();
        testMaxEntriesDefault();
        testEmptyIdNotInserted();
        testCacheSizeBound();

        std::cout << "SeenTransactionCache tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SeenTransactionCache tests failed: " << error.what() << "\n";
        return 1;
    }
}
