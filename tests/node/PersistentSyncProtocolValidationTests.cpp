#include "node/PersistentBlockStateSync.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;
using namespace nodo::node;

constexpr std::int64_t kTimestamp = 1900000000;

// Canonical 64-char hex strings with wrong economic values.
static const std::string kWrongStateRoot =
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
static const std::string kWrongReceiptsRoot =
    "cafebabecafebabecafebabecafebabecafebabecafebabecafebabecafebabe";

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

core::Transaction testTx(std::uint64_t nonce) {
    core::Transaction tx(
        core::TransactionType::TRANSFER,
        "psync-sender",
        "psync-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(10),
        nonce,
        kTimestamp
    );
    const crypto::KeyPair kp = crypto::KeyPair::createDeterministicEd25519KeyPair("psync-key");
    const crypto::Ed25519SignatureProvider provider;
    tx.attachSignatureBundle(
        crypto::SignatureBundle::createSignature(
            tx.signingPayload(),
            kp.publicKey(),
            kp.privateKeyForSigningOnly(),
            kTimestamp,
            provider,
            crypto::SigningDomain::USER_TRANSACTION
        )
    );
    return tx;
}

core::LedgerRecord record(const core::Transaction& tx) {
    return core::LedgerRecord::fromTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        kTimestamp
    );
}

core::Blockchain chainWithGenesis() {
    core::Blockchain blockchain;
    blockchain.addGenesisBlock(
        core::Block::createGenesisBlock({record(testTx(1))}, kTimestamp)
    );
    return blockchain;
}

core::StateTransitionPreviewContext senderContext() {
    core::AccountStateView view;
    view.putAccount(core::AccountState(
        "psync-sender",
        utils::Amount::fromRawUnits(1000),
        0
    ));
    return core::StateTransitionPreviewContext(10, view, false, true);
}

// Context builder for use with applyValidatedBatch.
core::StateTransitionPreviewContext buildContext(const core::Blockchain& /*chain*/) {
    return senderContext();
}

core::Block buildBlockWithRealRoots(const core::Blockchain& blockchain, std::uint64_t nonce) {
    const core::Transaction tx = testTx(nonce);
    const core::Block draft(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1,
        "",
        ""
    );
    const core::StateTransitionPreviewResult preview =
        core::StateTransitionPreview::previewBlock(draft, senderContext());
    if (!preview.accepted()) {
        throw std::runtime_error("buildBlockWithRealRoots: preview failed: " + preview.reason());
    }
    return core::Block(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1,
        preview.stateRoot(),
        preview.receiptsRoot()
    );
}

// ── QC helpers ────────────────────────────────────────────────────────────────

std::string registerValidator(
    core::ValidatorRegistry& registry,
    const crypto::KeyPair& kp,
    const std::string& seed
) {
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(kp.publicKey()).value();
    core::ValidatorRegistrationRecord rec(
        address, kp.publicKey(), 1, "meta-" + seed, kTimestamp
    );
    requireCondition(
        registry.registerValidator(rec).accepted(),
        "registerValidator failed: " + seed
    );
    return address;
}

consensus::FinalizedBlockRecord buildFinalizedRecord(
    const core::Block& block,
    const crypto::KeyPair& validatorKp,
    const core::ValidatorRegistry& registry
) {
    constexpr std::uint64_t kRound = 1;
    const crypto::Bls12381SignatureProvider blsProvider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(validatorKp.publicKey()).value();

    const consensus::ValidatorVoteRecord vote =
        consensus::ValidatorVoteRecord::createVote(
            address,
            validatorKp.publicKey(),
            validatorKp.privateKeyForSigningOnly(),
            block.index(),
            block.hash(),
            block.previousHash(),
            kRound,
            consensus::ValidatorVoteDecision::APPROVE,
            "reason-" + block.hash(),
            kTimestamp,
            blsProvider
        );

    const consensus::QuorumCertificateBuildResult qcResult =
        consensus::QuorumCertificateBuilder::buildFromVotes(
            block.index(),
            block.hash(),
            block.previousHash(),
            kRound,
            {vote},
            registry,
            policy,
            blsProvider
        );

    requireCondition(qcResult.certified(), "QC build failed: " + qcResult.reason());

    return consensus::FinalizedBlockRecord(
        block.index(),
        block.hash(),
        block.previousHash(),
        kRound,
        kTimestamp,
        qcResult.certificate()
    );
}

