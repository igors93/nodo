#include "node/RuntimeAccountStateBuilder.hpp"

#include "node/NodeRuntime.hpp"
#include "node/ProtocolStateTransition.hpp"

#include <memory>
#include <stdexcept>

namespace nodo::node {

core::AccountStateView RuntimeAccountStateBuilder::initialAccountStateView(
    const config::GenesisConfig& genesisConfig
) {
    return ProtocolStateTransition::initialAccountStateView(genesisConfig);
}

core::AccountStateView RuntimeAccountStateBuilder::accountStateViewAtTip(
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain,
    std::int64_t minimumFeeRawUnits
) {
    return ProtocolStateTransition::replayToTip(
        genesisConfig,
        blockchain,
        minimumFeeRawUnits
    ).accounts;
}

core::AccountStateView RuntimeAccountStateBuilder::accountStateViewFromSnapshot(
    const config::GenesisConfig& genesisConfig,
    const core::AccountStateView& snapshotView,
    const core::Blockchain& blockchain,
    std::uint64_t snapshotHeight,
    std::int64_t minimumFeeRawUnits
) {
    (void)snapshotView;
    (void)snapshotHeight;

    // Account-only snapshots do not contain governance, staking, validator,
    // supply or slashing domains.  A trusted protocol state root can therefore
    // only be rebuilt through the unified replay path that advances accounts and
    // domains together from genesis.  The snapshot parameters are retained for
    // callers that still use this helper as an account cache hint, but they are
    // never allowed to bypass canonical replay.
    return accountStateViewAtTip(
        genesisConfig,
        blockchain,
        minimumFeeRawUnits
    );
}

core::StateTransitionPreviewContext RuntimeAccountStateBuilder::previewContextAtTip(
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    auto replayState = ProtocolStateTransition::replayToTip(
        genesisConfig,
        blockchain,
        minimumFeeRawUnits
    );
    auto tracker = std::make_shared<ProtocolExecutionState>(replayState.execution);
    return ProtocolStateTransition::contextFromReplayState(
        genesisConfig,
        replayState,
        minimumFeeRawUnits,
        tracker,
        wallClockNow
    );
}

core::StateTransitionPreviewContext RuntimeAccountStateBuilder::previewContextAtTip(
    const NodeRuntime& runtime,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    return ProtocolStateTransition::contextForNextBlock(
        runtime, minimumFeeRawUnits, wallClockNow
    );
}

} // namespace nodo::node
