#include "node/RuntimeSupplyState.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

RuntimeSupplyState::RuntimeSupplyState()
    : m_latestSupply(utils::Amount::fromRawUnits(0)) {}

RuntimeSupplyState::RuntimeSupplyState(utils::Amount genesisSupply)
    : m_latestSupply(genesisSupply) {}

utils::Amount RuntimeSupplyState::latestSupply() const {
    return m_latestSupply;
}

std::uint64_t RuntimeSupplyState::finalizedDeltaCount() const {
    return static_cast<std::uint64_t>(m_finalizedDeltas.size());
}

const std::vector<economics::SupplyDelta>& RuntimeSupplyState::finalizedDeltas() const {
    return m_finalizedDeltas;
}

void RuntimeSupplyState::applyFinalizedDelta(economics::SupplyDelta delta) {
    if (!delta.isValid()) {
        throw std::invalid_argument(
            "RuntimeSupplyState::applyFinalizedDelta: invalid delta: " +
            delta.rejectionReason()
        );
    }
    if (delta.supplyBefore() != m_latestSupply) {
        throw std::invalid_argument(
            "RuntimeSupplyState::applyFinalizedDelta: supply continuity break — "
            "delta.supplyBefore (" + std::to_string(delta.supplyBefore().rawUnits()) +
            ") does not equal latestSupply (" +
            std::to_string(m_latestSupply.rawUnits()) + ")."
        );
    }
    m_latestSupply = delta.supplyAfter();
    m_finalizedDeltas.push_back(std::move(delta));
}

std::string RuntimeSupplyState::serialize() const {
    std::ostringstream oss;
    oss << "RuntimeSupplyState{"
        << "latestSupplyRaw=" << m_latestSupply.rawUnits()
        << ";deltaCount=" << m_finalizedDeltas.size()
        << "}";
    return oss.str();
}

} // namespace nodo::node
