#include "node/RuntimeAccountStateBuilder.hpp"

#include "core/StateTransitionPreview.hpp"

#include <stdexcept>

namespace nodo::node {

core::AccountStateView RuntimeAccountStateBuilder::initialAccountStateView(
    const config::GenesisConfig& genesisConfig
) {
    core::AccountStateView view;

    if (!genesisConfig.isValid()) {
        throw std::invalid_argument("Cannot build account state from invalid genesis config.");
    }

    for (const config::GenesisAccountConfig& account :
         genesisConfig.genesisAccounts()) {
        if (!view.putAccount(
                core::AccountState(
                    account.address(),
                    account.balance(),
                    account.nonce()
                )
            )) {
            throw std::logic_error("Genesis account allocation produced invalid account state.");
        }
    }

    return view;
}

core::AccountStateView RuntimeAccountStateBuilder::accountStateViewAtTip(
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain,
    std::int64_t minimumFeeRawUnits
) {
    if (minimumFeeRawUnits < 0) {
        throw std::invalid_argument("Minimum fee cannot be negative when rebuilding account state.");
    }

    if (!blockchain.isValid()) {
        throw std::invalid_argument("Cannot rebuild account state from invalid blockchain.");
    }

    core::AccountStateView view =
        initialAccountStateView(genesisConfig);

    for (const core::Block& block : blockchain.blocks()) {
        if (block.isGenesisBlock()) {
            continue;
        }

        const core::StateTransitionPreviewContext context(
            minimumFeeRawUnits,
            view,
            false,
            true
        );

        const core::StateTransitionPreviewResult preview =
            core::StateTransitionPreview::previewBlock(
                block,
                context
            );

        if (!preview.accepted()) {
            throw std::logic_error(
                "Cannot rebuild account state from finalized chain: "
                + preview.reason()
            );
        }

        core::AccountStateView nextView;

        for (const core::AccountState& account : preview.resultingAccounts()) {
            if (!nextView.putAccount(account)) {
                throw std::logic_error("Preview produced invalid account state during rebuild.");
            }
        }

        view = nextView;
    }

    return view;
}

core::StateTransitionPreviewContext RuntimeAccountStateBuilder::previewContextAtTip(
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain,
    std::int64_t minimumFeeRawUnits
) {
    return core::StateTransitionPreviewContext(
        minimumFeeRawUnits,
        accountStateViewAtTip(
            genesisConfig,
            blockchain,
            minimumFeeRawUnits
        ),
        false,
        true
    );
}

} // namespace nodo::node
