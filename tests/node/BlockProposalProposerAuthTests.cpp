#include "node/SignedBlockProposalMessage.hpp"

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

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000LL;
constexpr std::uint64_t kRound    = 1;

void requireCondition(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

// Signed transfer transaction with the given nonce.
core::Transaction makeTx(std::uint64_t nonce) {
    core::Transaction tx(
        core::TransactionType::TRANSFER,
        "prop-sender",
        "prop-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(10),
        nonce,
        kTimestamp
    );
    const crypto::KeyPair kp = crypto::KeyPair::createDeterministicEd25519KeyPair("prop-test-key");
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

core::LedgerRecord makeTxRecord(const core::Transaction& tx) {
    return core::LedgerRecord::fromTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        kTimestamp
    );
}

core::StateTransitionPreviewContext senderContext() {
    core::AccountStateView view;
    view.putAccount(core::AccountState(
        "prop-sender",
        utils::Amount::fromRawUnits(1000),
        0
    ));
    return core::StateTransitionPreviewContext(10, view, false, true);
}

// Returns a structurally valid non-genesis block at index=1 with real state roots.
// SignedBlockProposalMessage::isValid() requires blockIndex > 0.
core::Block makeTestBlock() {
    const core::Transaction tx = makeTx(1);
    const core::LedgerRecord rec = makeTxRecord(tx);

    const core::Block genesis =
        core::Block::createGenesisBlock({rec}, kTimestamp);

    const core::Block draft(
        1,
        genesis.hash(),
        {rec},
        kTimestamp + 1,
        "",
        ""
    );

    const core::StateTransitionPreviewResult preview =
        core::StateTransitionPreview::previewBlock(draft, senderContext());
    if (!preview.accepted()) {
        throw std::runtime_error(
            "makeTestBlock: preview failed: " + preview.reason()
        );
    }

    return core::Block(
        1,
        genesis.hash(),
        {rec},
        kTimestamp + 1,
        preview.stateRoot(),
        preview.receiptsRoot()
    );
}

// Build and register an active validator. Returns the validator address.
std::string registerActiveValidator(
    core::ValidatorRegistry& registry,
    const crypto::KeyPair& keyPair,
    const std::string& seed
) {
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(keyPair.publicKey()).value();

    core::ValidatorRegistrationRecord rec(
        address,
        keyPair.publicKey(),
        /*activationEpoch=*/1,
        "meta-" + seed,
        kTimestamp
    );

    const auto result = registry.registerValidator(rec);
    if (!result.accepted()) {
        throw std::runtime_error("registerValidator failed for seed: " + seed);
    }
    return address;
}

// ── 1. Valid proposal: sign and verify round-trips ────────────────────────────

void testValidProposalVerifies() {
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("auth-test-proposer");
    core::ValidatorRegistry registry;
    const std::string address = registerActiveValidator(registry, kp, "auth-test-proposer");

    const crypto::Bls12381SignatureProvider provider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();

    const core::Block block = makeTestBlock();
    requireCondition(block.isValid(), "test block must pass isValid()");

    const node::SignedBlockProposalMessage proposal =
        node::SignedBlockProposalMessage::sign(
            block,
            address,
            kp.publicKey(),
            kp.privateKeyForSigningOnly(),
            kRound,
            kTimestamp,
            provider
        );

    requireCondition(proposal.isValid(),
        "Signed proposal must be structurally valid.");
    requireCondition(
        proposal.verify(address, registry, policy, provider),
        "Signed proposal must verify successfully against the registry."
    );
}

// ── 2. Wrong expectedProposer: verify rejects ────────────────────────────────

void testWrongExpectedProposerRejected() {
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("auth-test-wrong-proposer");
    core::ValidatorRegistry registry;
    const std::string address = registerActiveValidator(registry, kp, "auth-test-wrong-proposer");

    const crypto::Bls12381SignatureProvider provider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();

    const core::Block block = makeTestBlock();
    const node::SignedBlockProposalMessage proposal =
        node::SignedBlockProposalMessage::sign(
            block, address, kp.publicKey(), kp.privateKeyForSigningOnly(),
            kRound, kTimestamp, provider
        );

    requireCondition(
        !proposal.verify("nodo1wrongaddress", registry, policy, provider),
        "Proposal with mismatched expectedProposer must be rejected."
    );
}

// ── 3. Unregistered proposer: verify rejects ─────────────────────────────────

void testUnregisteredProposerRejected() {
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("auth-test-unregistered");
    core::ValidatorRegistry emptyRegistry;

    const crypto::Bls12381SignatureProvider provider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();

    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(kp.publicKey()).value();

    const core::Block block = makeTestBlock();
    const node::SignedBlockProposalMessage proposal =
        node::SignedBlockProposalMessage::sign(
            block, address, kp.publicKey(), kp.privateKeyForSigningOnly(),
            kRound, kTimestamp, provider
        );

    requireCondition(
        !proposal.verify(address, emptyRegistry, policy, provider),
        "Proposal from an unregistered proposer must be rejected."
    );
}

// ── 4. Different key pair: verify rejects (signature mismatch) ───────────────

void testSignatureFromDifferentKeyRejected() {
    const crypto::KeyPair kpLegit  =
        crypto::KeyPair::createDeterministicBls12381KeyPair("auth-test-legit");
    const crypto::KeyPair kpAttack =
        crypto::KeyPair::createDeterministicBls12381KeyPair("auth-test-attack");

    core::ValidatorRegistry registry;
    const std::string address = registerActiveValidator(registry, kpLegit, "auth-test-legit");

    const crypto::Bls12381SignatureProvider provider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();

    const core::Block block = makeTestBlock();

    // Sign with the attacker's key but claim to be the legitimate proposer.
    const node::SignedBlockProposalMessage proposal =
        node::SignedBlockProposalMessage::sign(
            block, address, kpAttack.publicKey(), kpAttack.privateKeyForSigningOnly(),
            kRound, kTimestamp, provider
        );

    // The registered key doesn't match the key material in the message.
    requireCondition(
        !proposal.verify(address, registry, policy, provider),
        "Proposal signed with a different key must be rejected."
    );
}

// ── 5. Serialize/deserialize roundtrip preserves all fields ──────────────────

void testSerializeDeserializeRoundtrip() {
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("auth-test-roundtrip");
    core::ValidatorRegistry registry;
    const std::string address = registerActiveValidator(registry, kp, "auth-test-roundtrip");

    const crypto::Bls12381SignatureProvider provider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();

    const core::Block block = makeTestBlock();
    const node::SignedBlockProposalMessage original =
        node::SignedBlockProposalMessage::sign(
            block, address, kp.publicKey(), kp.privateKeyForSigningOnly(),
            kRound, kTimestamp, provider
        );

    const std::string serialized = original.serialize();
    requireCondition(!serialized.empty(), "Serialized proposal must not be empty.");

    const node::SignedBlockProposalMessage restored =
        node::SignedBlockProposalMessage::deserialize(serialized);
    requireCondition(restored.isValid(), "Deserialized proposal must be structurally valid.");

    requireCondition(
        restored.proposerAddress() == original.proposerAddress(),
        "proposerAddress must survive roundtrip."
    );
    requireCondition(
        restored.blockIndex() == original.blockIndex(),
        "blockIndex must survive roundtrip."
    );
    requireCondition(
        restored.blockHash() == original.blockHash(),
        "blockHash must survive roundtrip."
    );
    requireCondition(
        restored.round() == original.round(),
        "round must survive roundtrip."
    );
    requireCondition(
        restored.proposedAt() == original.proposedAt(),
        "proposedAt must survive roundtrip."
    );
    requireCondition(
        restored.serializedBlock() == original.serializedBlock(),
        "serializedBlock bytes must survive roundtrip."
    );

    requireCondition(
        restored.verify(address, registry, policy, provider),
        "Deserialized proposal must still verify successfully."
    );
}

// ── 6. Tampered block hash in proposal is rejected ───────────────────────────

void testTamperedProposalRejected() {
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("auth-test-tampered");
    core::ValidatorRegistry registry;
    const std::string address = registerActiveValidator(registry, kp, "auth-test-tampered");

    const crypto::Bls12381SignatureProvider provider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();

    const core::Block block = makeTestBlock();
    const node::SignedBlockProposalMessage original =
        node::SignedBlockProposalMessage::sign(
            block, address, kp.publicKey(), kp.privateKeyForSigningOnly(),
            kRound, kTimestamp, provider
        );

    // Tamper: construct a proposal with a different blockHash but the same signature.
    const node::SignedBlockProposalMessage tampered(
        original.proposerAddress(),
        original.proposerPublicKey(),
        original.blockIndex(),
        original.round(),
        "tampered-block-hash-xyz",
        original.serializedBlock(),
        original.proposedAt(),
        original.signatureBundle()
    );

    requireCondition(
        !tampered.verify(address, registry, policy, provider),
        "Proposal with tampered block hash must fail verification."
    );
}

// ── 7. Default-constructed (empty) proposal is not valid ─────────────────────

void testDefaultConstructedProposalIsInvalid() {
    const node::SignedBlockProposalMessage empty;
    requireCondition(
        !empty.isValid(),
        "Default-constructed proposal must not be structurally valid."
    );
}

} // namespace

int main() {
    try {
        testValidProposalVerifies();
        testWrongExpectedProposerRejected();
        testUnregisteredProposerRejected();
        testSignatureFromDifferentKeyRejected();
        testSerializeDeserializeRoundtrip();
        testTamperedProposalRejected();
        testDefaultConstructedProposalIsInvalid();
        std::cout << "Nodo block proposal proposer auth tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo block proposal proposer auth tests failed: " << e.what() << "\n";
        return 1;
    }
}
