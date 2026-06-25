#include "core/BlockStateTransitionValidator.hpp"

#include "core/StateTransitionEngine.hpp"

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
        case BlockValidationStatus::STATE_PREVIEW_FAILED:
            return "STATE_PREVIEW_FAILED";
        default:
            return "INVALID_BLOCK";
    }
}

BlockValidationResult::BlockValidationResult()
    : m_status(BlockValidationStatus::INVALID_BLOCK),
      m_reason("Uninitialized block validation result."),
      m_stateRoot(""),
      m_totalFee(),
      m_receiptsRoot("") {}

BlockValidationResult BlockValidationResult::valid() {
    return valid(
        "",
        utils::Amount()
    );
}

BlockValidationResult BlockValidationResult::valid(
    std::string stateRoot
) {
    return valid(
        std::move(stateRoot),
        utils::Amount()
    );
}

BlockValidationResult BlockValidationResult::valid(
    std::string stateRoot,
    utils::Amount totalFee,
    std::string receiptsRoot
) {
    BlockValidationResult result;
    result.m_status = BlockValidationStatus::VALID;
    result.m_reason = "";
    result.m_stateRoot = std::move(stateRoot);
    result.m_totalFee = totalFee;
    result.m_receiptsRoot = std::move(receiptsRoot);
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

const std::string& BlockValidationResult::stateRoot() const {
    return m_stateRoot;
}

utils::Amount BlockValidationResult::totalFee() const {
    return m_totalFee;
}

const std::string& BlockValidationResult::receiptsRoot() const {
    return m_receiptsRoot;
}

bool BlockValidationResult::accepted() const {
    return m_status == BlockValidationStatus::VALID;
}

std::string BlockValidationResult::serialize() const {
    std::ostringstream oss;

    oss << "BlockValidationResult{"
        << "status=" << blockValidationStatusToString(m_status)
        << ";reason=" << m_reason
        << ";stateRoot=" << m_stateRoot
        << ";totalFeeRawUnits=" << m_totalFee.rawUnits()
        << ";receiptsRoot=" << m_receiptsRoot
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
    return validateCandidateBlock(
        blockchain,
        candidateBlock,
        StateTransitionPreviewContext::structuralOnly(
            minimumFeeRawUnits
        )
    );
}

BlockValidationResult BlockStateTransitionValidator::validateCandidateBlock(
    const Blockchain& blockchain,
    const Block& candidateBlock,
    const StateTransitionPreviewContext& context
) {
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

    if (context.wallClockNow() > 0 &&
        candidateBlock.timestamp() > context.wallClockNow() + 300) {
        return BlockValidationResult::rejected(
            BlockValidationStatus::INVALID_BLOCK,
            "Candidate block timestamp is too far in the future."
        );
    }

    for (const LedgerRecord& record : candidateBlock.records()) {
        if (!record.isValid()) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::INVALID_LEDGER_RECORD,
                "Candidate block contains an invalid ledger record."
            );
        }
    }

    const StateTransitionPreviewResult preview =
        StateTransitionEngine::executeBlock(
            candidateBlock,
            context
        );

    if (!preview.accepted()) {
        if (preview.status() == StateTransitionPreviewStatus::INVALID_CONTEXT) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::STATE_PREVIEW_FAILED,
                preview.reason()
            );
        }

        if (preview.status() == StateTransitionPreviewStatus::DUPLICATE_TRANSACTION) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::DUPLICATE_LEDGER_SOURCE,
                preview.reason()
            );
        }

        if (preview.status() == StateTransitionPreviewStatus::INVALID_LEDGER_RECORD) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::INVALID_LEDGER_RECORD,
                preview.reason()
            );
        }

        if (preview.status() == StateTransitionPreviewStatus::INVALID_TRANSACTION) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::INVALID_TRANSACTION,
                preview.reason()
            );
        }

        if (preview.status() == StateTransitionPreviewStatus::INSUFFICIENT_BALANCE ||
            preview.status() == StateTransitionPreviewStatus::INVALID_NONCE) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::INVALID_TRANSACTION,
                preview.reason()
            );
        }

        return BlockValidationResult::rejected(
            BlockValidationStatus::STATE_PREVIEW_FAILED,
            preview.reason()
        );
    }

    if (!candidateBlock.isGenesisBlock() && context.enforceAccountState()) {
        if (preview.stateRoot() != candidateBlock.stateRoot()) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::INVALID_BLOCK,
                "State root mismatch between block header and computed state transition."
            );
        }

        if (preview.receiptsRoot() != candidateBlock.receiptsRoot()) {
            return BlockValidationResult::rejected(
                BlockValidationStatus::INVALID_BLOCK,
                "Receipts root mismatch between block header and computed state transition."
            );
        }
    }

    return BlockValidationResult::valid(
        preview.stateRoot(),
        preview.totalFee(),
        preview.receiptsRoot()
    );
}

} // namespace nodo::core
