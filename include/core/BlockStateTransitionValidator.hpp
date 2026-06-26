#ifndef NODO_CORE_BLOCK_STATE_TRANSITION_VALIDATOR_HPP
#define NODO_CORE_BLOCK_STATE_TRANSITION_VALIDATOR_HPP

#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "utils/Amount.hpp"

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
    DUPLICATE_LEDGER_SOURCE,
    STATE_PREVIEW_FAILED
};

std::string blockValidationStatusToString(
    BlockValidationStatus status
);

enum class BlockValidationMode {
    StructuralOnly,
    ProtocolCommitment
};

class BlockValidationResult {
public:
    BlockValidationResult();

    static BlockValidationResult valid();

    static BlockValidationResult valid(
        std::string stateRoot
    );

    static BlockValidationResult valid(
        std::string stateRoot,
        utils::Amount totalFee,
        std::string receiptsRoot = ""
    );

    static BlockValidationResult rejected(
        BlockValidationStatus status,
        std::string reason
    );

    BlockValidationStatus status() const;
    const std::string& reason() const;
    const std::string& stateRoot() const;
    utils::Amount totalFee() const;
    const std::string& receiptsRoot() const;
    bool accepted() const;

    std::string serialize() const;

private:
    BlockValidationStatus m_status;
    std::string m_reason;
    std::string m_stateRoot;
    utils::Amount m_totalFee;
    std::string m_receiptsRoot;
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
    /*
     * Structural-only overloads. These cannot produce a real account-state root
     * so they default to StructuralOnly mode: block hash, previous-hash linkage,
     * record validity, and duplicate detection are checked, but stateRoot /
     * receiptsRoot commitments are NOT compared against a computed transition.
     *
     * Use the full-context overload below for protocol-level validation.
     */
    static BlockValidationResult validateCandidateBlock(
        const Blockchain& blockchain,
        const Block& candidateBlock,
        BlockValidationMode mode = BlockValidationMode::StructuralOnly
    );

    static BlockValidationResult validateCandidateBlock(
        const Blockchain& blockchain,
        const Block& candidateBlock,
        std::int64_t minimumFeeRawUnits,
        BlockValidationMode mode = BlockValidationMode::StructuralOnly
    );

    /*
     * Full protocol validation. Requires a real AccountStateView in context so
     * the computed stateRoot and receiptsRoot can be compared against the block's
     * declared commitments. Defaults to ProtocolCommitment mode.
     */
    static BlockValidationResult validateCandidateBlock(
        const Blockchain& blockchain,
        const Block& candidateBlock,
        const StateTransitionPreviewContext& context,
        BlockValidationMode mode = BlockValidationMode::ProtocolCommitment
    );
};

} // namespace nodo::core

#endif
