#include "core/Block.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionEngine.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;

constexpr const char *kProtocolChainId = "validator-test-chain";
constexpr const char *kProtocolNetworkName = "localnet";

class TestProtocolDomainExecutor final
    : public core::TransactionDomainExecutor {
public:
  core::TransactionDomainExecutionResult
  applyBurn(const core::Transaction &, const core::AccountStateView &accounts,
            std::uint64_t, std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyStakeDeposit(const core::Transaction &,
                    const core::AccountStateView &accounts, std::uint64_t,
                    std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyStakeUnlock(const core::Transaction &,
                   const core::AccountStateView &accounts, std::uint64_t,
                   std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyStakeWithdraw(const core::Transaction &,
                     const core::AccountStateView &accounts, std::uint64_t,
                     std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyStakeTopUp(const core::Transaction &,
                  const core::AccountStateView &accounts, std::uint64_t,
                  std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyValidatorRegister(const core::Transaction &,
                         const core::AccountStateView &accounts, std::uint64_t,
                         std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyValidatorExitRequest(const core::Transaction &,
                            const core::AccountStateView &accounts,
                            std::uint64_t, std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyValidatorUnjailRequest(const core::Transaction &,
                              const core::AccountStateView &accounts,
                              std::uint64_t, std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyGovernanceProposal(const core::Transaction &,
                          const core::AccountStateView &accounts, std::uint64_t,
                          std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyGovernanceVote(const core::Transaction &,
                      const core::AccountStateView &accounts, std::uint64_t,
                      std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  applyGovernanceExecute(const core::Transaction &,
                         const core::AccountStateView &accounts, std::uint64_t,
                         std::int64_t) override {
    return accepted(accounts);
  }

  core::TransactionDomainExecutionResult
  finalizeBlock(const core::AccountStateView &accounts, utils::Amount,
                const std::vector<core::LedgerRecord> &, std::uint64_t,
                std::int64_t) override {
    return accepted(accounts);
  }

  const std::map<std::string, std::string> &domains() const override {
    return m_domains;
  }

private:
  std::map<std::string, std::string> m_domains{{"test_domain", "stable"}};

  core::TransactionDomainExecutionResult
  accepted(const core::AccountStateView &accounts) {
    return core::TransactionDomainExecutionResult::accepted(accounts,
                                                            m_domains);
  }
};

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

core::Transaction transaction(const std::string &nonce,
                              std::int64_t feeRawUnits = 1) {
  core::Transaction tx(
      core::TransactionType::TRANSFER, "validator-test-sender",
      "validator-test-recipient", utils::Amount::fromRawUnits(100),
      utils::Amount::fromRawUnits(feeRawUnits),
      static_cast<std::uint64_t>(std::stoull(nonce)), kTimestamp);

  const crypto::KeyPair keyPair =
      crypto::KeyPair::createDeterministicEd25519KeyPair(
          "state-transition-validator-key");
  const crypto::Ed25519SignatureProvider provider;

  tx.attachSignatureBundle(crypto::SignatureBundle::createSignature(
      tx.signingPayload(), keyPair.publicKey(),
      keyPair.privateKeyForSigningOnly(), kTimestamp, provider,
      crypto::SigningDomain::USER_TRANSACTION));

  return tx;
}

core::LedgerRecord record(const core::Transaction &tx) {
  return core::LedgerRecord::fromTransaction(
      tx, crypto::CryptoPolicy::developmentPolicy(),
      crypto::SecurityContext::USER_TRANSACTION, kTimestamp);
}

crypto::KeyPair protocolKeyPair() {
  return crypto::KeyPair::createDeterministicEd25519KeyPair(
      "state-transition-authoritative-key");
}

std::string protocolSenderAddress() {
  return crypto::AddressDerivation::deriveFromPublicKey(
             protocolKeyPair().publicKey())
      .value();
}

core::Transaction protocolTransaction(std::uint64_t nonce = 1,
                                      std::int64_t feeRawUnits = 1) {
  const crypto::KeyPair keyPair = protocolKeyPair();
  const crypto::Ed25519SignatureProvider provider;

  core::Transaction tx(
      core::TransactionType::TRANSFER, protocolSenderAddress(),
      "validator-protocol-recipient", utils::Amount::fromRawUnits(100),
      utils::Amount::fromRawUnits(feeRawUnits), nonce, kTimestamp);
  tx.withChainId(kProtocolChainId);
  tx.attachSignatureBundle(crypto::SignatureBundle::createSignature(
      tx.signingPayload(), keyPair.publicKey(),
      keyPair.privateKeyForSigningOnly(), kTimestamp, provider,
      crypto::SigningDomain::USER_TRANSACTION));
  return tx;
}

core::LedgerRecord protocolRecord(const core::Transaction &tx) {
  return core::LedgerRecord::fromTransaction(
      tx, crypto::CryptoPolicy::developmentPolicy(),
      crypto::SecurityContext::USER_TRANSACTION, kTimestamp);
}

core::Blockchain chain() {
  const core::Transaction tx = transaction("1");

  core::Blockchain blockchain;
  blockchain.addGenesisBlock(
      core::Block::createGenesisBlock({record(tx)}, kTimestamp));

  return blockchain;
}

core::StateTransitionPreviewContext
economicContext(std::int64_t senderBalanceRawUnits = 1000,
                std::uint64_t senderNonce = 0,
                std::int64_t minimumFeeRawUnits = 1) {
  core::AccountStateView view;

  if (!view.putAccount(core::AccountState(
          "validator-test-sender",
          utils::Amount::fromRawUnits(senderBalanceRawUnits), senderNonce))) {
    throw std::runtime_error(
        "Failed to create validator preview account state.");
  }

  return core::StateTransitionPreviewContext(minimumFeeRawUnits, view, false,
                                             true);
}

core::StateTransitionPreviewContext
authoritativeContext(std::int64_t senderBalanceRawUnits = 1000,
                     std::uint64_t senderNonce = 0,
                     std::int64_t minimumFeeRawUnits = 1) {
  core::AccountStateView view;

  if (!view.putAccount(core::AccountState(
          protocolSenderAddress(),
          utils::Amount::fromRawUnits(senderBalanceRawUnits), senderNonce))) {
    throw std::runtime_error(
        "Failed to create authoritative validator account state.");
  }

  return core::StateTransitionPreviewContext(
      minimumFeeRawUnits, view, false, true, "", 0, kProtocolChainId,
      kProtocolNetworkName, {}, {},
      []() { return std::make_unique<TestProtocolDomainExecutor>(); }, true);
}

core::Block authoritativeBlockWithRealRoots(
    const core::Blockchain &blockchain,
    const core::StateTransitionPreviewContext &ctx) {
  const core::Transaction tx = protocolTransaction(1, 1);
  const core::LedgerRecord ledgerRecord = protocolRecord(tx);
  const core::Block draft(1, blockchain.latestBlock().hash(), {ledgerRecord},
                          kTimestamp + 1, "", "");

  const core::StateTransitionPreviewResult previewResult =
      core::StateTransitionEngine::executeBlock(draft, ctx);

  if (!previewResult.accepted()) {
    throw std::runtime_error(
        "authoritativeBlockWithRealRoots: engine failed: " +
        previewResult.reason());
  }

  return core::Block(1, blockchain.latestBlock().hash(), {ledgerRecord},
                     kTimestamp + 1, previewResult.stateRoot(),
                     previewResult.receiptsRoot());
}

/*
 * Build the correct stateRoot and receiptsRoot for a block by running preview
 * with the given context, then return a final Block with those real roots.
 */
core::Block blockWithRealRoots(const core::Blockchain &blockchain,
                               const core::Transaction &tx,
                               const core::StateTransitionPreviewContext &ctx) {
  const core::Block draft(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1, "", "");

  const core::StateTransitionPreviewResult previewResult =
      core::StateTransitionPreview::previewBlock(draft, ctx);

  if (!previewResult.accepted()) {
    throw std::runtime_error("blockWithRealRoots: preview failed: " +
                             previewResult.reason());
  }

  return core::Block(1, blockchain.latestBlock().hash(), {record(tx)},
                     kTimestamp + 1, previewResult.stateRoot(),
                     previewResult.receiptsRoot());
}

// ---------------------------------------------------------------------------
// Structural validation tests (StructuralOnly default for 1st/2nd overloads)
// ---------------------------------------------------------------------------

void testAcceptsAppendableCandidate() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2");

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(blockchain,
                                                                  block);

  requireCondition(result.accepted(),
                   "Appendable candidate block should pass structural state "
                   "transition validation.");
}

void testAcceptsCandidateWithValidEconomicState() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2", 5);

  const core::Block block =
      blockWithRealRoots(blockchain, tx, economicContext(1000, 1, 5));

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, economicContext(1000, 1, 5),
          core::BlockValidationMode::StructuralOnly);

  requireCondition(
      result.accepted() && !result.stateRoot().empty(),
      "Candidate block with valid balance and nonce should pass validation.");
}

void testRejectsCandidateWithInsufficientBalance() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2", 5);

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1);

  // StructuralOnly mode: allows empty roots so the state transition
  // (insufficient balance) is the rejection cause, not root validation.
  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, economicContext(104, 1, 5),
          core::BlockValidationMode::StructuralOnly);

  requireCondition(
      !result.accepted() &&
          result.status() == core::BlockValidationStatus::INVALID_TRANSACTION &&
          result.reason().find("insufficient") != std::string::npos,
      "Candidate block with insufficient sender balance should be rejected.");
}

void testRejectsCandidateWithInvalidNonce() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("3", 5);

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1);

  // StructuralOnly mode: the nonce error is the rejection cause.
  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, economicContext(1000, 1, 5),
          core::BlockValidationMode::StructuralOnly);

  requireCondition(
      !result.accepted() &&
          result.status() == core::BlockValidationStatus::INVALID_TRANSACTION &&
          result.reason().find("nonce") != std::string::npos,
      "Candidate block with invalid sender nonce should be rejected.");
}

void testRejectsWrongPreviousHash() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2");

  const core::Block block(1, "wrong-previous-hash", {record(tx)},
                          kTimestamp + 1);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(blockchain,
                                                                  block);

  requireCondition(
      !result.accepted() &&
          result.status() == core::BlockValidationStatus::INVALID_PREVIOUS_HASH,
      "Candidate block with wrong previous hash should be rejected.");
}

