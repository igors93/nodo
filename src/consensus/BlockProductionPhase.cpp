#include "consensus/BlockProductionPhase.hpp"

#include "core/BlockStateTransitionValidator.hpp"
#include "core/MempoolBlockProducer.hpp"
#include "core/ProtocolLimits.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/StateTransitionEngine.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/FeeEconomics.hpp"
#include "node/CanonicalSlashingTransition.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeMonetaryValidation.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace nodo::consensus {

namespace {

std::int64_t effectiveMinFeeRaw(const node::NodeRuntime& runtime) {
    const std::uint64_t raw = runtime.effectiveMinimumFeeRawUnits();
    if (raw > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return std::numeric_limits<std::int64_t>::max();
    return static_cast<std::int64_t>(raw);
}

} // namespace

BlockCandidateResult BlockProductionPhase::produce(
    node::NodeRuntime&                      runtime,
    const node::RuntimeBlockPipelineConfig& config,
    PendingSlashingEvidenceBatch slashingEvidence
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

    const bool evidenceOnly =
        !production.produced() &&
        production.status() == core::BlockProductionStatus::EMPTY_MEMPOOL &&
        !slashingEvidence.empty() &&
        config.minTransactionsPerBlock() == 0;
    if (!production.produced() && !evidenceOnly) {
        return BlockCandidateResult::failed(production.reason());
    }

    if (slashingEvidence.size() >
        core::ProtocolLimits::MAX_SLASHING_EVIDENCE_PER_BLOCK) {
        return BlockCandidateResult::failed(
            "Too many slashing evidence records for one block."
        );
    }
    std::sort(
        slashingEvidence.doubleVotes.begin(),
        slashingEvidence.doubleVotes.end(),
        [](const DoubleVoteEvidence& left, const DoubleVoteEvidence& right) {
            return left.evidenceId() < right.evidenceId();
        }
    );
    std::sort(
        slashingEvidence.proposerEquivocations.begin(),
        slashingEvidence.proposerEquivocations.end(),
        [](const ProposerEquivocationEvidence& left,
           const ProposerEquivocationEvidence& right) {
            return left.evidenceId() < right.evidenceId();
        }
    );

    std::vector<core::LedgerRecord> records = production.produced()
        ? production.block().records()
        : std::vector<core::LedgerRecord>{};
    std::size_t transactionRecordCount = records.size();
    try {
        std::vector<core::LedgerRecord> evidenceRecords;
        evidenceRecords.reserve(slashingEvidence.size());
        for (const DoubleVoteEvidence& evidence : slashingEvidence.doubleVotes) {
            evidenceRecords.push_back(
                node::CanonicalSlashingTransition::buildEvidenceRecord(
                    evidence, config.timestamp()
                )
            );
        }
        for (const ProposerEquivocationEvidence& evidence :
             slashingEvidence.proposerEquivocations) {
            evidenceRecords.push_back(
                node::CanonicalSlashingTransition::buildEvidenceRecord(
                    evidence, config.timestamp()
                )
            );
        }
        std::sort(
            evidenceRecords.begin(),
            evidenceRecords.end(),
            [](const core::LedgerRecord& left, const core::LedgerRecord& right) {
                return left.sourceId() < right.sourceId();
            }
        );
        records.insert(records.end(), evidenceRecords.begin(), evidenceRecords.end());
    } catch (const std::exception& error) {
        return BlockCandidateResult::failed(error.what());
    }

    const std::uint64_t candidateHeight = runtime.blockchain().size();
    if (candidateHeight != activeRound.height()) {
        return BlockCandidateResult::failed(
            "Candidate block height " + std::to_string(candidateHeight) +
            " does not match active consensus height " +
            std::to_string(activeRound.height()) + "."
        );
    }

    try {
        std::optional<core::Block> draft;
        while (!draft.has_value()) {
            try {
                draft.emplace(
                    candidateHeight,
                    runtime.blockchain().latestBlock().hash(),
                    records,
                    config.timestamp(),
                    "",
                    ""
                );
            } catch (const std::invalid_argument& error) {
                const bool canTrimTransaction =
                    std::string(error.what()) ==
                        "Block exceeds canonical protocol resource limits." &&
                    transactionRecordCount >
                        config.minTransactionsPerBlock();
                if (!canTrimTransaction) throw;
                records.erase(
                    records.begin() +
                    static_cast<std::ptrdiff_t>(transactionRecordCount - 1)
                );
                --transactionRecordCount;
            }
        }
        const core::Block& draftBlock = draft.value();
        const core::StateTransitionPreviewContext committedContext =
            node::RuntimeAccountStateBuilder::previewContextAtTip(
                runtime, effectiveMinFeeRaw(runtime)
            );
        const core::StateTransitionPreviewResult committedPreview =
            core::StateTransitionEngine::executeBlock(
                draftBlock, committedContext
            );
        if (!committedPreview.accepted()) {
            return BlockCandidateResult::failed(committedPreview.reason());
        }

        core::Block committedBlock(
            draftBlock.index(),
            draftBlock.previousHash(),
            draftBlock.records(),
            draftBlock.timestamp(),
            committedPreview.stateRoot(),
            committedPreview.receiptsRoot()
        );
        const core::BlockValidationResult transitionValidation =
            core::BlockStateTransitionValidator::validateCandidateBlock(
                runtime.blockchain(),
                committedBlock,
                committedContext
            );
        if (!transitionValidation.accepted()) {
            return BlockCandidateResult::failed(
                transitionValidation.reason()
            );
        }

        const node::FeeEconomicBalance feeBalance =
            node::FeeEconomics::buildFeeEconomicBalance(
                committedBlock.index(), transitionValidation.totalFee()
            );
        const node::RuntimeMonetaryValidationResult monetaryResult =
            node::RuntimeMonetaryValidation::validateCandidate(
                runtime.config().genesisConfig(),
                committedBlock,
                feeBalance.burnAmount(),
                runtime.supplyState().latestSupply()
            );
        if (!monetaryResult.isAccepted()) {
            return BlockCandidateResult::failed(
                "Monetary gate rejected candidate: " +
                monetaryResult.reason()
            );
        }

        return BlockCandidateResult::ok(
            std::move(committedBlock)
        );
    } catch (const std::exception& error) {
        return BlockCandidateResult::failed(error.what());
    }
}

} // namespace nodo::consensus
