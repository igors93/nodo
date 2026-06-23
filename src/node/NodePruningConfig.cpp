#include "node/NodePruningConfig.hpp"

#include <sstream>

namespace nodo::node {

std::string nodePruningModeToString(NodePruningMode mode) {
    switch (mode) {
        case NodePruningMode::ARCHIVE: return "ARCHIVE";
        case NodePruningMode::FULL:    return "FULL";
        case NodePruningMode::LIGHT:   return "LIGHT";
        default:                       return "ARCHIVE";
    }
}

NodePruningConfig::NodePruningConfig()
    : m_mode(NodePruningMode::ARCHIVE),
      m_retainEpochs(0) {}

NodePruningConfig::NodePruningConfig(NodePruningMode mode, std::size_t retainEpochs)
    : m_mode(mode),
      m_retainEpochs(retainEpochs) {}

NodePruningConfig NodePruningConfig::archiveMode() {
    return NodePruningConfig(NodePruningMode::ARCHIVE, 0);
}

NodePruningConfig NodePruningConfig::fullMode(std::size_t retainEpochs) {
    return NodePruningConfig(NodePruningMode::FULL, retainEpochs);
}

NodePruningConfig NodePruningConfig::lightMode() {
    return NodePruningConfig(NodePruningMode::LIGHT, 0);
}

NodePruningMode NodePruningConfig::mode() const {
    return m_mode;
}

std::size_t NodePruningConfig::retainEpochs() const {
    return m_retainEpochs;
}

bool NodePruningConfig::shouldPruneBlockAtHeight(
    std::uint64_t blockHeight,
    std::uint64_t currentHeight,
    std::uint64_t epochDurationBlocks
) const {
    switch (m_mode) {
        case NodePruningMode::ARCHIVE:
            return false;

        case NodePruningMode::FULL: {
            if (m_retainEpochs == 0 || epochDurationBlocks == 0) {
                return false;
            }
            const std::uint64_t retainBlocks =
                static_cast<std::uint64_t>(m_retainEpochs) * epochDurationBlocks;

            if (currentHeight < retainBlocks) {
                return false;
            }
            return blockHeight < (currentHeight - retainBlocks);
        }

        case NodePruningMode::LIGHT:
            // Keep only the current tip (currentHeight itself); prune everything older
            return blockHeight < currentHeight;

        default:
            return false;
    }
}

bool NodePruningConfig::shouldPruneStateAtHeight(
    std::uint64_t blockHeight,
    std::uint64_t currentHeight,
    std::uint64_t epochDurationBlocks
) const {
    // State pruning policy mirrors block pruning policy
    return shouldPruneBlockAtHeight(blockHeight, currentHeight, epochDurationBlocks);
}

std::uint64_t NodePruningConfig::retainFromHeight(
    std::uint64_t currentHeight,
    std::uint64_t epochDurationBlocks
) const {
    switch (m_mode) {
        case NodePruningMode::ARCHIVE:
            return 0;

        case NodePruningMode::FULL: {
            if (m_retainEpochs == 0 || epochDurationBlocks == 0) {
                return 0;
            }
            const std::uint64_t retainBlocks =
                static_cast<std::uint64_t>(m_retainEpochs) * epochDurationBlocks;

            if (currentHeight < retainBlocks) {
                return 0;
            }
            return currentHeight - retainBlocks;
        }

        case NodePruningMode::LIGHT:
            return currentHeight;

        default:
            return 0;
    }
}

bool NodePruningConfig::isValid() const {
    if (m_mode == NodePruningMode::FULL) {
        return m_retainEpochs > 0;
    }
    return true;
}

std::string NodePruningConfig::serialize() const {
    std::ostringstream oss;
    oss << "NodePruningConfig{"
        << "mode=" << nodePruningModeToString(m_mode)
        << ";retainEpochs=" << m_retainEpochs
        << "}";
    return oss.str();
}

NodePruningConfig NodePruningConfig::deserialize(const std::string& serialized) {
    // Minimal parser: find mode= and retainEpochs= in the serialized string.
    // This is a best-effort deserializer that mirrors our serialize() format.

    NodePruningMode mode = NodePruningMode::ARCHIVE;
    std::size_t retainEpochs = 0;

    const auto findValue = [&](const std::string& key) -> std::string {
        const std::size_t keyPos = serialized.find(key + "=");
        if (keyPos == std::string::npos) {
            return "";
        }
        const std::size_t valueStart = keyPos + key.size() + 1;
        const std::size_t valueEnd = serialized.find_first_of(";}", valueStart);
        if (valueEnd == std::string::npos) {
            return serialized.substr(valueStart);
        }
        return serialized.substr(valueStart, valueEnd - valueStart);
    };

    const std::string modeStr = findValue("mode");
    if (modeStr == "FULL") {
        mode = NodePruningMode::FULL;
    } else if (modeStr == "LIGHT") {
        mode = NodePruningMode::LIGHT;
    } else {
        mode = NodePruningMode::ARCHIVE;
    }

    const std::string epochsStr = findValue("retainEpochs");
    if (!epochsStr.empty()) {
        try {
            retainEpochs = static_cast<std::size_t>(std::stoull(epochsStr));
        } catch (...) {
            retainEpochs = 0;
        }
    }

    return NodePruningConfig(mode, retainEpochs);
}

} // namespace nodo::node
