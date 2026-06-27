#include "core/ChainStateRebuilder.hpp"

#include "core/BlockStateTransitionValidator.hpp"
#include "core/LedgerRecord.hpp"
#include "core/Transaction.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/MintRecord.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

StateRebuildReport::StateRebuildReport()
    : m_success(true),
      m_failureReason(""),
      m_blockCount(0),
      m_ledgerRecordCount(0),
      m_mintRecordCount(0),
      m_genesisRewardRecordCount(0),
      m_transactionRecordCount(0),
      m_protectionMetadataRecordCount(0),
      m_validatorPenaltyRecordCount(0),
      m_commitmentVerificationPassed(false),
      m_firstFailedCommitmentHeight(0),
      m_commitmentFailureReason("") {}

bool StateRebuildReport::success() const {
    return m_success;
}

const std::string& StateRebuildReport::failureReason() const {
    return m_failureReason;
}

std::size_t StateRebuildReport::blockCount() const {
    return m_blockCount;
}

std::size_t StateRebuildReport::ledgerRecordCount() const {
    return m_ledgerRecordCount;
}

std::size_t StateRebuildReport::mintRecordCount() const {
    return m_mintRecordCount;
}

std::size_t StateRebuildReport::genesisRewardRecordCount() const {
    return m_genesisRewardRecordCount;
}

std::size_t StateRebuildReport::transactionRecordCount() const {
    return m_transactionRecordCount;
}

std::size_t StateRebuildReport::protectionMetadataRecordCount() const {
    return m_protectionMetadataRecordCount;
}

std::size_t StateRebuildReport::validatorPenaltyRecordCount() const {
    return m_validatorPenaltyRecordCount;
}

void StateRebuildReport::markFailure(std::string reason) {
    m_success = false;
    m_failureReason = std::move(reason);
}

void StateRebuildReport::setBlockCount(std::size_t value) {
    m_blockCount = value;
}

void StateRebuildReport::incrementLedgerRecordCount() {
    ++m_ledgerRecordCount;
}

void StateRebuildReport::incrementMintRecordCount() {
    ++m_mintRecordCount;
}

void StateRebuildReport::incrementGenesisRewardRecordCount() {
    ++m_genesisRewardRecordCount;
}

void StateRebuildReport::incrementTransactionRecordCount() {
    ++m_transactionRecordCount;
}

void StateRebuildReport::incrementProtectionMetadataRecordCount() {
    ++m_protectionMetadataRecordCount;
}

void StateRebuildReport::incrementValidatorPenaltyRecordCount() {
    ++m_validatorPenaltyRecordCount;
}

bool StateRebuildReport::commitmentVerificationPassed() const {
    return m_commitmentVerificationPassed;
}

std::uint64_t StateRebuildReport::firstFailedCommitmentHeight() const {
    return m_firstFailedCommitmentHeight;
}

const std::string& StateRebuildReport::commitmentFailureReason() const {
    return m_commitmentFailureReason;
}

void StateRebuildReport::setCommitmentVerificationPassed(bool value) {
    m_commitmentVerificationPassed = value;
}

void StateRebuildReport::setFirstFailedCommitmentHeight(std::uint64_t height) {
    m_firstFailedCommitmentHeight = height;
}

void StateRebuildReport::setCommitmentFailureReason(std::string reason) {
    m_commitmentFailureReason = std::move(reason);
}

std::string StateRebuildReport::serialize() const {
    std::ostringstream oss;

    oss << "StateRebuildReport{"
        << "success=" << (m_success ? "true" : "false")
        << ";failureReason=" << m_failureReason
        << ";blockCount=" << m_blockCount
        << ";ledgerRecordCount=" << m_ledgerRecordCount
        << ";mintRecordCount=" << m_mintRecordCount
        << ";genesisRewardRecordCount=" << m_genesisRewardRecordCount
        << ";transactionRecordCount=" << m_transactionRecordCount
        << ";protectionMetadataRecordCount=" << m_protectionMetadataRecordCount
        << ";validatorPenaltyRecordCount=" << m_validatorPenaltyRecordCount
        << ";commitmentVerificationPassed=" << (m_commitmentVerificationPassed ? "true" : "false")
        << ";firstFailedCommitmentHeight=" << m_firstFailedCommitmentHeight
        << ";commitmentFailureReason=" << m_commitmentFailureReason
        << "}";

    return oss.str();
}

StateRebuildReport ChainStateRebuilder::auditBlockchain(
    const Blockchain& blockchain
) {
    StateRebuildReport report;

    if (blockchain.empty()) {
        report.markFailure("Blockchain is empty.");
        return report;
    }

    if (!blockchain.isValid(false)) {
        report.markFailure("Blockchain validation failed.");
        return report;
    }

    report.setBlockCount(blockchain.size());

    for (const auto& block : blockchain.blocks()) {
        if (!block.isValid(false)) {
            report.markFailure("Invalid block found during rebuild audit.");
            return report;
        }

        for (const auto& record : block.records()) {
            if (!record.isValid()) {
                report.markFailure("Invalid LedgerRecord found during rebuild audit.");
                return report;
            }

            report.incrementLedgerRecordCount();

            if (record.type() == LedgerRecordType::MINT) {
                report.incrementMintRecordCount();
            } else if (record.type() == LedgerRecordType::GENESIS_REWARD) {
                report.incrementGenesisRewardRecordCount();
            } else if (record.type() == LedgerRecordType::TRANSACTION) {
                report.incrementTransactionRecordCount();
            } else if (record.type() == LedgerRecordType::VALIDATOR_PENALTY) {
                report.incrementValidatorPenaltyRecordCount();
            } else if (record.type() == LedgerRecordType::VALIDATION_WORK ||
                       record.type() == LedgerRecordType::VALIDATOR_SCORE ||
                       record.type() == LedgerRecordType::PROTECTION_EPOCH) {
                report.incrementProtectionMetadataRecordCount();
            } else {
                report.markFailure("Unknown LedgerRecord type found during rebuild audit.");
                return report;
            }
        }
    }

    return report;
}