void testRejectsDuplicateLedgerSource() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2");

  const core::LedgerRecord ledgerRecord = record(tx);

  const core::Block block(1, blockchain.latestBlock().hash(),
                          {ledgerRecord, ledgerRecord}, kTimestamp + 1);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(blockchain,
                                                                  block);

  requireCondition(
      !result.accepted() &&
          result.status() ==
              core::BlockValidationStatus::DUPLICATE_LEDGER_SOURCE,
      "Candidate block with duplicate ledger source ids should be rejected.");
}

void testRejectsTransactionBelowMinimumFee() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2", 4);

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(blockchain,
                                                                  block, 5);

  requireCondition(
      !result.accepted() &&
          result.status() == core::BlockValidationStatus::INVALID_TRANSACTION &&
          result.reason().find("minimum fee") != std::string::npos,
      "Candidate block with transaction below minimum fee should be rejected.");
}

void testAcceptsTransactionAtMinimumFee() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2", 5);

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(blockchain,
                                                                  block, 5);

  requireCondition(
      result.accepted(),
      "Candidate block with transaction at minimum fee should be accepted.");
}

void testRejectsBlockWithTimestampEqualToPrevious() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2");

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(blockchain,
                                                                  block);

  requireCondition(!result.accepted() &&
                       result.status() ==
                           core::BlockValidationStatus::INVALID_BLOCK &&
                       result.reason().find("timestamp") != std::string::npos,
                   "Block with timestamp equal to previous block should be "
                   "rejected with INVALID_BLOCK and timestamp reason.");
}

