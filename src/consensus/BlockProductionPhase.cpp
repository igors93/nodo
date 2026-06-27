#include "consensus/BlockProductionPhase.hpp"

#include "core/BlockStateTransitionValidator.hpp"
#include "core/MempoolBlockProducer.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/StateTransitionEngine.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/FeeEconomics.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeMonetaryValidation.hpp"

#include <limits>
#include <stdexcept>

namespace nodo::consensus {

namespace {

std::int64_t effectiveMinFeeRaw(const node::NodeRuntime& runtime) {
    const std::uint64_t raw =
        runtime.config().genesisConfig().networkParameters().minimumFeeRawUnits();
    if (raw > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return std::numeric_limits<std::int64_t>::max();
    return static_cast<std::int64_t>(raw);
}

} // namespace

BlockCandidateResult BlockProductionPhase::produce(
    node::NodeRuntime&                      runtime,
    const node::RuntimeBlockPipelineConfig& config
) {
    if (!config.isValid()) {
        return BlockCandidateResult::failed("Runtime block pipeline config is invalid.");
    }

    if (!runtime.isValid()) {
        return BlockCandidateResult::failed("Node runtime is invalid.");
    }

    const consensus::ConsensusRoundState activeRound =
        runtime.consensusRoundManager().currentState();

    if (config.consensusRound() != activeRound.round()) {
        return BlockCandidateResult::failed(
            "Consensus round mismatch: expected " +
            std::to_string(activeRound.round()) +
            ", got " + std::to_string(config.consensusRound()) + "."
        );
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            runtime.config().genesisConfig().networkParameters().networkName()
        );

    if (!cryptoContext.isValid()) {
        return BlockCandidateResult::failed(
            "Protocol crypto context is invalid: " + cryptoContext.rejectionReason()
        );
    }

    core::StateTransitionPreviewContext previewContext;
    try {
        previewContext = node::RuntimeAccountStateBuilder::previewContextAtTip(
            runtime, effectiveMinFeeRaw(runtime)
        );
    } catch (const std::exception& e) {
        return BlockCandidateResult::failed(
            std::string("Unable to rebuild account state for mempool selection: ") + e.what()
        );
    }

    const core::BlockProductionResult production =
        core::MempoolBlockProducer::produceCandidateBlock(
            runtime.blockchain(),
            runtime.mempool(),
            previewContext,
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            core::BlockProductionConfig(
                config.maxTransactionsPerBlock(),
                config.minTransactionsPerBlock()
            ),
            config.timestamp()
        );

    if (!production.produced()) {
        return BlockCandidateResult::failed(production.reason());
    }

    if (production.block().index() != activeRound.height()) {
        return BlockCandidateResult::failed(
            "Candidate block height " + std::to_string(production.block().index()) +
            " does not match active consensus height " +
            std::to_string(activeRound.height()) + "."
        );
    }

    core::BlockValidationResult transitionValidation;
    try {
        transitionValidation =
            core::BlockStateTransitionValidator::validateCandidateBlock(
                runtime.blockchain(),
                production.block(),
                node::RuntimeAccountStateBuilder::previewContextAtTip(
                    runtime, effectiveMinFeeRaw(runtime)
                )
            );
    } catch (const std::exception& e) {
        return BlockCandidateResult::failed(e.what());
    }

    if (!transitionValidation.accepted()) {
        return BlockCandidateResult::failed(transitionValidation.reason());
    }

    const node::FeeEconomicBalance preMintFeeBalance =
        node::FeeEconomics::buildFeeEconomicBalance(
            production.block().index(),
            transitionValidation.totalFee()
        );

    const node::RuntimeMonetaryValidationResult monetaryResult =
        node::RuntimeMonetaryValidation::validateCandidate(
            runtime.config().genesisConfig(),
            production.block(),
            preMintFeeBalance.burnAmount(),
            runtime.supplyState().latestSupply()
        );

    if (!monetaryResult.isAccepted()) {
        return BlockCandidateResult::failed(
            "Monetary gate rejected candidate: " + monetaryResult.reason()
        );
    }

    try {
        const core::StateTransitionPreviewContext committedContext =
            node::RuntimeAccountStateBuilder::previewContextAtTip(
                runtime,
                effectiveMinFeeRaw(runtime)
            );
        const core::Block draft(
            production.block().index(),
            production.block().previousHash(),
            production.block().records(),
            production.block().timestamp(),
            "",
            ""
        );
        const core::StateTransitionPreviewResult committedPreview =
            core::StateTransitionEngine::executeBlock(draft, committedContext);
        if (!committedPreview.accepted()) {
            return BlockCandidateResult::failed(committedPreview.reason());
        }
        core::Block committedBlock(
            draft.index(), draft.previousHash(), draft.records(), draft.timestamp(),
            committedPreview.stateRoot(), committedPreview.receiptsRoot()
        );
        const core::BlockValidationResult committedValidation =
            core::BlockStateTransitionValidator::validateCandidateBlock(
                runtime.blockchain(), committedBlock, committedContext
            );
        if (!committedValidation.accepted()) {
            return BlockCandidateResult::failed(committedValidation.reason());
        }
        return BlockCandidateResult::ok(std::move(committedBlock));
    } catch (const std::exception& error) {
        return BlockCandidateResult::failed(error.what());
    }
}

} // namespace nodo::consensus
