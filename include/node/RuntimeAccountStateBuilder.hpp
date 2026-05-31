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

    static core::StateTransitionPreviewContext previewContextAtTip(
        const config::GenesisConfig& genesisConfig,
        const core::Blockchain& blockchain,
        std::int64_t minimumFeeRawUnits
    );
};

} // namespace nodo::node

#endif
