#include "node/RuntimeBlockPipeline.hpp"

#include "consensus/ValidatorVoteBuilder.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "crypto/ProtocolCryptoContext.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::int64_t minimumFeeRawUnitsForRuntime(
    const NodeRuntime& runtime
) {
    const std::uint64_t minimumFee =
        runtime.config().genesisConfig().networkParameters().minimumFeeRawUnits();

    if (minimumFee > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }

    return static_cast<std::int64_t>(minimumFee);
}

} // namespace

RuntimeBlockPipelineConfig::RuntimeBlockPipelineConfig()
    : m_maxTransactionsPerBlock(1000),
      m_minTransactionsPerBlock(1),
      m_consensusRound(1),
      m_timestamp(0) {}

RuntimeBlockPipelineConfig::RuntimeBlockPipelineConfig(
    std::size_t maxTransactionsPerBlock,
    std::size_t minTransactionsPerBlock,
    std::uint64_t consensusRound,
    std::int64_t timestamp
)
    : m_maxTransactionsPerBlock(maxTransactionsPerBlock),
      m_minTransactionsPerBlock(minTransactionsPerBlock),
      m_consensusRound(consensusRound),
      m_timestamp(timestamp) {}

std::size_t RuntimeBlockPipelineConfig::maxTransactionsPerBlock() const {
    return m_maxTransactionsPerBlock;
}

std::size_t RuntimeBlockPipelineConfig::minTransactionsPerBlock() const {
    return m_minTransactionsPerBlock;
}

std::uint64_t RuntimeBlockPipelineConfig::consensusRound() const {
    return m_consensusRound;
}

std::int64_t RuntimeBlockPipelineConfig::timestamp() const {
    return m_timestamp;
}

bool RuntimeBlockPipelineConfig::isValid() const {
    return m_maxTransactionsPerBlock > 0 &&
           m_minTransactionsPerBlock > 0 &&
           m_minTransactionsPerBlock <= m_maxTransactionsPerBlock &&
           m_consensusRound > 0 &&
           m_timestamp > 0;
}

std::string RuntimeBlockPipelineConfig::serialize() const {
    std::ostringstream oss;

    oss << "RuntimeBlockPipelineConfig{"
        << "maxTransactionsPerBlock=" << m_maxTransactionsPerBlock
        << ";minTransactionsPerBlock=" << m_minTransactionsPerBlock
        << ";consensusRound=" << m_consensusRound
        << ";timestamp=" << m_timestamp
        << "}";

    return oss.str();
}

std::string runtimeBlockPipelineStatusToString(
    RuntimeBlockPipelineStatus status
) {
    switch (status) {
        case RuntimeBlockPipelineStatus::FINALIZED:
            return "FINALIZED";
        case RuntimeBlockPipelineStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case RuntimeBlockPipelineStatus::INVALID_RUNTIME:
            return "INVALID_RUNTIME";
        case RuntimeBlockPipelineStatus::BLOCK_PRODUCTION_FAILED:
            return "BLOCK_PRODUCTION_FAILED";
        case RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED:
            return "STATE_TRANSITION_FAILED";
        case RuntimeBlockPipelineStatus::NOT_ENOUGH_VALIDATORS:
            return "NOT_ENOUGH_VALIDATORS";
        case RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED:
            return "VOTE_BUILD_FAILED";
        case RuntimeBlockPipelineStatus::QUORUM_BUILD_FAILED:
            return "QUORUM_BUILD_FAILED";
        case RuntimeBlockPipelineStatus::FINALIZATION_FAILED:
            return "FINALIZATION_FAILED";
        default:
            return "FINALIZATION_FAILED";
    }
}

RuntimeBlockPipelineResult::RuntimeBlockPipelineResult()
    : m_status(RuntimeBlockPipelineStatus::INVALID_CONFIG),
      m_reason("Uninitialized runtime block pipeline result."),
      m_block(std::nullopt),
      m_certificate(),
      m_finalizedRecord(),
      m_finalizedTransactionIds() {}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::finalized(
    core::Block block,
    consensus::QuorumCertificate certificate,
    consensus::FinalizedBlockRecord finalizedRecord,
    std::vector<std::string> finalizedTransactionIds
) {
    RuntimeBlockPipelineResult result;
    result.m_status = RuntimeBlockPipelineStatus::FINALIZED;
    result.m_reason = "";
    result.m_block = std::move(block);
    result.m_certificate = std::move(certificate);
    result.m_finalizedRecord = std::move(finalizedRecord);
    result.m_finalizedTransactionIds = std::move(finalizedTransactionIds);
    return result;
}

