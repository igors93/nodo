#ifndef NODO_CORE_BLOCK_STATE_TRANSITION_VALIDATOR_HPP
#define NODO_CORE_BLOCK_STATE_TRANSITION_VALIDATOR_HPP

#include "core/AccountState.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

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

std::string blockValidationStatusToString(BlockValidationStatus status);

enum class BlockValidationMode { StructuralOnly, ProtocolCommitment };

class BlockValidationResult {
public:
  BlockValidationResult();

  static BlockValidationResult valid();

  static BlockValidationResult valid(std::string stateRoot);

  static BlockValidationResult
  valid(std::string stateRoot, utils::Amount totalFee,
        std::string receiptsRoot = "",
        std::vector<AccountState> resultingAccounts = {});

  static BlockValidationResult rejected(BlockValidationStatus status,
                                        std::string reason);

  BlockValidationStatus status() const;
  const std::string &reason() const;
  const std::string &stateRoot() const;
  utils::Amount totalFee() const;
  const std::string &receiptsRoot() const;
  const std::vector<AccountState> &resultingAccounts() const;
  bool accepted() const;

  std::string serialize() const;

private:
  BlockValidationStatus m_status;
  std::string m_reason;
  std::string m_stateRoot;
  utils::Amount m_totalFee;
  std::string m_receiptsRoot;
  std::vector<AccountState> m_resultingAccounts;
};

/*
 * BlockStateTransitionValidator is the protocol gate before validator votes.
 *
 * StructuralOnly mode performs non-authoritative structural/economic preview
 * checks through StateTransitionPreview and never validates header commitments.
 * ProtocolCommitment mode is the consensus path: it enters
 * StateTransitionEngine, which requires a complete authoritative protocol
 * context and compares computed stateRoot/receiptsRoot values against the
 * candidate block header. The validator does not mutate runtime state.
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
      const Blockchain &blockchain, const Block &candidateBlock,
      BlockValidationMode mode = BlockValidationMode::StructuralOnly);

  static BlockValidationResult validateCandidateBlock(
      const Blockchain &blockchain, const Block &candidateBlock,
      std::int64_t minimumFeeRawUnits,
      BlockValidationMode mode = BlockValidationMode::StructuralOnly);

  /*
   * Full protocol validation. ProtocolCommitment requires an authoritative
   * context: enforced account state, no missing-account fallback, chain id and
   * crypto context for signature checks, and the canonical protocol-domain
   * executor. Defaults to ProtocolCommitment mode.
   */
  static BlockValidationResult validateCandidateBlock(
      const Blockchain &blockchain, const Block &candidateBlock,
      const StateTransitionPreviewContext &context,
      BlockValidationMode mode = BlockValidationMode::ProtocolCommitment);
};

} // namespace nodo::core

#endif