void testRejectsBlockWithTimestampBeforePrevious() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2");

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp - 1);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(blockchain,
                                                                  block);

  requireCondition(!result.accepted() &&
                       result.status() ==
                           core::BlockValidationStatus::INVALID_BLOCK &&
                       result.reason().find("timestamp") != std::string::npos,
                   "Block with timestamp before previous block should be "
                   "rejected with INVALID_BLOCK and timestamp reason.");
}

void testRejectsBlockTooFarInFuture() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2");

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1);

  // StructuralOnly: testing timestamp, not protocol commitments.
  const core::StateTransitionPreviewContext context(
      0, core::AccountStateView(), true, false, "", kTimestamp + 1 - 301);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, context,
          core::BlockValidationMode::StructuralOnly);

  requireCondition(!result.accepted() &&
                       result.status() ==
                           core::BlockValidationStatus::INVALID_BLOCK,
                   "Block with timestamp more than 300 seconds in the future "
                   "should be rejected.");
}

void testAcceptsBlockWithinFutureWindow() {
  const core::Blockchain blockchain = chain();

  const core::Transaction tx = transaction("2");

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1);

  // StructuralOnly: testing timestamp, not protocol commitments.
  const core::StateTransitionPreviewContext context(
      0, core::AccountStateView(), true, false, "", kTimestamp + 1 - 299);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, context,
          core::BlockValidationMode::StructuralOnly);

  requireCondition(result.accepted(), "Block with timestamp within 300-second "
                                      "future window should be accepted.");
}

// ---------------------------------------------------------------------------
// Protocol commitment tests (ProtocolCommitment mode, real roots required)
// ---------------------------------------------------------------------------

