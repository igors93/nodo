#include "core/StatePruner.hpp"

namespace nodo::core {

StatePruner::StatePruner(std::size_t keepRecentStatesCount)
    : m_keepRecentStatesCount(keepRecentStatesCount),
      m_prunedCount(0) {}

void StatePruner::recordStateRoot(std::uint64_t blockHeight, const std::string& stateRoot) {
    m_historicalStateRoots[blockHeight] = stateRoot;
}

void StatePruner::pruneHistory(std::uint64_t currentBlock) {
    if (currentBlock <= m_keepRecentStatesCount) {
        return;
    }
    const std::uint64_t pruneThreshold = currentBlock - m_keepRecentStatesCount;
    auto it = m_historicalStateRoots.begin();
    while (it != m_historicalStateRoots.end() && it->first < pruneThreshold) {
        it = m_historicalStateRoots.erase(it);
        ++m_prunedCount;
    }
}

bool StatePruner::hasStateRoot(std::uint64_t blockHeight) const {
    return m_historicalStateRoots.find(blockHeight) != m_historicalStateRoots.end();
}

std::string StatePruner::getStateRoot(std::uint64_t blockHeight) const {
    const auto it = m_historicalStateRoots.find(blockHeight);
    return (it != m_historicalStateRoots.end()) ? it->second : "";
}

std::size_t StatePruner::prunedCount() const {
    return m_prunedCount;
}

} // namespace nodo::core
