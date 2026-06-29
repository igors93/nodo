#ifndef NODO_NODE_PROTOCOL_STATE_TRANSITION_HPP
#define NODO_NODE_PROTOCOL_STATE_TRANSITION_HPP

#include "core/AccountStateView.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/ValidatorRegistry.hpp"
#include "node/StakingRegistry.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace nodo::node {

class NodeRuntime;

/*
 * Builds the complete deterministic post-block state used by block production,
 * validation, finalization, replay and manifest verification.
 */
class ProtocolStateTransition {
public:
    // Returns the preview context together with a shared StakingRegistry that
    // reflects the post-block staking state after the domain transition runs.
    // Callers that need to write the updated registry back to the runtime
    // (e.g. applyCertifiedBlock) should use this form.
    static std::pair<core::StateTransitionPreviewContext, std::shared_ptr<StakingRegistry>>
    contextForNextBlockWithRegistry(
        const NodeRuntime& runtime,
        std::int64_t minimumFeeRawUnits,
        std::int64_t wallClockNow = 0
    );

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
