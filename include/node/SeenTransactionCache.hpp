#ifndef NODO_NODE_SEEN_TRANSACTION_CACHE_HPP
#define NODO_NODE_SEEN_TRANSACTION_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>

namespace nodo::node {

/*
 * SeenTransactionCache tracks transaction IDs that have been seen from the
 * network, to prevent re-broadcasting transactions already propagated.
 *
 * Security: bounded by maxEntries to prevent unbounded memory growth.
 * Eviction: LRU when at capacity; stale entries also expire after ttlSeconds.
 */
class SeenTransactionCache {
public:
    static constexpr std::size_t  DEFAULT_MAX_ENTRIES  = 65536;
    static constexpr std::int64_t DEFAULT_TTL_SECONDS  = 600;

    explicit SeenTransactionCache(
        std::size_t  maxEntries = DEFAULT_MAX_ENTRIES,
        std::int64_t ttlSeconds = DEFAULT_TTL_SECONDS
    );

    // Returns true if txId was NOT yet seen (i.e., it is new and should be
    // propagated). Marks it as seen. Thread-unsafe — call under external lock.
    bool markSeen(const std::string& txId, std::int64_t now);

    // Returns true if txId has already been seen and is still in cache.
    bool hasSeen(const std::string& txId, std::int64_t now) const;

    // Evict all entries whose TTL has expired relative to `now`.
    void evictExpired(std::int64_t now);

    std::size_t size()       const;
    std::size_t maxEntries() const;

private:
    struct Entry {
        std::int64_t seenAt;
    };

    void evictOldest();

    std::size_t  m_maxEntries;
    std::int64_t m_ttlSeconds;

    // LRU list: front = most-recently-used
    std::list<std::string>                                   m_lruList;
    std::unordered_map<std::string, std::pair<
        std::list<std::string>::iterator, std::int64_t>>     m_index;
};

} // namespace nodo::node

#endif
