#include "node/PersistentBlockStateSync.hpp"

#include "config/NetworkParameters.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionExecutionContext.hpp"
#include "core/TransactionType.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "utils/Amount.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;
using namespace nodo::node;

constexpr std::int64_t kTimestamp = 1900000000;

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

// Canonical 64-char hex strings with wrong economic values.
static const std::string kWrongStateRoot =
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
static const std::string kWrongReceiptsRoot =
    "cafebabecafebabecafebabecafebabecafebabecafebabecafebabecafebabe";

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

static std::string getPsyncSenderAddress() {
  const crypto::KeyPair kp =
      crypto::KeyPair::createDeterministicEd25519KeyPair("psync-key");
  return crypto::AddressDerivation::deriveFromPublicKey(kp.publicKey()).value();
}

core::Transaction testTx(std::uint64_t nonce) {
  const std::string senderAddress = getPsyncSenderAddress();
  core::Transaction tx(core::TransactionType::TRANSFER, senderAddress,
                       "psync-recipient", utils::Amount::fromRawUnits(100),
                       utils::Amount::fromRawUnits(10), nonce, kTimestamp);
  tx.withChainId("test-chain");
  const crypto::KeyPair kp =
      crypto::KeyPair::createDeterministicEd25519KeyPair("psync-key");
  const crypto::Ed25519SignatureProvider provider;
  tx.attachSignatureBundle(crypto::SignatureBundle::createSignature(
      tx.signingPayload(), kp.publicKey(), kp.privateKeyForSigningOnly(),
      kTimestamp, provider, crypto::SigningDomain::USER_TRANSACTION));
  return tx;
}

core::LedgerRecord record(const core::Transaction &tx) {
  return core::LedgerRecord::fromTransaction(
      tx, crypto::CryptoPolicy::developmentPolicy(),
      crypto::SecurityContext::USER_TRANSACTION, kTimestamp);
}

core::Blockchain chainWithGenesis() {
  core::Blockchain blockchain;
  blockchain.addGenesisBlock(
      core::Block::createGenesisBlock({record(testTx(1))}, kTimestamp));
  return blockchain;
}

core::StateTransitionPreviewContext senderContext() {
  core::AccountStateView view;
  view.putAccount(core::AccountState(getPsyncSenderAddress(),
                                     utils::Amount::fromRawUnits(1000), 0));
  return core::StateTransitionPreviewContext(
      10, view, false, true, "", 0, "test-chain", "localnet", {}, {},
      []() { return std::make_unique<TestProtocolDomainExecutor>(); }, true);
}

// Context builder for use with applyValidatedBatch.
core::StateTransitionPreviewContext
buildContext(const core::Blockchain & /*chain*/) {
  return senderContext();
}

core::Block buildBlockWithRealRoots(const core::Blockchain &blockchain,
                                    std::uint64_t nonce) {
  const core::Transaction tx = testTx(nonce);
  const core::Block draft(1, blockchain.latestBlock().hash(), {record(tx)},
                          kTimestamp + 1, "", "");
  const core::StateTransitionPreviewResult preview =
      core::StateTransitionPreview::previewBlock(draft, senderContext());
  if (!preview.accepted()) {
    throw std::runtime_error("buildBlockWithRealRoots: preview failed: " +
                             preview.reason());
  }
  return core::Block(1, blockchain.latestBlock().hash(), {record(tx)},
                     kTimestamp + 1, preview.stateRoot(),
                     preview.receiptsRoot());
}

// ── QC helpers
// ────────────────────────────────────────────────────────────────

std::string registerValidator(core::ValidatorRegistry &registry,
                              const crypto::KeyPair &kp,
                              const std::string &seed) {
  const std::string address =
      crypto::AddressDerivation::deriveFromPublicKey(kp.publicKey()).value();
  core::ValidatorRegistrationRecord rec(address, kp.publicKey(), 1,
                                        "meta-" + seed, kTimestamp);
  requireCondition(registry.registerValidator(rec).accepted(),
                   "registerValidator failed: " + seed);
  return address;
}

