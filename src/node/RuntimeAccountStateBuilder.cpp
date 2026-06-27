#include "node/RuntimeAccountStateBuilder.hpp"

#include "core/StateTransitionPreview.hpp"
#include "node/NodeRuntime.hpp"
#include "node/FeeEconomics.hpp"

#include <stdexcept>
#include <map>
#include <utility>

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
            true,
            "",
            0,
            genesisConfig.networkParameters().chainId(),
            genesisConfig.networkParameters().networkName()
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

core::AccountStateView RuntimeAccountStateBuilder::accountStateViewFromSnapshot(
    const core::AccountStateView& snapshotView,
    const core::Blockchain& blockchain,
    std::uint64_t snapshotHeight,
    std::int64_t minimumFeeRawUnits
) {
    if (minimumFeeRawUnits < 0) {
        throw std::invalid_argument("Minimum fee cannot be negative when rebuilding from snapshot.");
    }

    if (!blockchain.isValid()) {
        throw std::invalid_argument("Cannot rebuild from snapshot with invalid blockchain.");
    }

    core::AccountStateView view = snapshotView;

    for (const core::Block& block : blockchain.blocks()) {
        if (block.isGenesisBlock() || block.index() <= snapshotHeight) {
            continue;
        }

        const core::StateTransitionPreviewContext context(
            minimumFeeRawUnits,
            view,
            false,
            true,
            "",
            0,
            genesisConfig.networkParameters().chainId(),
            genesisConfig.networkParameters().networkName()
        );

        const core::StateTransitionPreviewResult preview =
            core::StateTransitionPreview::previewBlock(block, context);

        if (!preview.accepted()) {
            throw std::logic_error(
                "Cannot rebuild account state from snapshot: " + preview.reason()
            );
        }

        core::AccountStateView nextView;
        for (const core::AccountState& account : preview.resultingAccounts()) {
            if (!nextView.putAccount(account)) {
                throw std::logic_error("Preview produced invalid account state during snapshot replay.");
            }
        }
        view = nextView;
    }

    return view;
}

core::StateTransitionPreviewContext RuntimeAccountStateBuilder::previewContextAtTip(
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    return core::StateTransitionPreviewContext(
        minimumFeeRawUnits,
        accountStateViewAtTip(
            genesisConfig,
            blockchain,
            minimumFeeRawUnits
        ),
        false,
        true,
        "",
        wallClockNow,
        genesisConfig.networkParameters().chainId(),
        genesisConfig.networkParameters().networkName()
    );
}

core::StateTransitionPreviewContext RuntimeAccountStateBuilder::previewContextAtTip(
    const NodeRuntime& runtime,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    const utils::Amount supplyBefore = runtime.supplyState().latestSupply();
    const std::uint64_t nextBlockHeight = runtime.blockchain().size();
    std::map<std::string, std::string> domains = {
        {"governance", runtime.governanceExecutor().serialize()},
        {"supply", "RuntimeSupply{latestRawUnits=" + std::to_string(supplyBefore.rawUnits()) + "}"},
        {"validators", runtime.validatorRegistry().serialize()}
    };
    const core::DeterministicStateDomainTransition domainTransition =
        [domains, supplyBefore, nextBlockHeight](utils::Amount totalFee) mutable {
            const utils::Amount burn = FeeEconomics::buildFeeEconomicBalance(
                nextBlockHeight, totalFee
            ).burnAmount();
            const utils::Amount supplyAfter = supplyBefore - burn;
            domains["supply"] =
                "RuntimeSupply{latestRawUnits="
                + std::to_string(supplyAfter.rawUnits()) + "}";
            return domains;
        };
    const config::GenesisConfig& genesisConfig = runtime.config().genesisConfig();
    return core::StateTransitionPreviewContext(
        minimumFeeRawUnits,
        accountStateViewAtTip(genesisConfig, runtime.blockchain(), minimumFeeRawUnits),
        false,
        true,
        "",
        wallClockNow,
        genesisConfig.networkParameters().chainId(),
        genesisConfig.networkParameters().networkName(),
        std::move(domains),
        domainTransition
    );
}

} // namespace nodo::node
