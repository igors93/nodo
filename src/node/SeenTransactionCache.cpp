#include "node/SeenTransactionCache.hpp"

namespace nodo::node {

SeenTransactionCache::SeenTransactionCache(
    std::size_t  maxEntries,
    std::int64_t ttlSeconds
)
    : m_maxEntries(maxEntries > 0 ? maxEntries : DEFAULT_MAX_ENTRIES)
    , m_ttlSeconds(ttlSeconds  > 0 ? ttlSeconds  : DEFAULT_TTL_SECONDS)
{}

bool SeenTransactionCache::markSeen(
    const std::string& txId,
    std::int64_t       now
) {
    if (txId.empty()) {
        return false;
    }

    const auto found = m_index.find(txId);

    if (found != m_index.end()) {
        const std::int64_t age = now - found->second.second;
        if (age <= m_ttlSeconds) {
            // Move to front (most recently seen).
            m_lruList.splice(m_lruList.begin(), m_lruList, found->second.first);
            found->second.second = now;
            return false; // already seen
        }
        // TTL expired — treat as unseen, remove stale entry.
        m_lruList.erase(found->second.first);
        m_index.erase(found);
    }

    // Evict LRU entry if at capacity.
    if (m_index.size() >= m_maxEntries) {
        evictOldest();
    }

    m_lruList.push_front(txId);
    m_index.emplace(txId, std::make_pair(m_lruList.begin(), now));
    return true; // newly seen
}

bool SeenTransactionCache::hasSeen(
    const std::string& txId,
    std::int64_t       now
) const {
    const auto found = m_index.find(txId);
    if (found == m_index.end()) {
        return false;
    }
    const std::int64_t age = now - found->second.second;
    return age <= m_ttlSeconds;
}

void SeenTransactionCache::evictExpired(std::int64_t now) {
    auto it = m_lruList.end();
    while (it != m_lruList.begin()) {
        --it;
        const auto found = m_index.find(*it);
        if (found == m_index.end()) {
            it = m_lruList.erase(it);
            continue;
        }
        const std::int64_t age = now - found->second.second;
        if (age > m_ttlSeconds) {
            m_index.erase(found);
            it = m_lruList.erase(it);
        }
    }
}

std::size_t SeenTransactionCache::size() const {
    return m_index.size();
}

std::size_t SeenTransactionCache::maxEntries() const {
    return m_maxEntries;
}

void SeenTransactionCache::evictOldest() {
    if (m_lruList.empty()) {
        return;
    }
    const std::string& oldest = m_lruList.back();
    m_index.erase(oldest);
    m_lruList.pop_back();
}

} // namespace nodo::node