// Build a PersistentSyncCheckpoint anchored at the genesis block.
PersistentSyncCheckpoint genesisCheckpoint(const core::Blockchain& blockchain) {
    // finalizedStateRoot must be a non-empty safe scalar; use a placeholder
    // since genesis blocks carry no state root.
    return PersistentSyncCheckpoint::genesis(
        blockchain.latestBlock().hash(),
        "genesis-state-root-placeholder",
        kTimestamp
    );
}

// Build a one-item batch containing the given (serialized) block at height 1.
PersistentBlockSyncBatch singleBlockBatch(
    const core::Block& block,
    const std::string& genesisHash,
    const std::string& finalizedStateRoot = "finalized-state-root-1"
) {
    const PersistentBlockSyncItem item(
        1,
        block.hash(),
        genesisHash,
        block.serialize(),
        finalizedStateRoot,
        kTimestamp + 1
    );
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

    const core::Block badBlock(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(2))},
        kTimestamp + 1,
        kWrongStateRoot,
        kWrongReceiptsRoot
    );

    const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
    const PersistentBlockSyncBatch batch =
        singleBlockBatch(badBlock, blockchain.latestBlock().hash());

    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint, batch, blockchain, registry, policy, provider, buildContext, kTimestamp + 3
        );

    requireCondition(
        !result.applied(),
        "Batch with wrong state root must be rejected."
    );
    requireCondition(
        result.status() == PersistentSyncApplyStatus::REJECTED,
        "Rejected batch must have REJECTED status."
    );
}

void testApplyValidatedBatchRejectionReasonMentionsHeight() {
    core::Blockchain blockchain = chainWithGenesis();
    const core::ValidatorRegistry registry;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const crypto::Bls12381SignatureProvider provider;

    const core::Block badBlock(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(2))},
        kTimestamp + 1,
        kWrongStateRoot,
        kWrongReceiptsRoot
    );

    const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
    const PersistentBlockSyncBatch batch =
        singleBlockBatch(badBlock, blockchain.latestBlock().hash());

    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint, batch, blockchain, registry, policy, provider, buildContext, kTimestamp + 3
        );

    requireCondition(
        result.reason().find("1") != std::string::npos,
        "Rejection reason must mention the failing block height (1)."
    );
}

void testNoCheckpointAdvanceOnProtocolFailure() {
    core::Blockchain blockchain = chainWithGenesis();
    const core::ValidatorRegistry registry;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const crypto::Bls12381SignatureProvider provider;

    const core::Block badBlock(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(2))},
        kTimestamp + 1,
        kWrongStateRoot,
        kWrongReceiptsRoot
    );

    const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
    const PersistentBlockSyncBatch batch =
        singleBlockBatch(badBlock, blockchain.latestBlock().hash());

    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint, batch, blockchain, registry, policy, provider, buildContext, kTimestamp + 3
        );

    requireCondition(
        !result.checkpoint().has_value(),
        "Rejected batch must not return an advanced checkpoint."
    );
    requireCondition(
        blockchain.size() == 1U,
        "Blockchain must not grow after persistent sync protocol failure."
    );
}

void testApplyValidatedBatchRejectsNonCanonicalRoots() {
    core::Blockchain blockchain = chainWithGenesis();
    const core::ValidatorRegistry registry;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const crypto::Bls12381SignatureProvider provider;

    // Block whose roots fail the canonical format check entirely.
    const core::Block badBlock(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(2))},
        kTimestamp + 1,
        "DRAFT_STATE_ROOT",
        "DRAFT_RECEIPTS_ROOT"
    );

    const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
    const PersistentBlockSyncBatch batch =
        singleBlockBatch(badBlock, blockchain.latestBlock().hash());

    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint, batch, blockchain, registry, policy, provider, buildContext, kTimestamp + 3
        );

    requireCondition(
        !result.applied(),
        "Batch with non-canonical roots must be rejected."
    );
}