consensus::FinalizedBlockRecord
buildFinalizedRecord(const core::Block &block,
                     const crypto::KeyPair &validatorKp,
                     const core::ValidatorRegistry &registry) {
  constexpr std::uint64_t kRound = 1;
  const crypto::Bls12381SignatureProvider blsProvider;
  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const std::string address =
      crypto::AddressDerivation::deriveFromPublicKey(validatorKp.publicKey())
          .value();

  const consensus::ValidatorVoteRecord vote =
      consensus::ValidatorVoteRecord::createVote(
          address, validatorKp.publicKey(),
          validatorKp.privateKeyForSigningOnly(), block.index(), block.hash(),
          block.previousHash(), kRound,
          consensus::ValidatorVoteDecision::PRECOMMIT, "reason-" + block.hash(),
          kTimestamp, blsProvider);

  const consensus::QuorumCertificateBuildResult qcResult =
      consensus::QuorumCertificateBuilder::buildFromVotes(
          block.index(), block.hash(), block.previousHash(), kRound, {vote},
          registry, policy, blsProvider);

  requireCondition(qcResult.certified(),
                   "QC build failed: " + qcResult.reason());

  return consensus::FinalizedBlockRecord(block.index(), block.hash(),
                                         block.previousHash(), kRound,
                                         kTimestamp, qcResult.certificate());
}

// Build a PersistentSyncCheckpoint anchored at the genesis block.
PersistentSyncCheckpoint genesisCheckpoint(const core::Blockchain &blockchain) {
  // finalizedStateRoot must be a non-empty safe scalar; use a placeholder
  // since genesis blocks carry no state root.
  return PersistentSyncCheckpoint::genesis(blockchain.latestBlock().hash(),
                                           "genesis-state-root-placeholder",
                                           kTimestamp);
}

