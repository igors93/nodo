#include "node/SlashingEvidenceGossipAdmission.hpp"

#include "core/ProtocolLimits.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/SignatureProvider.hpp"
#include "node/SlashingEvidenceMessages.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kNow = 1900000000LL;

class TestBlsSignatureProvider final : public crypto::SignatureProvider {
public:
    crypto::CryptoAlgorithm algorithm() const override {
        return crypto::CryptoAlgorithm::BLS12_381;
    }

    crypto::Signature sign(
        const std::string&,
        const crypto::PublicKey& publicKey,
        const crypto::PrivateKey&,
        std::int64_t timestamp,
        crypto::SigningDomain domain
    ) const override {
        return crypto::Signature(
            crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
            domain,
            algorithm(),
            publicKey,
            std::string(192, 'c'),
            timestamp
        );
    }

    crypto::SignatureVerificationResult verify(
        const std::string& message,
        const crypto::Signature& signature
    ) const override {
        return !message.empty() && signature.isValid() &&
               signature.algorithm() == algorithm()
            ? crypto::SignatureVerificationResult::valid()
            : crypto::SignatureVerificationResult::invalid(
                "Invalid deterministic test signature."
            );
    }
};

struct TestKey {
    crypto::PublicKey publicKey;
    crypto::PrivateKey privateKey;

    std::string address() const {
        return crypto::AddressDerivation::deriveFromPublicKey(publicKey).value();
    }
};

TestKey validatorKey(char marker) {
    return TestKey{
        crypto::PublicKey(
            crypto::CryptoAlgorithm::BLS12_381,
            std::string(96, marker)
        ),
        crypto::PrivateKey(
            crypto::CryptoAlgorithm::BLS12_381,
            std::string(64, static_cast<char>(marker + 1))
        )
    };
}

consensus::DoubleVoteEvidence makeEvidence(
    const TestKey& key,
    const TestBlsSignatureProvider& provider
) {
    const auto makeVote = [&](const std::string& blockHash) {
        return consensus::ValidatorVoteRecord::createVote(
            key.address(),
            key.publicKey,
            key.privateKey,
            1,
            blockHash,
            "previous-block",
            1,
            consensus::ValidatorVoteDecision::PRECOMMIT,
            "gossip-admission-test",
            kNow - 2,
            provider
        );
    };
    return consensus::DoubleVoteEvidence(
        makeVote("block-a"), makeVote("block-b"), kNow - 1
    );
}

p2p::PeerMetadata peer(const std::string& nodeId, std::uint16_t port) {
    return p2p::PeerMetadata(
        nodeId,
        p2p::PeerEndpoint("127.0.0.1", port),
        "fingerprint-" + nodeId,
        kNow,
        kNow,
        0,
        false
    );
}

core::ValidatorSetHistory validatorHistory(const TestKey& key) {
    core::ValidatorRegistry validators;
    assert(validators.registerValidator(
        core::ValidatorRegistrationRecord(
            key.address(), key.publicKey, 1, "validator", kNow - 10
        )
    ).success());
    core::ValidatorSetHistory history;
    assert(history.recordSet(1, validators));
    return history;
}

p2p::NetworkEnvelope envelopeFor(
    const std::string& sender,
    const consensus::DoubleVoteEvidence& evidence,
    std::int64_t announcedAt = kNow
) {
    const node::SlashingEvidenceAnnouncement announcement(
        "localnet", "chain-localnet", sender, evidence, announcedAt
    );
    return p2p::NetworkEnvelope(
        "localnet",
        "chain-localnet",
        "1",
        p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE,
        sender,
        announcedAt,
        60,
        announcement.serialize()
    );
}