void testApplyValidatedBatchAcceptsBlockWithCorrectRoots() {
    core::Blockchain blockchain = chainWithGenesis();

    const crypto::KeyPair validatorKey =
        crypto::KeyPair::createDeterministicBls12381KeyPair(
            "protocol-valid-qc"
        );

    core::ValidatorRegistry registry;
    registerValidator(registry, validatorKey, "protocol-valid-qc");

    const crypto::CryptoPolicy policy =
        crypto::CryptoPolicy::developmentPolicy();
    const crypto::Bls12381SignatureProvider provider;

    const core::Block goodBlock =
        buildBlockWithRealRoots(blockchain, 1);

    const consensus::FinalizedBlockRecord finalizedRecord =
        buildFinalizedRecord(goodBlock, validatorKey, registry);

    const PersistentSyncCheckpoint checkpoint =
        genesisCheckpoint(blockchain);

    const PersistentBlockSyncItem item(
        goodBlock.index(),
        goodBlock.hash(),
        goodBlock.previousHash(),
        goodBlock.serialize(),
        goodBlock.stateRoot(),
        kTimestamp + 1,
        finalizedRecord.serialize()
    );

    const PersistentBlockSyncBatch batch(
        "peer-a",
        goodBlock.index(),
        goodBlock.index(),
        {item},
        kTimestamp + 2
    );

    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint,
            batch,
            blockchain,
            registry,
            policy,
            provider,
            buildContext,
            kTimestamp + 3
        );

    requireCondition(
        result.applied(),
        "Batch with correct roots and valid QC must be accepted."
    );
    requireCondition(
        result.checkpoint().has_value(),
        "Accepted batch must return an advanced checkpoint."
    );
    requireCondition(
        result.checkpoint()->finalizedHeight() == 1,
        "Advanced checkpoint must be at height 1."
    );
    requireCondition(
        blockchain.size() == 2U,
        "Blockchain must grow by one after successful persistent sync."
    );
}

// ---------------------------------------------------------------------------
// Fast-path tests (first overload — no Blockchain, requires QC records)
// ---------------------------------------------------------------------------

void testFastPathRejectsMissingFinalizedRecord() {
    const core::Blockchain blockchain = chainWithGenesis();
    const core::ValidatorRegistry registry;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const crypto::Bls12381SignatureProvider provider;

    // Fast-path checks serializedFinalizedRecord.empty() before deserializing
    // the block, so actual block content is irrelevant for this rejection test.
    // Use a string placeholder to avoid any platform-specific preview behaviour.
    const PersistentBlockSyncItem item(
        1,
        "fast-path-block-hash-1",
        blockchain.latestBlock().hash(),
        "Block{index=1}",
        "finalized-state-root-1",
        kTimestamp + 1
        // serializedFinalizedRecord intentionally omitted (defaults to "")
    );
    const PersistentBlockSyncBatch batch("peer-b", 1, 1, {item}, kTimestamp + 2);
    const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);

    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint, batch, registry, policy, provider, kTimestamp + 3
        );

    requireCondition(
        !result.applied(),
        "Fast-path must reject item with missing FinalizedBlockRecord."
    );
    requireCondition(
        result.status() == PersistentSyncApplyStatus::REJECTED,
        "Fast-path rejection must have REJECTED status."
    );
    requireCondition(
        result.reason().find("QuorumCertificate") != std::string::npos ||
        result.reason().find("FinalizedBlockRecord") != std::string::npos,
        "Fast-path rejection reason must mention the missing proof."
    );
}

void testFastPathRejectsWhenAnyItemMissesFinalizedRecord() {
    const core::Blockchain blockchain = chainWithGenesis();
    const core::ValidatorRegistry registry;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const crypto::Bls12381SignatureProvider provider;

    // First item has a non-empty record, second is missing its record.
    // Fast-path rejects before deserializing any block, so string placeholders
    // are sufficient — no StateTransitionPreview is invoked on this path.
    const PersistentBlockSyncItem itemWithRecord(
        1,
        "fast-path-block-hash-1",
        blockchain.latestBlock().hash(),
        "Block{index=1}",
        "finalized-state-root-1",
        kTimestamp + 1,
        "non-empty-placeholder-record"
    );
    const PersistentBlockSyncItem itemWithoutRecord(
        2,
        "fast-path-block-hash-2",
        "fast-path-block-hash-1",
        "Block{index=2}",
        "finalized-state-root-2",
        kTimestamp + 2
        // serializedFinalizedRecord intentionally omitted
    );
    const PersistentBlockSyncBatch batch(
        "peer-c", 1, 2, {itemWithRecord, itemWithoutRecord}, kTimestamp + 3
    );
    const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);

    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint, batch, registry, policy, provider, kTimestamp + 4
        );

    requireCondition(
        !result.applied(),
        "Fast-path must reject batch where any item is missing a FinalizedBlockRecord."
    );
}