void testProtocolCommitmentRejectsBlockWithWrongStateRoot() {
  const core::Blockchain blockchain = chain();
  const core::StateTransitionPreviewContext context = authoritativeContext();

  const core::Block validBlock =
      authoritativeBlockWithRealRoots(blockchain, context);
  const core::Block block(
      1, blockchain.latestBlock().hash(), validBlock.records(), kTimestamp + 1,
      "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef",
      validBlock.receiptsRoot());

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, context);

  requireCondition(
      !result.accepted() &&
          result.status() == core::BlockValidationStatus::INVALID_BLOCK &&
          result.reason().find("State root mismatch") != std::string::npos,
      "Authoritative protocol validation must reject a wrong declared state "
      "root.");
}

void testProtocolCommitmentAcceptsBlockWithCorrectStateRoot() {
  const core::Blockchain blockchain = chain();
  const core::StateTransitionPreviewContext context = authoritativeContext();

  const core::Block blockWithRoot =
      authoritativeBlockWithRealRoots(blockchain, context);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, blockWithRoot, context);

  requireCondition(result.accepted(), "Authoritative protocol validation must "
                                      "accept a correct declared state root.");
}

void testProtocolCommitmentRejectsBlockWithWrongReceiptsRoot() {
  const core::Blockchain blockchain = chain();
  const core::StateTransitionPreviewContext context = authoritativeContext();
  const core::Block validBlock =
      authoritativeBlockWithRealRoots(blockchain, context);

  const core::Block blockWithWrongRoot(
      1, blockchain.latestBlock().hash(), validBlock.records(), kTimestamp + 1,
      validBlock.stateRoot(),
      "cafebabecafebabecafebabecafebabecafebabecafebabecafebabecafebabe");

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, blockWithWrongRoot, context);

  requireCondition(
      !result.accepted() &&
          result.status() == core::BlockValidationStatus::INVALID_BLOCK &&
          result.reason().find("Receipts root mismatch") != std::string::npos,
      "Authoritative protocol validation must reject a wrong declared receipts "
      "root.");
}

void testProtocolCommitmentAcceptsBlockWithCorrectReceiptsRoot() {
  const core::Blockchain blockchain = chain();
  const core::StateTransitionPreviewContext context = authoritativeContext();

  const core::Block blockWithCorrectRoot =
      authoritativeBlockWithRealRoots(blockchain, context);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, blockWithCorrectRoot, context);

  requireCondition(result.accepted(),
                   "Authoritative protocol validation must accept a correct "
                   "declared receipts root.");
}

void testBlockReceiptsRootInHeaderPayload() {
  const core::Transaction tx = transaction("1");
  const core::Block blockEmpty(0, "GENESIS", {record(tx)}, kTimestamp, "", "");
  const core::Block blockWithRoot(0, "GENESIS", {record(tx)}, kTimestamp, "",
                                  "some-receipts-root");
  // Different receiptsRoot → different hash (receiptsRoot is in headerPayload).
  requireCondition(
      blockEmpty.hash() != blockWithRoot.hash(),
      "Blocks with different receiptsRoot must have different hashes.");
  // Round-trip: serialize + deserialize preserves receiptsRoot.
  const auto deserialized = core::Block::deserialize(blockWithRoot.serialize());
  requireCondition(deserialized.has_value() &&
                       deserialized->receiptsRoot() == "some-receipts-root",
                   "Block deserialize must preserve receiptsRoot.");
}

// ---------------------------------------------------------------------------
// Protocol commitment enforcement tests
// These tests verify that ProtocolCommitment mode rejects blocks that lack
// real commitments, while StructuralOnly mode does not impose that requirement.
// ---------------------------------------------------------------------------

void testProtocolCommitmentRejectsBlockWithEmptyStateRoot() {
  const core::Blockchain blockchain = chain();
  const core::Transaction tx = transaction("2");

  // receiptsRoot is non-empty but stateRoot is empty.
  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1, "", "some-receipts-root");

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, core::BlockValidationMode::ProtocolCommitment);

  requireCondition(!result.accepted() &&
                       result.status() ==
                           core::BlockValidationStatus::INVALID_BLOCK,
                   "Non-genesis block with empty stateRoot must be rejected in "
                   "ProtocolCommitment mode.");
}

void testProtocolCommitmentRejectsBlockWithEmptyReceiptsRoot() {
  const core::Blockchain blockchain = chain();
  const core::Transaction tx = transaction("2");

  // stateRoot is non-empty but receiptsRoot is empty.
  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1, "some-state-root", "");

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, core::BlockValidationMode::ProtocolCommitment);

  requireCondition(!result.accepted() &&
                       result.status() ==
                           core::BlockValidationStatus::INVALID_BLOCK,
                   "Non-genesis block with empty receiptsRoot must be rejected "
                   "in ProtocolCommitment mode.");
}