void testVerifiedEvidenceTravelsBetweenPeers() {
    const TestBlsSignatureProvider provider;
    const TestKey key = validatorKey('a');
    const consensus::DoubleVoteEvidence evidence = makeEvidence(key, provider);
    const core::ValidatorSetHistory history = validatorHistory(key);

    auto bus = std::make_shared<p2p::LoopbackTransportBus>();
    p2p::LoopbackTransport transportA(bus);
    p2p::LoopbackTransport transportB(bus);
    p2p::GossipMesh meshA(
        p2p::GossipMeshConfig(
            "node-a", "localnet", "chain-localnet", "1", "genesis", 60, 4, 100, 50
        ),
        transportA
    );
    p2p::GossipMesh meshB(
        p2p::GossipMeshConfig(
            "node-b", "localnet", "chain-localnet", "1", "genesis", 60, 4, 100, 50
        ),
        transportB
    );
    assert(meshA.registerPeer(peer("node-b", 19002)).success());
    assert(meshB.registerPeer(peer("node-a", 19001)).success());
    assert(meshA.connectPeer("node-b").sent());
    assert(meshB.connectPeer("node-a").sent());

    const node::SlashingEvidenceAnnouncement announcement(
        "localnet", "chain-localnet", "node-a", evidence, kNow
    );
    assert(meshA.broadcast(
        p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE,
        announcement.serialize(),
        kNow
    ).acceptedCount() == 1);
    assert(meshA.flushOutbound(kNow).acceptedCount() == 1);
    assert(meshB.receiveAvailable(kNow).acceptedCount() == 1);

    const auto messages = meshB.drainInbox(
        p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE
    );
    assert(messages.size() == 1);

    consensus::EvidencePool pool;
    node::SlashingEvidenceGossipAdmission admission;
    const auto accepted = admission.admit(
        messages.front(),
        "localnet",
        "chain-localnet",
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        pool,
        kNow
    );
    assert(accepted.accepted());
    assert(pool.contains(evidence.evidenceId()));
    assert(pool.doubleVoteEvidenceById(evidence.evidenceId()) != nullptr);
    assert(
        pool.doubleVoteEvidenceById(evidence.evidenceId())->detectedAt() ==
        kNow
    );

    const auto duplicate = admission.admit(
        messages.front(),
        "localnet",
        "chain-localnet",
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        pool,
        kNow
    );
    assert(duplicate.duplicate());
    assert(pool.size() == 1);
}

void testIdentityBindingAndRateLimit() {
    const TestBlsSignatureProvider provider;
    const TestKey key = validatorKey('a');
    const consensus::DoubleVoteEvidence evidence = makeEvidence(key, provider);
    const core::ValidatorSetHistory history = validatorHistory(key);

    consensus::EvidencePool pool;
    node::SlashingEvidenceGossipAdmission admission;
    const p2p::NetworkEnvelope wrongSender(
        "localnet",
        "chain-localnet",
        "1",
        p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE,
        "node-b",
        kNow,
        60,
        node::SlashingEvidenceAnnouncement(
            "localnet", "chain-localnet", "node-a", evidence, kNow
        ).serialize()
    );
    assert(!admission.admit(
        wrongSender,
        "localnet",
        "chain-localnet",
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        pool,
        kNow
    ).accepted());
    assert(pool.size() == 0);

    const TestKey unknownKey = validatorKey('d');
    const auto unknownValidator = admission.admit(
        envelopeFor("node-a", makeEvidence(unknownKey, provider)),
        "localnet",
        "chain-localnet",
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        pool,
        kNow
    );
    assert(!unknownValidator.accepted());
    assert(pool.size() == 0);

    const p2p::NetworkEnvelope oversized(
        "localnet",
        "chain-localnet",
        "1",
        p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE,
        "oversized-peer",
        kNow,
        60,
        std::string(
            core::ProtocolLimits::MAX_SLASHING_EVIDENCE_GOSSIP_BYTES + 1,
            'x'
        )
    );
    assert(!admission.admit(
        oversized,
        "localnet",
        "chain-localnet",
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        pool,
        kNow
    ).accepted());
    assert(pool.size() == 0);

    node::SlashingEvidenceGossipAdmission floodAdmission;
    for (std::size_t index = 0;
         index < core::ProtocolLimits::MAX_SLASHING_EVIDENCE_PER_PEER_WINDOW;
         ++index) {
        const p2p::NetworkEnvelope malformed(
            "localnet",
            "chain-localnet",
            "1",
            p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE,
            "flood-peer",
            kNow,
            60,
            "malformed-" + std::to_string(index)
        );
        const auto rejected = floodAdmission.admit(
            malformed,
            "localnet",
            "chain-localnet",
            2,
            history,
            crypto::CryptoPolicy::developmentPolicy(),
            provider,
            pool,
            kNow
        );
        assert(!rejected.accepted() && !rejected.rateLimited());
    }

    const auto limited = floodAdmission.admit(
        envelopeFor("flood-peer", evidence),
        "localnet",
        "chain-localnet",
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        pool,
        kNow
    );
    assert(limited.rateLimited());

    const auto nextWindow = floodAdmission.admit(
        envelopeFor("flood-peer", evidence, kNow + 60),
        "localnet",
        "chain-localnet",
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        pool,
        kNow + 60
    );
    assert(nextWindow.accepted());
}

} // namespace

int main() {
    testVerifiedEvidenceTravelsBetweenPeers();
    testIdentityBindingAndRateLimit();
    return 0;
}
