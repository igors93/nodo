#ifndef NODO_NODE_PROTOCOL_STATE_TRANSITION_HPP
#define NODO_NODE_PROTOCOL_STATE_TRANSITION_HPP

#include "config/NetworkParameters.hpp"
#include "core/AccountStateView.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/ValidatorRegistry.hpp"
#include "node/StakingRegistry.hpp"
#include "node/ProtocolTransactionDomainExecutor.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace nodo::node {

class NodeRuntime;

/*
 * Complete deterministic protocol state at a replay boundary.
 *
 * The account view and all consensus/economic domains are advanced together by
 * StateTransitionEngine.  Runtime reload, manifest verification, block import
 * and block production use this shape so a finalized block never has separate
 * account-only and domain-only replay paths.
 */
struct ProtocolReplayState {
    core::AccountStateView accounts;
    ProtocolExecutionState execution;
    core::ValidatorSetHistory validatorSetHistory;
    std::string stateRoot;
    std::string receiptsRoot;
    utils::Amount totalFee;
};

/*
 * Builds the complete deterministic post-block state used by block production,
 * validation, finalization, replay and manifest verification.
 */
class ProtocolStateTransition {
public:
    static core::AccountStateView initialAccountStateView(
        const config::GenesisConfig& genesisConfig
    );

    static ProtocolExecutionState initialExecutionState(
        const config::GenesisConfig& genesisConfig
    );

    static ProtocolReplayState initialReplayState(
        const config::GenesisConfig& genesisConfig
    );

    static ProtocolReplayState replayBlock(
        const config::GenesisConfig& genesisConfig,
        const ProtocolReplayState& previousState,
        const core::Block& block,
        std::int64_t minimumFeeRawUnits,
        std::int64_t wallClockNow = 0
    );

    static ProtocolReplayState replayToTip(
        const config::GenesisConfig& genesisConfig,
        const core::Blockchain& blockchain,
        std::int64_t minimumFeeRawUnits
    );

    static ProtocolReplayState replayNextBlock(
        const NodeRuntime& runtime,
        const core::Block& block,
        std::int64_t minimumFeeRawUnits,
        std::int64_t wallClockNow = 0
    );

    static core::StateTransitionPreviewContext contextFromReplayState(
        const config::GenesisConfig& genesisConfig,
        const ProtocolReplayState& state,
        std::int64_t minimumFeeRawUnits,
        std::shared_ptr<ProtocolExecutionState> resultTracker,
        std::int64_t wallClockNow = 0
    );

    // Returns the preview context together with a tracker for every protocol
    // domain changed by canonical transaction execution.
    static std::pair<core::StateTransitionPreviewContext, std::shared_ptr<ProtocolExecutionState>>
    contextForNextBlockWithState(
        const NodeRuntime& runtime,
        std::int64_t minimumFeeRawUnits,
        std::int64_t wallClockNow = 0
    );

    static core::StateTransitionPreviewContext contextForNextBlock(
        const NodeRuntime& runtime,
        std::int64_t minimumFeeRawUnits,
        std::int64_t wallClockNow = 0
    );

    static void applyReplayDomainsToRuntime(
        NodeRuntime& runtime,
        const ProtocolReplayState& state
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