void testProtocolCommitmentRejectsItemsWithoutFinalizedRecord() {
    core::Blockchain blockchain = chainWithGenesis();
    const core::ValidatorRegistry registry;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const crypto::Bls12381SignatureProvider provider;

    // Protocol-commitment mode verifies state roots from scratch; QC records
    // are optional (verified as defence-in-depth only when present).
    // Use nonce 1 — matches the senderContext() account nonce of 0 (next = 1).
    const core::Block goodBlock = buildBlockWithRealRoots(blockchain, 1);
    const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
    const PersistentBlockSyncBatch batch =
        singleBlockBatch(goodBlock, blockchain.latestBlock().hash());

    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint, batch, blockchain, registry, policy, provider, buildContext, kTimestamp + 3
        );

    requireCondition(
        !result.applied(),
        "Protocol-commitment sync must reject a block without finality proof."
    );
    requireCondition(
        blockchain.size() == 1U,
        "Blockchain must remain unchanged when the QC is missing."
    );
}

// ---------------------------------------------------------------------------
// Fast-path: accepts a valid QC record and advances the checkpoint
// ---------------------------------------------------------------------------

void testFastPathAcceptsValidQcRecord() {
    // Build a validator and register it.
    const crypto::KeyPair validatorKp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("psync-fast-path-val");
    core::ValidatorRegistry registry;
    registerValidator(registry, validatorKp, "psync-fast-path-val");

    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const crypto::Bls12381SignatureProvider provider;

    // Build a valid block with real state roots so the QC is over real data.
    core::Blockchain blockchain = chainWithGenesis();
    const core::Block goodBlock = buildBlockWithRealRoots(blockchain, 1);

    // Build a structurally and cryptographically valid FinalizedBlockRecord.
    const consensus::FinalizedBlockRecord record =
        buildFinalizedRecord(goodBlock, validatorKp, registry);

    requireCondition(
        record.isStructurallyValid(),
        "FinalizedBlockRecord must be structurally valid."
    );

    // Package it into a fast-path sync batch item.
    const PersistentBlockSyncItem item(
        goodBlock.index(),
        goodBlock.hash(),
        goodBlock.previousHash(),
        goodBlock.serialize(),
        goodBlock.stateRoot(),
        kTimestamp + 1,
        record.serialize()
    );
    const PersistentBlockSyncBatch batch(
        "peer-fast-path",
        goodBlock.index(),
        goodBlock.index(),
        {item},
        kTimestamp + 2
    );
    const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);

    // Fast-path: no contextBuilder, no Blockchain mutation — relies on QC proof.
    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint, batch, registry, policy, provider, kTimestamp + 3
        );

    requireCondition(
        result.applied(),
        "Fast-path must accept batch with a valid FinalizedBlockRecord. "
        "Reason: " + result.reason()
    );
    requireCondition(
        result.checkpoint().has_value(),
        "Fast-path acceptance must return an advanced checkpoint."
    );
    requireCondition(
        result.checkpoint()->finalizedHeight() == goodBlock.index(),
        "Advanced checkpoint must be at the block's height."
    );
    requireCondition(
        result.checkpoint()->finalizedBlockHash() == goodBlock.hash(),
        "Advanced checkpoint must carry the block's hash."
    );
}

} // namespace

int main() {
    try {
        testApplyValidatedBatchRejectsBlockWithWrongStateRoot();
        testApplyValidatedBatchRejectionReasonMentionsHeight();
        testNoCheckpointAdvanceOnProtocolFailure();
        testApplyValidatedBatchRejectsNonCanonicalRoots();
        testApplyValidatedBatchAcceptsBlockWithCorrectRoots();
        testFastPathRejectsMissingFinalizedRecord();
        testFastPathRejectsWhenAnyItemMissesFinalizedRecord();
        testProtocolCommitmentRejectsItemsWithoutFinalizedRecord();
        testFastPathAcceptsValidQcRecord();

        std::cout << "PersistentSync protocol validation tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
