#include "core/StatePruner.hpp"

namespace nodo::core {

StatePruner::StatePruner(size_t keepRecentStatesCount)
    : m_keepRecentStatesCount(keepRecentStatesCount) {}

void StatePruner::recordStateRoot(uint64_t blockHeight, const std::string& stateRoot) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_historicalStateRoots[blockHeight] = stateRoot;
}

void StatePruner::pruneHistory(uint64_t currentBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (currentBlock <= m_keepRecentStatesCount) {
        return;
    }
    uint64_t pruneThreshold = currentBlock - m_keepRecentStatesCount;

    auto it = m_historicalStateRoots.begin();
    while (it != m_historicalStateRoots.end() && it->first < pruneThreshold) {
        it = m_historicalStateRoots.erase(it);
        m_prunedCount++;
    }
}

bool StatePruner::hasStateRoot(uint64_t blockHeight) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_historicalStateRoots.find(blockHeight) != m_historicalStateRoots.end();
}

std::string StatePruner::getStateRoot(uint64_t blockHeight) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_historicalStateRoots.find(blockHeight);
    return (it != m_historicalStateRoots.end()) ? it->second : "";
}

size_t StatePruner::prunedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_prunedCount;
}

} // namespace nodo::core