// Build a one-item batch containing the given (serialized) block at height 1.
PersistentBlockSyncBatch
singleBlockBatch(const core::Block &block, const std::string &genesisHash,
                 const std::string &finalizedStateRoot = "") {
  const PersistentBlockSyncItem item(
      1, block.hash(), genesisHash, block.serialize(),
      finalizedStateRoot.empty() ? block.stateRoot() : finalizedStateRoot,
      kTimestamp + 1);
  return PersistentBlockSyncBatch("peer-a", 1, 1, {item}, kTimestamp + 2);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testApplyValidatedBatchRejectsBlockWithWrongStateRoot() {
  core::Blockchain blockchain = chainWithGenesis();
  const core::ValidatorRegistry registry;
  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider provider;

  const core::Block badBlock(1, blockchain.latestBlock().hash(),
                             {record(testTx(2))}, kTimestamp + 1,
                             kWrongStateRoot, kWrongReceiptsRoot);

  const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
  const PersistentBlockSyncBatch batch =
      singleBlockBatch(badBlock, blockchain.latestBlock().hash());

  const PersistentSyncApplyResult result =
      PersistentBlockStateSyncApplier::applyValidatedBatch(
          checkpoint, batch, blockchain, registry, policy, provider,
          buildContext, kTimestamp + 3);

  requireCondition(!result.applied(),
                   "Batch with wrong state root must be rejected.");
  requireCondition(result.status() == PersistentSyncApplyStatus::REJECTED,
                   "Rejected batch must have REJECTED status.");
}

void testApplyValidatedBatchRejectionReasonMentionsHeight() {
  core::Blockchain blockchain = chainWithGenesis();
  const core::ValidatorRegistry registry;
  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider provider;

  const core::Block badBlock(1, blockchain.latestBlock().hash(),
                             {record(testTx(2))}, kTimestamp + 1,
                             kWrongStateRoot, kWrongReceiptsRoot);

  const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
  const PersistentBlockSyncBatch batch =
      singleBlockBatch(badBlock, blockchain.latestBlock().hash());

  const PersistentSyncApplyResult result =
      PersistentBlockStateSyncApplier::applyValidatedBatch(
          checkpoint, batch, blockchain, registry, policy, provider,
          buildContext, kTimestamp + 3);

  requireCondition(
      result.reason().find("1") != std::string::npos,
      "Rejection reason must mention the failing block height (1).");
}

void testNoCheckpointAdvanceOnProtocolFailure() {
  core::Blockchain blockchain = chainWithGenesis();
  const core::ValidatorRegistry registry;
  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider provider;

  const core::Block badBlock(1, blockchain.latestBlock().hash(),
                             {record(testTx(2))}, kTimestamp + 1,
                             kWrongStateRoot, kWrongReceiptsRoot);

  const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
  const PersistentBlockSyncBatch batch =
      singleBlockBatch(badBlock, blockchain.latestBlock().hash());

  const PersistentSyncApplyResult result =
      PersistentBlockStateSyncApplier::applyValidatedBatch(
          checkpoint, batch, blockchain, registry, policy, provider,
          buildContext, kTimestamp + 3);

  requireCondition(!result.checkpoint().has_value(),
                   "Rejected batch must not return an advanced checkpoint.");
  requireCondition(
      blockchain.size() == 1U,
      "Blockchain must not grow after persistent sync protocol failure.");
}

void testApplyValidatedBatchRejectsNonCanonicalRoots() {
  core::Blockchain blockchain = chainWithGenesis();
  const core::ValidatorRegistry registry;
  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider provider;

  // Block whose roots fail the canonical format check entirely.
  const core::Block badBlock(1, blockchain.latestBlock().hash(),
                             {record(testTx(2))}, kTimestamp + 1,
                             "DRAFT_STATE_ROOT", "DRAFT_RECEIPTS_ROOT");

  const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
  const PersistentBlockSyncBatch batch =
      singleBlockBatch(badBlock, blockchain.latestBlock().hash());

  const PersistentSyncApplyResult result =
      PersistentBlockStateSyncApplier::applyValidatedBatch(
          checkpoint, batch, blockchain, registry, policy, provider,
          buildContext, kTimestamp + 3);

  requireCondition(!result.applied(),
                   "Batch with non-canonical roots must be rejected.");
}

void testApplyValidatedBatchAcceptsBlockWithCorrectRoots() {
  core::Blockchain blockchain = chainWithGenesis();

  const crypto::KeyPair validatorKey =
      crypto::KeyPair::createDeterministicBls12381KeyPair("protocol-valid-qc");

  core::ValidatorRegistry registry;
  registerValidator(registry, validatorKey, "protocol-valid-qc");

  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider provider;

  const core::Block goodBlock = buildBlockWithRealRoots(blockchain, 1);

  const consensus::FinalizedBlockRecord finalizedRecord =
      buildFinalizedRecord(goodBlock, validatorKey, registry);

  const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);

  const PersistentBlockSyncItem item(
      goodBlock.index(), goodBlock.hash(), goodBlock.previousHash(),
      goodBlock.serialize(), goodBlock.stateRoot(), kTimestamp + 1,
      finalizedRecord.serialize());

  const PersistentBlockSyncBatch batch(
      "peer-a", goodBlock.index(), goodBlock.index(), {item}, kTimestamp + 2);

  const PersistentSyncApplyResult result =
      PersistentBlockStateSyncApplier::applyValidatedBatch(
          checkpoint, batch, blockchain, registry, policy, provider,
          buildContext, kTimestamp + 3);

  requireCondition(result.applied(),
                   "Batch with correct roots and valid QC must be accepted.");
  requireCondition(result.checkpoint().has_value(),
                   "Accepted batch must return an advanced checkpoint.");
  requireCondition(result.checkpoint()->finalizedHeight() == 1,
                   "Advanced checkpoint must be at height 1.");
  requireCondition(
      blockchain.size() == 2U,
      "Blockchain must grow by one after successful persistent sync.");
}

// ── importFinalizedBatch boundary semantics ─────────────────────────────────
//
// These tests pin the self-healing behavior of the runtime-level import: a
// batch that raced against local consensus (the tip advanced between the
// sync request and its response) must be recognized as STALE rather than
// rejected as a failure, an overlapping batch must agree with the local
// chain block-for-block, and a batch that leaves a gap must be rejected
// with a reason that names the heights involved.

constexpr std::int64_t kImportGenesisTimestamp = 1900800000;
constexpr const char *kImportProtocolVersion = "nodo/test";

crypto::KeyPair importValidatorKey() {
  return crypto::KeyPair::createDeterministicBls12381KeyPair(
      "psync-import-validator");
}

