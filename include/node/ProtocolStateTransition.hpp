#ifndef NODO_NODE_PROTOCOL_STATE_TRANSITION_HPP
#define NODO_NODE_PROTOCOL_STATE_TRANSITION_HPP

#include "core/AccountStateView.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/ValidatorRegistry.hpp"
#include "utils/Amount.hpp"

#include <cstdint>

namespace nodo::node {

class NodeRuntime;

/*
 * Builds the complete deterministic post-block state used by block production,
 * validation, finalization, replay and manifest verification.
 */
class ProtocolStateTransition {
public:
    static core::StateTransitionPreviewContext contextForNextBlock(
        const NodeRuntime& runtime,
        std::int64_t minimumFeeRawUnits,
        std::int64_t wallClockNow = 0
    );

    static core::DeterministicStateDomainTransition accountSettlementForReplay(
        std::uint64_t blockHeight
    );

    static core::AccountStateView settleFees(
        const core::AccountStateView& accounts,
        std::uint64_t blockHeight,
        utils::Amount totalFee
    );

    static void applyValidatorEpochTransition(
        core::ValidatorRegistry& validators,
        std::uint64_t blockHeight,
        std::int64_t blockTimestamp
    );
};

} // namespace nodo::node

#endif
