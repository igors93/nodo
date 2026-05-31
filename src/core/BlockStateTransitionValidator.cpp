#include "core/BlockStateTransitionValidator.hpp"

#include "core/Transaction.hpp"

#include <exception>
#include <set>
#include <sstream>
#include <utility>

namespace nodo::core {

std::string blockValidationStatusToString(
    BlockValidationStatus status
) {
    switch (status) {
        case BlockValidationStatus::VALID:
            return "VALID";
        case BlockValidationStatus::INVALID_BLOCKCHAIN:
            return "INVALID_BLOCKCHAIN";
        case BlockValidationStatus::INVALID_BLOCK:
            return "INVALID_BLOCK";
        case BlockValidationStatus::INVALID_PREVIOUS_HASH:
            return "INVALID_PREVIOUS_HASH";
        case BlockValidationStatus::INVALID_LEDGER_RECORD:
            return "INVALID_LEDGER_RECORD";
        case BlockValidationStatus::INVALID_TRANSACTION:
            return "INVALID_TRANSACTION";
        case BlockValidationStatus::DUPLICATE_LEDGER_SOURCE:
            return "DUPLICATE_LEDGER_SOURCE";
        default:
            return "INVALID_BLOCK";
    }
}

BlockValidationResult::BlockValidationResult()
    : m_status(BlockValidationStatus::INVALID_BLOCK),
      m_reason("Uninitialized block validation result.") {}

BlockValidationResult BlockValidationResult::valid() {
    BlockValidationResult result;
    result.m_status = BlockValidationStatus::VALID;
    result.m_reason = "";
    return result;
}

BlockValidationResult BlockValidationResult::rejected(
    BlockValidationStatus status,
    std::string reason
) {
    BlockValidationResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

BlockValidationStatus BlockValidationResult::status() const {
    return m_status;
}

const std::string& BlockValidationResult::reason() const {
    return m_reason;
}

bool BlockValidationResult::accepted() const {
    return m_status == BlockValidationStatus::VALID;
}

std::string BlockValidationResult::serialize() const {
    std::ostringstream oss;

    oss << "BlockValidationResult{"
        << "status=" << blockValidationStatusToString(m_status)
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

BlockValidationResult BlockStateTransitionValidator::validateCandidateBlock(
    const Blockchain& blockchain,
    const Block& candidateBlock
) {
    return validateCandidateBlock(
        blockchain,
        candidateBlock,
        0
    );
}

BlockValidationResult BlockStateTransitionValidator::validateCandidateBlock(
    const Blockchain& blockchain,
    const Block& candidateBlock,
    std::int64_t minimumFeeRawUnits
) {
    if (minimumFeeRawUnits < 0) {
        return BlockValidationResult::rejected(
            BlockValidationStatus::INVALID_TRANSACTION,
            "Minimum fee cannot be negative."
        );
    }

    if (blockchain.empty() ||
        !blockchain.isValid()) {
        return BlockValidationResult::rejected(
            BlockValidationStatus::INVALID_BLOCKCHAIN,
            "Blockchain is empty or invalid."
        );
    }

    if (!candidateBlock.isValid()) {
        return BlockValidationResult::rejected(
            BlockValidationStatus::INVALID_BLOCK,
            "Candidate block is structurally invalid."
        );
    }

    if (!blockchain.canAppendBlock(candidateBlock)) {
        return BlockValidationResult::rejected(
            BlockValidationStatus::INVALID_PREVIOUS_HASH,
            "Candidate block does not append to the current chain tip."
        );
    }

    std::set<std::string> sourceIds;
    std::set<std::string> transactionIds;

    for (const LedgerRecord& record : candidateBlock.records()) {
        if (!record.isValid()) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::INVALID_LEDGER_RECORD,
                "Candidate block contains an invalid ledger record."
            );
        }

        if (!sourceIds.insert(record.sourceId()).second) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::DUPLICATE_LEDGER_SOURCE,
                "Candidate block contains duplicate ledger record source ids."
            );
        }

        if (record.type() == LedgerRecordType::TRANSACTION) {
            try {
                const Transaction transaction =
                    Transaction::deserializeForStateReplay(
                        record.payload()
                    );

                if (transaction.id() != record.sourceId() ||
                    transaction.nonce() == 0 ||
                    !transaction.amount().isPositive() ||
                    transaction.fee().isNegative() ||
                    transaction.fromAddress().empty() ||
                    transaction.toAddress().empty() ||
                    transaction.fromAddress() == transaction.toAddress()) {
                    return BlockValidationResult::rejected(
                        BlockValidationStatus::INVALID_TRANSACTION,
                        "Candidate block contains an invalid transaction ledger payload."
                    );
                }

                if (transaction.fee().rawUnits() < minimumFeeRawUnits) {
                    return BlockValidationResult::rejected(
                        BlockValidationStatus::INVALID_TRANSACTION,
                        "Candidate block contains a transaction below the network minimum fee."
                    );
                }

                if (!transactionIds.insert(transaction.id()).second) {
                    return BlockValidationResult::rejected(
                        BlockValidationStatus::DUPLICATE_LEDGER_SOURCE,
                        "Candidate block contains duplicate transaction ids."
                    );
                }
            } catch (const std::exception& error) {
                return BlockValidationResult::rejected(
                    BlockValidationStatus::INVALID_TRANSACTION,
                    error.what()
                );
            }
        }
    }

    return BlockValidationResult::valid();
}

} // namespace nodo::core