config::GenesisConfig importGenesisConfig() {
  return config::GenesisConfig(
      config::NetworkParameters::developmentLocal(), kImportGenesisTimestamp,
      {config::BootstrapValidatorConfig(importValidatorKey().publicKey(), 1, 1,
                                        "psync-import-validator")},
      {}, "psync-import-genesis");
}

struct ImportRuntimeFixture {
  NodeRuntime runtime;
  core::Block canonicalBlock;
};

// Runtime whose canonical chain already contains a block at height 1 — the
// position of a node whose tip advanced while a sync response was in flight.
ImportRuntimeFixture importRuntimeWithCanonicalBlock() {
  const config::GenesisConfig genesis = importGenesisConfig();
  const p2p::PeerInfo peer("psync-import-node", "127.0.0.1:39111",
                           kImportProtocolVersion, 0, kImportGenesisTimestamp);
  const NodeRuntimeStartResult started =
      NodeRuntimeFactory::startFromGenesis(NodeRuntimeConfig(genesis, peer, 8));
  requireCondition(started.started(),
                   "Import fixture runtime failed to start: " +
                       started.reason());

  NodeRuntime runtime = started.runtime();
  const economics::ValidationWorkRecord work(
      importValidatorKey().address().value(), 1,
      economics::ValidationWorkType::VALIDATE_BLOCK,
      economics::ValidationWorkResult::ACCEPTED, "psync-import-target",
      "psync-import-evidence", 1, kImportGenesisTimestamp + 19);
  core::Block canonicalBlock(1, runtime.blockchain().latestBlock().hash(),
                             {core::LedgerRecord::fromValidationWorkRecord(
                                 work, kImportGenesisTimestamp + 19)},
                             kImportGenesisTimestamp + 20);
  runtime.mutableBlockchain().addBlock(canonicalBlock);
  return ImportRuntimeFixture{std::move(runtime), std::move(canonicalBlock)};
}

PersistentSyncCheckpoint checkpointAtTip(const NodeRuntime &runtime) {
  const core::Block &tip = runtime.blockchain().latestBlock();
  return PersistentSyncCheckpoint(
      PersistentSyncCheckpoint::SCHEMA_VERSION, tip.index(), tip.hash(),
      tip.hasCanonicalStateRoot() ? tip.stateRoot() : kWrongStateRoot,
      PersistentSyncStatus::IDLE, "psync-import-node",
      kImportGenesisTimestamp + 30);
}

void testImportFinalizedBatchReportsStaleBatchInsteadOfFailure() {
  ImportRuntimeFixture fixture = importRuntimeWithCanonicalBlock();
  const PersistentSyncCheckpoint checkpoint = checkpointAtTip(fixture.runtime);

  // The batch re-delivers exactly the block the node already finalized.
  const PersistentBlockSyncItem item(
      1, fixture.canonicalBlock.hash(), fixture.canonicalBlock.previousHash(),
      fixture.canonicalBlock.serialize(), kWrongStateRoot,
      kImportGenesisTimestamp + 21);
  const PersistentBlockSyncBatch batch("peer-a", 1, 1, {item},
                                       kImportGenesisTimestamp + 22);
  const NodeDataDirectoryConfig directory(
      std::filesystem::temp_directory_path() / "nodo-psync-import-stale");

  const PersistentSyncApplyResult result =
      PersistentBlockStateSyncApplier::importFinalizedBatch(
          checkpoint, batch, fixture.runtime, directory, nullptr,
          kImportGenesisTimestamp + 40);

  requireCondition(result.status() == PersistentSyncApplyStatus::STALE,
                   "A batch entirely behind the local tip must be STALE, "
                   "got: " +
                       result.serialize());
  requireCondition(!result.checkpoint().has_value(),
                   "A stale batch must not advance the checkpoint.");
  requireCondition(fixture.runtime.blockchain().size() == 2U,
                   "A stale batch must not mutate the local chain.");
}

