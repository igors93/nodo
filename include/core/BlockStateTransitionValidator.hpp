#ifndef NODO_CORE_BLOCK_STATE_TRANSITION_VALIDATOR_HPP
#define NODO_CORE_BLOCK_STATE_TRANSITION_VALIDATOR_HPP

#include "core/Block.hpp"
#include "core/Blockchain.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

enum class BlockValidationStatus {
    VALID,
    INVALID_BLOCKCHAIN,
    INVALID_BLOCK,
    INVALID_PREVIOUS_HASH,
    INVALID_LEDGER_RECORD,
    INVALID_TRANSACTION,
    DUPLICATE_LEDGER_SOURCE
};

std::string blockValidationStatusToString(
    BlockValidationStatus status
);

class BlockValidationResult {
public:
    BlockValidationResult();

    static BlockValidationResult valid();

    static BlockValidationResult rejected(
        BlockValidationStatus status,
        std::string reason
    );

    BlockValidationStatus status() const;
    const std::string& reason() const;
    bool accepted() const;

    std::string serialize() const;

private:
    BlockValidationStatus m_status;
    std::string m_reason;
};

/*
 * BlockStateTransitionValidator is the protocol gate before validator votes.
 *
 * The validator checks deterministic chain-transition rules that can be audited
 * from the current block and ledger model. It does not mutate state; richer
 * balance, nonce, supply and coin-lot checks can be added here without changing
 * the consensus pipeline shape.
 */
class BlockStateTransitionValidator {
public:
    static BlockValidationResult validateCandidateBlock(
        const Blockchain& blockchain,
        const Block& candidateBlock
    );

    static BlockValidationResult validateCandidateBlock(
        const Blockchain& blockchain,
        const Block& candidateBlock,
        std::int64_t minimumFeeRawUnits
    );
};

} // namespace nodo::core

#endif
