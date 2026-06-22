#ifndef NODO_CORE_STATE_PRUNER_HPP
#define NODO_CORE_STATE_PRUNER_HPP

#include <string>
#include <map>
#include <mutex>
#include <cstdint>

namespace nodo::core {

class StatePruner {
public:
    explicit StatePruner(size_t keepRecentStatesCount = 100);

    void pruneHistory(uint64_t currentBlock);

    void recordStateRoot(uint64_t blockHeight, const std::string& stateRoot);
    bool hasStateRoot(uint64_t blockHeight) const;
    std::string getStateRoot(uint64_t blockHeight) const;

    size_t prunedCount() const;

private:
    size_t m_keepRecentStatesCount;
    std::map<uint64_t, std::string> m_historicalStateRoots;
    size_t m_prunedCount{0};
    mutable std::mutex m_mutex;
};

} // namespace nodo::core

#endif