void testImportFinalizedBatchRejectsOverlapDisagreeingWithLocalChain() {
  ImportRuntimeFixture fixture = importRuntimeWithCanonicalBlock();
  const PersistentSyncCheckpoint checkpoint = checkpointAtTip(fixture.runtime);

  // Same height as the local block, different block hash: the peer is on a
  // different chain, so the batch must be rejected — never treated as stale.
  const PersistentBlockSyncItem item(
      1, kWrongReceiptsRoot, fixture.canonicalBlock.previousHash(),
      fixture.canonicalBlock.serialize(), kWrongStateRoot,
      kImportGenesisTimestamp + 21);
  const PersistentBlockSyncBatch batch("peer-a", 1, 1, {item},
                                       kImportGenesisTimestamp + 22);
  const NodeDataDirectoryConfig directory(
      std::filesystem::temp_directory_path() / "nodo-psync-import-disagree");

  const PersistentSyncApplyResult result =
      PersistentBlockStateSyncApplier::importFinalizedBatch(
          checkpoint, batch, fixture.runtime, directory, nullptr,
          kImportGenesisTimestamp + 40);

  requireCondition(result.status() == PersistentSyncApplyStatus::REJECTED,
                   "An overlapping batch that disagrees with the local chain "
                   "must be rejected.");
  requireCondition(result.reason().find("disagrees") != std::string::npos,
                   "Rejection reason must name the chain disagreement, got: " +
                       result.reason());
}

void testImportFinalizedBatchRejectsBatchStartingPastLocalTip() {
  ImportRuntimeFixture fixture = importRuntimeWithCanonicalBlock();
  const PersistentSyncCheckpoint checkpoint = checkpointAtTip(fixture.runtime);

  // fromHeight 3 with a local tip at height 1 leaves a gap at height 2.
  const PersistentBlockSyncItem item(
      3, fixture.canonicalBlock.hash(), fixture.canonicalBlock.previousHash(),
      fixture.canonicalBlock.serialize(), kWrongStateRoot,
      kImportGenesisTimestamp + 21);
  const PersistentBlockSyncBatch batch("peer-a", 3, 3, {item},
                                       kImportGenesisTimestamp + 22);
  const NodeDataDirectoryConfig directory(
      std::filesystem::temp_directory_path() / "nodo-psync-import-gap");

  const PersistentSyncApplyResult result =
      PersistentBlockStateSyncApplier::importFinalizedBatch(
          checkpoint, batch, fixture.runtime, directory, nullptr,
          kImportGenesisTimestamp + 40);

  requireCondition(result.status() == PersistentSyncApplyStatus::REJECTED,
                   "A batch starting past the local tip must be rejected.");
  requireCondition(
      result.reason().find("starts at height 3") != std::string::npos,
      "Rejection reason must name the gap boundary, got: " + result.reason());
}

void testProtocolCommitmentRejectsItemsWithoutFinalizedRecord() {
  core::Blockchain blockchain = chainWithGenesis();
  const core::ValidatorRegistry registry;
  const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
  const crypto::Bls12381SignatureProvider provider;

  // Use nonce 1 — matches the senderContext() account nonce of 0 (next = 1).
  const core::Block goodBlock = buildBlockWithRealRoots(blockchain, 1);
  const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
  const PersistentBlockSyncBatch batch =
      singleBlockBatch(goodBlock, blockchain.latestBlock().hash());

  const PersistentSyncApplyResult result =
      PersistentBlockStateSyncApplier::applyValidatedBatch(
          checkpoint, batch, blockchain, registry, policy, provider,
          buildContext, kTimestamp + 3);

  requireCondition(
      !result.applied(),
      "Protocol-commitment sync must reject a block without finality proof.");
  requireCondition(blockchain.size() == 1U,
                   "Blockchain must remain unchanged when the QC is missing.");
}

} // namespace

int main() {
  try {
    testApplyValidatedBatchRejectsBlockWithWrongStateRoot();
    testApplyValidatedBatchRejectionReasonMentionsHeight();
    testNoCheckpointAdvanceOnProtocolFailure();
    testApplyValidatedBatchRejectsNonCanonicalRoots();
    testApplyValidatedBatchAcceptsBlockWithCorrectRoots();
    testProtocolCommitmentRejectsItemsWithoutFinalizedRecord();
    testImportFinalizedBatchReportsStaleBatchInsteadOfFailure();
    testImportFinalizedBatchRejectsOverlapDisagreeingWithLocalChain();
    testImportFinalizedBatchRejectsBatchStartingPastLocalTip();

    std::cout << "PersistentSync protocol validation tests passed.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAILED: " << e.what() << '\n';
    return 1;
  }
}
