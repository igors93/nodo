#ifndef NODO_CORE_STATE_PRUNER_HPP
#define NODO_CORE_STATE_PRUNER_HPP

#include <cstdint>
#include <map>
#include <string>

namespace nodo::core {

/*
 * StatePruner maintains a rolling window of (blockHeight → stateRoot) entries.
 * It is not thread-safe; callers must synchronize externally.
 *
 * Used by the sync layer so that recent state roots can be looked up without
 * replaying the full chain from genesis.
 */
class StatePruner {
public:
    explicit StatePruner(std::size_t keepRecentStatesCount = 100);

    void recordStateRoot(std::uint64_t blockHeight, const std::string& stateRoot);
    void pruneHistory(std::uint64_t currentBlock);

    bool hasStateRoot(std::uint64_t blockHeight) const;
    std::string getStateRoot(std::uint64_t blockHeight) const;

    std::size_t prunedCount() const;

private:
    std::size_t m_keepRecentStatesCount;
    std::map<std::uint64_t, std::string> m_historicalStateRoots;
    std::size_t m_prunedCount{0};
};

} // namespace nodo::core

#endif