RuntimeBlockPipelineResult RuntimeBlockPipelineResult::rejected(
    RuntimeBlockPipelineStatus status,
    std::string reason
) {
    RuntimeBlockPipelineResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

RuntimeBlockPipelineStatus RuntimeBlockPipelineResult::status() const {
    return m_status;
}

const std::string& RuntimeBlockPipelineResult::reason() const {
    return m_reason;
}

bool RuntimeBlockPipelineResult::finalized() const {
    return m_status == RuntimeBlockPipelineStatus::FINALIZED &&
           m_block.has_value() &&
           m_block->isValid() &&
           m_certificate.isStructurallyValid() &&
           m_finalizedRecord.isStructurallyValid();
}

const core::Block& RuntimeBlockPipelineResult::block() const {
    if (!m_block.has_value()) {
        throw std::logic_error("RuntimeBlockPipelineResult has no finalized block.");
    }

    return m_block.value();
}

const consensus::QuorumCertificate& RuntimeBlockPipelineResult::certificate() const {
    return m_certificate;
}

const consensus::FinalizedBlockRecord& RuntimeBlockPipelineResult::finalizedRecord() const {
    return m_finalizedRecord;
}

const std::vector<std::string>& RuntimeBlockPipelineResult::finalizedTransactionIds() const {
    return m_finalizedTransactionIds;
}

std::string RuntimeBlockPipelineResult::serialize() const {
    std::ostringstream oss;

    oss << "RuntimeBlockPipelineResult{"
        << "status=" << runtimeBlockPipelineStatusToString(m_status)
        << ";reason=" << m_reason
        << ";blockHash=" << (m_block.has_value() && m_block->isValid() ? m_block->hash() : "NONE")
        << ";finalizedTransactionCount=" << m_finalizedTransactionIds.size()
        << "}";

    return oss.str();
}

RuntimeBlockPipelineResult RuntimeBlockPipeline::produceAndFinalizeNextBlock(
    NodeRuntime& runtime,
    const RuntimeBlockPipelineConfig& config,
    const crypto::Signer& localValidatorSigner
) {
    if (!config.isValid()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::INVALID_CONFIG,
            "Runtime block pipeline config is invalid."
        );
    }

    if (!runtime.isValid()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::INVALID_RUNTIME,
            "Node runtime is invalid."
        );
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            runtime.config().genesisConfig().networkParameters().networkName()
        );

    if (!cryptoContext.isValid()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::INVALID_CONFIG,
            "Protocol crypto context is invalid for network '"
            + runtime.config().genesisConfig().networkParameters().networkName()
            + "': "
            + cryptoContext.rejectionReason()
        );
    }

    const core::BlockProductionResult production =
        core::MempoolBlockProducer::produceCandidateBlock(
            runtime.blockchain(),
            runtime.mempool(),
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            core::BlockProductionConfig(
                config.maxTransactionsPerBlock(),
                config.minTransactionsPerBlock()
            ),
            config.timestamp()
        );

    if (!production.produced()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::BLOCK_PRODUCTION_FAILED,
            production.reason()
        );
    }

    const core::BlockValidationResult transitionValidation =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            runtime.blockchain(),
            production.block(),
            minimumFeeRawUnitsForRuntime(runtime)
        );

    if (!transitionValidation.accepted()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
            transitionValidation.reason()
        );
    }

    std::vector<consensus::ValidatorVoteRecord> votes;

    try {
        votes = buildValidatorVotes(
            runtime,
            production.block(),
            config.consensusRound(),
            config.timestamp() + 1,
            localValidatorSigner
        );
    } catch (const std::exception& error) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED,
            error.what()
        );
    }

    if (votes.empty()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::NOT_ENOUGH_VALIDATORS,
            "No active validators are available to vote."
        );
    }

    const consensus::QuorumCertificateBuildResult certificate =
        consensus::QuorumCertificateBuilder::buildFromVotes(
            production.block().index(),
            production.block().hash(),
            production.block().previousHash(),
            config.consensusRound(),
            votes,
            runtime.validatorRegistry(),
            cryptoContext.policy(),
            cryptoContext.signatureProvider(),
            runtime.config().genesisConfig().networkParameters().quorumThresholdNumerator(),
            runtime.config().genesisConfig().networkParameters().quorumThresholdDenominator()
        );

    if (!certificate.certified()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::QUORUM_BUILD_FAILED,
            certificate.reason()
        );
    }

    const consensus::BlockFinalizationResult finalization =
        consensus::BlockFinalizer::finalizeBlock(
            runtime.mutableBlockchain(),
            production.block(),
            certificate.certificate(),
            runtime.validatorRegistry(),
            runtime.mutableFinalizationRegistry(),
            cryptoContext.policy(),
            cryptoContext.signatureProvider(),
            config.timestamp() + 2
        );

    if (!finalization.finalized() &&
        !finalization.duplicate()) {
        return RuntimeBlockPipelineResult::rejected(
            RuntimeBlockPipelineStatus::FINALIZATION_FAILED,
            finalization.reason()
        );
    }

    const std::vector<std::string> finalizedTransactionIds =
        production.plan().transactionIds();

    removeFinalizedTransactionsFromMempool(
        runtime,
        finalizedTransactionIds
    );

    return RuntimeBlockPipelineResult::finalized(
        production.block(),
        certificate.certificate(),
        finalization.record(),
        finalizedTransactionIds
    );
}

std::vector<consensus::ValidatorVoteRecord> RuntimeBlockPipeline::buildValidatorVotes(
    const NodeRuntime& runtime,
    const core::Block& block,
    std::uint64_t consensusRound,
    std::int64_t timestamp,
    const crypto::Signer& localValidatorSigner
) {
    std::vector<consensus::ValidatorVoteRecord> votes;

    votes.push_back(
        consensus::ValidatorVoteBuilder::buildApprovalVote(
            runtime.validatorRegistry(),
            block,
            consensusRound,
            timestamp,
            localValidatorSigner
        )
    );

    return votes;
}

void RuntimeBlockPipeline::removeFinalizedTransactionsFromMempool(
    NodeRuntime& runtime,
    const std::vector<std::string>& transactionIds
) {
    for (const std::string& transactionId : transactionIds) {
        runtime.mutableMempool().removeTransaction(transactionId);
    }
}

} // namespace nodo::node
