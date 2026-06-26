#include "node/PersistentBlockStateSync.hpp"

#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "core/ValidatorRegistry.hpp"
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
    const core::ValidatorRegistry registry;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const crypto::Bls12381SignatureProvider provider;

    const core::Block goodBlock = buildBlockWithRealRoots(blockchain, 1);

    const PersistentSyncCheckpoint checkpoint = genesisCheckpoint(blockchain);
    const PersistentBlockSyncBatch batch =
        singleBlockBatch(goodBlock, blockchain.latestBlock().hash());

    const PersistentSyncApplyResult result =
        PersistentBlockStateSyncApplier::applyValidatedBatch(
            checkpoint, batch, blockchain, registry, policy, provider, buildContext, kTimestamp + 3
        );

    requireCondition(
        result.applied(),
        "Batch with correct roots must be accepted."
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

} // namespace

int main() {
    try {
        testApplyValidatedBatchRejectsBlockWithWrongStateRoot();
        testApplyValidatedBatchRejectionReasonMentionsHeight();
        testNoCheckpointAdvanceOnProtocolFailure();
        testApplyValidatedBatchRejectsNonCanonicalRoots();
        testApplyValidatedBatchAcceptsBlockWithCorrectRoots();

        std::cout << "PersistentSync protocol validation tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
