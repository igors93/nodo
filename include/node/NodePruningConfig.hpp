#ifndef NODO_NODE_NODE_PRUNING_CONFIG_HPP
#define NODO_NODE_NODE_PRUNING_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace nodo::node {

enum class NodePruningMode {
    ARCHIVE,  // Keep ALL block and state history. No pruning ever.
    FULL,     // Keep last N epochs of full state. Prune older state roots.
    LIGHT     // Keep only the current state tip. No block bodies for old blocks.
};

std::string nodePruningModeToString(NodePruningMode mode);

/*
 * NodePruningConfig controls how much historical data a node retains.
 *
 * Security principle:
 * Archive nodes provide the strongest auditability guarantee.
 * Full nodes balance storage cost with the ability to serve recent history.
 * Light nodes are constrained clients and cannot serve historical block data.
 */
class NodePruningConfig {
public:
    NodePruningConfig();

    static NodePruningConfig archiveMode();
    static NodePruningConfig fullMode(std::size_t retainEpochs);
    static NodePruningConfig lightMode();

    NodePruningMode mode() const;
    std::size_t retainEpochs() const;  // only meaningful for FULL mode

    // Returns true if the block at blockHeight should be pruned.
    // For ARCHIVE: always false
    // For FULL: true if blockHeight < currentHeight - (retainEpochs * epochDurationBlocks)
    // For LIGHT: true if blockHeight < currentHeight - 1
    bool shouldPruneBlockAtHeight(
        std::uint64_t blockHeight,
        std::uint64_t currentHeight,
        std::uint64_t epochDurationBlocks = 1000
    ) const;

    // Returns true if state at blockHeight should be pruned.
    // Same policy as block pruning.
    bool shouldPruneStateAtHeight(
        std::uint64_t blockHeight,
        std::uint64_t currentHeight,
        std::uint64_t epochDurationBlocks = 1000
    ) const;

    // Minimum height to retain (everything below can be pruned).
    // Returns 0 for ARCHIVE mode.
    std::uint64_t retainFromHeight(
        std::uint64_t currentHeight,
        std::uint64_t epochDurationBlocks = 1000
    ) const;

    bool isValid() const;
    std::string serialize() const;
    static NodePruningConfig deserialize(const std::string& serialized);

private:
    explicit NodePruningConfig(NodePruningMode mode, std::size_t retainEpochs);

    NodePruningMode m_mode;
    std::size_t m_retainEpochs;
};

} // namespace nodo::node

#endif
