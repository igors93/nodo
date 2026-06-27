#ifndef NODO_NODE_RUNTIME_ACCOUNT_STATE_BUILDER_HPP
#define NODO_NODE_RUNTIME_ACCOUNT_STATE_BUILDER_HPP

#include "config/NetworkParameters.hpp"
#include "core/AccountStateView.hpp"
#include "core/Blockchain.hpp"
#include "core/StateTransitionPreviewContext.hpp"

#include <cstdint>

namespace nodo::node {

class RuntimeAccountStateBuilder {
public:
    static core::AccountStateView initialAccountStateView(
        const config::GenesisConfig& genesisConfig
    );

    static core::AccountStateView accountStateViewAtTip(
        const config::GenesisConfig& genesisConfig,
        const core::Blockchain& blockchain,
        std::int64_t minimumFeeRawUnits
    );

    // Replay only blocks with index > snapshotHeight starting from snapshotView.
    // Avoids O(N) full replay when a fresh snapshot is available.
    static core::AccountStateView accountStateViewFromSnapshot(
        const core::AccountStateView& snapshotView,
        const core::Blockchain& blockchain,
        std::uint64_t snapshotHeight,
        std::int64_t minimumFeeRawUnits
    );

    static core::StateTransitionPreviewContext previewContextAtTip(
        const config::GenesisConfig& genesisConfig,
        const core::Blockchain& blockchain,
        std::int64_t minimumFeeRawUnits,
        std::int64_t wallClockNow = 0
    );
};

} // namespace nodo::node

#endif
