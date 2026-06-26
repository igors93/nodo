#include "node/BlockAnnounceHandler.hpp"

#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

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
        "announce-sender",
        "announce-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(10),
        nonce,
        kTimestamp
    );
    const crypto::KeyPair kp = crypto::KeyPair::createDeterministicEd25519KeyPair("announce-key");
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

// Build a context that provides the sender's initial balance so the preview
// can compute real state and receipts roots.
core::StateTransitionPreviewContext senderContext() {
    core::AccountStateView view;
    view.putAccount(core::AccountState(
        "announce-sender",
        utils::Amount::fromRawUnits(1000),
        0
    ));
    return core::StateTransitionPreviewContext(10, view, false, true);
}

p2p::NetworkEnvelope envelopeFor(const core::Block& block) {
    return p2p::NetworkEnvelope(
        "test-net",
        "test-chain",
        "1",
        p2p::NetworkMessageType::BLOCK_ANNOUNCE,
        "peer-a",
        kTimestamp,
        60,
        block.serialize()
    );
}

core::Block buildBlockWithRealRoots(const core::Blockchain& blockchain) {
    const core::Transaction tx = testTx(1);
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

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testRejectsBlockWithWrongStateRoot() {
    core::Blockchain blockchain = chainWithGenesis();

    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(2))},
        kTimestamp + 1,
        kWrongStateRoot,
        kWrongReceiptsRoot
    );

    const node::BlockAnnounceResult result =
        node::BlockAnnounceHandler::processEnvelope(
            envelopeFor(block), blockchain, senderContext()
        );

    requireCondition(
        result.status == node::BlockAnnounceStatus::INVALID_BLOCK,
        "Block with wrong state root must be rejected as INVALID_BLOCK."
    );
    requireCondition(
        blockchain.size() == 1U,
        "Blockchain must not grow after rejection."
    );
}

void testRejectionReasonMentionsMismatch() {
    core::Blockchain blockchain = chainWithGenesis();

    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(1))},
        kTimestamp + 1,
        kWrongStateRoot,
        kWrongReceiptsRoot
    );

    const node::BlockAnnounceResult result =
        node::BlockAnnounceHandler::processEnvelope(
            envelopeFor(block), blockchain, senderContext()
        );

    requireCondition(
        result.reason.find("mismatch") != std::string::npos ||
        result.reason.find("invalid") != std::string::npos,
        "Rejection reason must mention root mismatch or invalidity."
    );
}

void testRejectsBlockWithCorrectStateRootButWrongReceiptsRoot() {
    core::Blockchain blockchain = chainWithGenesis();
    const core::Transaction tx = testTx(1);

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

    // Correct stateRoot but wrong receiptsRoot — must still be rejected.
    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1,
        preview.stateRoot(),
        kWrongReceiptsRoot
    );

    const node::BlockAnnounceResult result =
        node::BlockAnnounceHandler::processEnvelope(
            envelopeFor(block), blockchain, senderContext()
        );

    requireCondition(
        result.status == node::BlockAnnounceStatus::INVALID_BLOCK,
        "Block with wrong receipts root must be rejected as INVALID_BLOCK."
    );
    requireCondition(
        blockchain.size() == 1U,
        "Blockchain must not grow after receipts root rejection."
    );
}

void testAcceptsBlockWithCorrectRoots() {
    core::Blockchain blockchain = chainWithGenesis();
    const core::Block block = buildBlockWithRealRoots(blockchain);

    const node::BlockAnnounceResult result =
        node::BlockAnnounceHandler::processEnvelope(
            envelopeFor(block), blockchain, senderContext()
        );

    requireCondition(
        result.status == node::BlockAnnounceStatus::APPLIED,
        "Block with correct roots must be accepted and applied."
    );
    requireCondition(
        blockchain.size() == 2U,
        "Blockchain must grow by one after successful announce."
    );
}

void testRejectsDuplicateBlock() {
    core::Blockchain blockchain = chainWithGenesis();
    const core::Block block = buildBlockWithRealRoots(blockchain);
    blockchain.addBlock(block);

    const node::BlockAnnounceResult result =
        node::BlockAnnounceHandler::processEnvelope(
            envelopeFor(block), blockchain, senderContext()
        );

    requireCondition(
        result.status == node::BlockAnnounceStatus::ALREADY_KNOWN,
        "Duplicate block must be flagged ALREADY_KNOWN."
    );
}

void testRejectsNonCanonicalStateRoot() {
    core::Blockchain blockchain = chainWithGenesis();

    // Block whose stateRoot fails the canonical format check (< 64 chars).
    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(2))},
        kTimestamp + 1,
        "DRAFT_STATE_ROOT",
        "DRAFT_RECEIPTS_ROOT"
    );

    const node::BlockAnnounceResult result =
        node::BlockAnnounceHandler::processEnvelope(
            envelopeFor(block), blockchain, senderContext()
        );

    // The block is rejected either at decode time (DECODE_FAILED, because
    // BlockCodec enforces canonical roots during deserialization) or at
    // validation time (INVALID_BLOCK). Either rejection is correct; the
    // block must never be applied.
    requireCondition(
        result.status == node::BlockAnnounceStatus::INVALID_BLOCK ||
        result.status == node::BlockAnnounceStatus::DECODE_FAILED,
        "Block with non-canonical roots must be rejected."
    );
    requireCondition(
        blockchain.size() == 1U,
        "Blockchain must not grow after non-canonical root rejection."
    );
}

} // namespace

int main() {
    try {
        testRejectsBlockWithWrongStateRoot();
        testRejectionReasonMentionsMismatch();
        testRejectsBlockWithCorrectStateRootButWrongReceiptsRoot();
        testAcceptsBlockWithCorrectRoots();
        testRejectsDuplicateBlock();
        testRejectsNonCanonicalStateRoot();

        std::cout << "BlockAnnounceHandler protocol validation tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