void testProtocolCommitmentRejectsBlockWithPlaceholderStateRoot() {
  const core::Blockchain blockchain = chain();
  const core::Transaction tx = transaction("2");

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1, "DRAFT_STATE_ROOT",
                          "some-valid-receipts-root");

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, core::BlockValidationMode::ProtocolCommitment);

  requireCondition(
      !result.accepted() &&
          result.status() == core::BlockValidationStatus::INVALID_BLOCK,
      "Non-genesis block with DRAFT_STATE_ROOT placeholder must be rejected.");
}

void testProtocolCommitmentRejectsBlockWithPlaceholderReceiptsRoot() {
  const core::Blockchain blockchain = chain();
  const core::Transaction tx = transaction("2");

  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1, "some-valid-state-root",
                          "DRAFT_RECEIPTS_ROOT");

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, core::BlockValidationMode::ProtocolCommitment);

  requireCondition(!result.accepted() &&
                       result.status() ==
                           core::BlockValidationStatus::INVALID_BLOCK,
                   "Non-genesis block with DRAFT_RECEIPTS_ROOT placeholder "
                   "must be rejected.");
}

void testStructuralOnlyModeAcceptsBlockWithoutRoots() {
  const core::Blockchain blockchain = chain();
  const core::Transaction tx = transaction("2");

  // Block with no roots at all.
  const core::Block block(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1, "", "");

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, core::BlockValidationMode::StructuralOnly);

  requireCondition(
      result.accepted(),
      "Block without roots must be accepted in StructuralOnly mode.");
}

void testProtocolCommitmentRejectsUnauthorizedContextBeforeRootComparison() {
  // ProtocolCommitment mode is the authoritative protocol gate. It must not
  // execute with a structural-only or otherwise unauthorized context because
  // that would skip chain-bound signature checks and canonical domain
  // execution.
  const core::Blockchain blockchain = chain();
  const core::Transaction tx = transaction("2", 1);

  const core::Block block =
      blockWithRealRoots(blockchain, tx, economicContext(1000, 1, 1));

  // Structural-only context: no account state → computes empty/mismatched
  // roots.
  const core::StateTransitionPreviewContext structuralCtx =
      core::StateTransitionPreviewContext::structuralOnly(0);

  const core::BlockValidationResult result =
      core::BlockStateTransitionValidator::validateCandidateBlock(
          blockchain, block, structuralCtx,
          core::BlockValidationMode::ProtocolCommitment);

  requireCondition(
      !result.accepted() &&
          result.status() ==
              core::BlockValidationStatus::STATE_PREVIEW_FAILED &&
          result.reason().find("Authoritative protocol execution") !=
              std::string::npos,
      "ProtocolCommitment with a structural-only context must be rejected "
      "before root comparison.");
}

} // namespace

int main() {
  try {
    testAcceptsAppendableCandidate();
    testAcceptsCandidateWithValidEconomicState();
    testRejectsCandidateWithInsufficientBalance();
    testRejectsCandidateWithInvalidNonce();
    testRejectsWrongPreviousHash();
    testRejectsDuplicateLedgerSource();
    testRejectsTransactionBelowMinimumFee();
    testAcceptsTransactionAtMinimumFee();
    testRejectsBlockWithTimestampEqualToPrevious();
    testRejectsBlockWithTimestampBeforePrevious();
    testRejectsBlockTooFarInFuture();
    testAcceptsBlockWithinFutureWindow();
    testProtocolCommitmentRejectsBlockWithWrongStateRoot();
    testProtocolCommitmentAcceptsBlockWithCorrectStateRoot();
    testProtocolCommitmentRejectsBlockWithWrongReceiptsRoot();
    testProtocolCommitmentAcceptsBlockWithCorrectReceiptsRoot();
    testBlockReceiptsRootInHeaderPayload();
    testProtocolCommitmentRejectsBlockWithEmptyStateRoot();
    testProtocolCommitmentRejectsBlockWithEmptyReceiptsRoot();
    testProtocolCommitmentRejectsBlockWithPlaceholderStateRoot();
    testProtocolCommitmentRejectsBlockWithPlaceholderReceiptsRoot();
    testStructuralOnlyModeAcceptsBlockWithoutRoots();
    testProtocolCommitmentRejectsUnauthorizedContextBeforeRootComparison();

    std::cout << "Nodo block state transition validator tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo block state transition validator tests failed: "
              << error.what() << "\n";
    return 1;
  }
}
