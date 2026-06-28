#include "node/RuntimeAccountStateBuilder.hpp"

#include "core/StateTransitionPreview.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtocolStateTransition.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::int64_t replayMinimumFee(
    const GovernanceExecutor& governance,
    std::int64_t genesisMinimumFee
) {
    const std::string governed = governance.currentValueForTarget(
        GovernanceParameterTarget::MINIMUM_FEE_RAW
    );
    if (governed.empty()) {
        return genesisMinimumFee;
    }
    const std::uint64_t raw = std::stoull(governed);
    if (raw > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error(
            "Governed minimum fee exceeds supported Amount range."
        );
    }
    return static_cast<std::int64_t>(raw);
}

void replayGovernanceBlock(
    GovernanceExecutor& governance,
    const core::Block& block
) {
    for (const core::LedgerRecord& record : block.records()) {
        if (record.type() != core::LedgerRecordType::TRANSACTION) continue;
        const core::Transaction transaction =
            core::Transaction::deserialize(record.payload());
        if (transaction.type() != core::TransactionType::GOVERNANCE_PROPOSE) {
            continue;
        }
        const GovernanceExecutionResult result = governance.executeProposal(
            transaction.id(),
            transaction.toAddress(),
            block.index(),
            block.timestamp()
        );
        if (!result.isApplied() && !result.isPending()) {
            throw std::logic_error(
                "Governance replay failed: " + result.detail()
            );
        }
    }
    governance.advanceToHeight(block.index() + 1, block.timestamp());
}

} // namespace

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
    GovernanceExecutor governance;

    for (const core::Block& block : blockchain.blocks()) {
        if (block.isGenesisBlock()) {
            continue;
        }

        const std::int64_t blockMinimumFee =
            replayMinimumFee(governance, minimumFeeRawUnits);
        const core::StateTransitionPreviewContext context(
            blockMinimumFee,
            view,
            false,
            true,
            "",
            0,
            genesisConfig.networkParameters().chainId(),
            genesisConfig.networkParameters().networkName(),
            {},
            ProtocolStateTransition::accountSettlementForReplay(block.index())
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
        replayGovernanceBlock(governance, block);
    }

    return view;
}

core::AccountStateView RuntimeAccountStateBuilder::accountStateViewFromSnapshot(
    const config::GenesisConfig& genesisConfig,
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
    GovernanceExecutor governance;

    for (const core::Block& block : blockchain.blocks()) {
        if (block.isGenesisBlock()) {
            continue;
        }

        const std::int64_t blockMinimumFee =
            replayMinimumFee(governance, minimumFeeRawUnits);
        if (block.index() <= snapshotHeight) {
            replayGovernanceBlock(governance, block);
            continue;
        }

        const core::StateTransitionPreviewContext context(
            blockMinimumFee,
            view,
            false,
            true,
            "",
            0,
            genesisConfig.networkParameters().chainId(),
            genesisConfig.networkParameters().networkName(),
            {},
            ProtocolStateTransition::accountSettlementForReplay(block.index())
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
        replayGovernanceBlock(governance, block);
    }

    return view;
}

core::StateTransitionPreviewContext RuntimeAccountStateBuilder::previewContextAtTip(
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    const std::uint64_t nextBlockHeight = blockchain.size();
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
        genesisConfig.networkParameters().networkName(),
        {},
        ProtocolStateTransition::accountSettlementForReplay(nextBlockHeight)
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