State ChainStateRebuilder::rebuildStateFromGenesisRewardRecords(
    const Blockchain& blockchain
) {
    StateRebuildReport report = auditBlockchain(blockchain);

    if (!report.success()) {
        throw std::logic_error(
            "Cannot rebuild State from invalid Blockchain: "
            + report.failureReason()
        );
    }

    State rebuiltState;

    for (const auto& block : blockchain.blocks()) {
        if (rebuiltState.currentBlockIndex() != block.index()) {
            throw std::logic_error("State block index does not match Blockchain block index.");
        }

        for (const auto& record : block.records()) {
            if (record.type() != LedgerRecordType::GENESIS_REWARD) {
                continue;
            }

            economics::GenesisRewardRecord genesisRewardRecord =
                economics::GenesisRewardRecord::deserialize(record.payload());

            rebuiltState.applyGenesisRewardRecord(genesisRewardRecord);
        }

        if (block.index() + 1 < blockchain.size()) {
            rebuiltState.advanceBlock();
        }
    }

    if (!rebuiltState.isSupplyAuditable()) {
        throw std::logic_error("Rebuilt GenesisReward State failed supply audit.");
    }

    return rebuiltState;
}

State ChainStateRebuilder::rebuildStateFromLedgerRecords(
    const Blockchain& blockchain
) {
    StateRebuildReport report = auditBlockchain(blockchain);

    if (!report.success()) {
        throw std::logic_error(
            "Cannot rebuild State from invalid Blockchain: "
            + report.failureReason()
        );
    }

    State rebuiltState;

    for (const auto& block : blockchain.blocks()) {
        if (rebuiltState.currentBlockIndex() != block.index()) {
            throw std::logic_error("State block index does not match Blockchain block index.");
        }

        for (const auto& record : block.records()) {
            if (record.type() == LedgerRecordType::MINT) {
                economics::MintRecord mintRecord =
                    economics::MintRecord::deserialize(record.payload());

                rebuiltState.applyMintRecord(mintRecord);
                continue;
            }

            if (record.type() == LedgerRecordType::GENESIS_REWARD) {
                economics::GenesisRewardRecord genesisRewardRecord =
                    economics::GenesisRewardRecord::deserialize(record.payload());

                rebuiltState.applyGenesisRewardRecord(genesisRewardRecord);
                continue;
            }

            if (record.type() == LedgerRecordType::TRANSACTION) {
                Transaction transaction =
                    Transaction::deserialize(record.payload());

                if (transaction.type() != TransactionType::TRANSFER) {
                    throw std::logic_error("Unsupported transaction type during State rebuild.");
                }

                rebuiltState.applyTransferTransaction(transaction);
                continue;
            }

            if (record.type() == LedgerRecordType::VALIDATION_WORK ||
                record.type() == LedgerRecordType::VALIDATOR_SCORE ||
                record.type() == LedgerRecordType::PROTECTION_EPOCH ||
                record.type() == LedgerRecordType::VALIDATOR_PENALTY) {
                /*
                 * These records are part of official ledger history but do not
                 * directly mutate public coin ownership State.
                 */
                continue;
            }

            throw std::logic_error("Unsupported LedgerRecord type during State rebuild.");
        }

        if (block.index() + 1 < blockchain.size()) {
            rebuiltState.advanceBlock();
        }
    }

    if (!rebuiltState.isSupplyAuditable()) {
        throw std::logic_error("Rebuilt State failed supply audit.");
    }

    return rebuiltState;
}

StateRebuildReport ChainStateRebuilder::rebuildAndVerifyViaEngine(
    const Blockchain& blockchain,
    std::function<StateTransitionPreviewContext(const Blockchain&)> contextBuilder
) {
    StateRebuildReport report = auditBlockchain(blockchain);
    if (!report.success()) {
        return report;
    }

    if (blockchain.size() <= 1) {
        // Genesis-only or empty chain: commitments trivially pass.
        report.setCommitmentVerificationPassed(true);
        return report;
    }

    // Replay the chain block-by-block through StateTransitionEngine.
    // For each non-genesis block, build the context from the partial chain (all
    // preceding blocks), then validate with ProtocolCommitment mode to verify
    // that the engine-computed stateRoot and receiptsRoot match the block's
    // declared values.
    Blockchain partialChain;
    partialChain.addGenesisBlock(blockchain.blocks().front());

    const auto& allBlocks = blockchain.blocks();
    for (std::size_t i = 1; i < allBlocks.size(); ++i) {
        const Block& candidate = allBlocks[i];

        const StateTransitionPreviewContext ctx = contextBuilder(partialChain);

        const BlockValidationResult result =
            BlockStateTransitionValidator::validateCandidateBlock(
                partialChain,
                candidate,
                ctx,
                BlockValidationMode::ProtocolCommitment
            );

        if (!result.accepted()) {
            report.setCommitmentVerificationPassed(false);
            report.setFirstFailedCommitmentHeight(candidate.index());
            report.setCommitmentFailureReason(
                "Engine commitment mismatch at block " +
                std::to_string(candidate.index()) + ": " + result.reason()
            );
            report.markFailure(report.commitmentFailureReason());
            return report;
        }

        partialChain.addBlock(candidate);
    }

    report.setCommitmentVerificationPassed(true);
    return report;
}

} // namespace nodo::core
