#include "node/SlashingEvidenceSync.hpp"

#include "crypto/AddressDerivation.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/SignatureProvider.hpp"
#include "node/SlashingEvidenceMessages.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "storage/SlashingEvidenceStore.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
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

TestKey validatorKey() {
    return {
        crypto::PublicKey(
            crypto::CryptoAlgorithm::BLS12_381,
            std::string(96, 'a')
        ),
        crypto::PrivateKey(
            crypto::CryptoAlgorithm::BLS12_381,
            std::string(64, 'b')
        )
    };
}

consensus::DoubleVoteEvidence evidenceFor(
    const TestKey& key,
    const TestBlsSignatureProvider& provider
) {
    const auto vote = [&](const std::string& blockHash) {
        return consensus::ValidatorVoteRecord::createVote(
            key.address(),
            key.publicKey,
            key.privateKey,
            1,
            blockHash,
            "previous-block",
            1,
            consensus::ValidatorVoteDecision::PRECOMMIT,
            "offline-sync-test",
            kNow - 2,
            provider
        );
    };
    return consensus::DoubleVoteEvidence(
        vote("block-a"), vote("block-b"), kNow - 1
    );
}

core::ValidatorSetHistory historyFor(const TestKey& key) {
    core::ValidatorRegistry registry;
    assert(registry.registerValidator(
        core::ValidatorRegistrationRecord(
            key.address(), key.publicKey, 1, "sync-validator", kNow - 10
        )
    ).success());
    core::ValidatorSetHistory history;
    assert(history.recordSet(1, registry));
    return history;
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

void connect(
    p2p::GossipMesh& meshA,
    p2p::GossipMesh& meshB
) {
    assert(meshA.registerPeer(peer("node-b", 19002)).success());
    assert(meshB.registerPeer(peer("node-a", 19001)).success());
    assert(meshA.connectPeer("node-b").sent());
    assert(meshB.connectPeer("node-a").sent());
}

void testOfflineNodeSynchronizesAndPersistsEvidence() {
    const TestBlsSignatureProvider provider;
    const TestKey key = validatorKey();
    const consensus::DoubleVoteEvidence evidence =
        evidenceFor(key, provider);
    const core::ValidatorSetHistory history = historyFor(key);

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
    connect(meshA, meshB);

    consensus::EvidencePool poolA;
    assert(poolA.submitDoubleVoteEvidence(evidence).accepted());

    const std::filesystem::path evidenceDirectory =
        std::filesystem::temp_directory_path() /
        "nodo-slashing-evidence-offline-sync-test";
    std::error_code cleanupError;
    std::filesystem::remove_all(evidenceDirectory, cleanupError);
    storage::SlashingEvidenceStore storeB(evidenceDirectory);
    consensus::EvidencePool poolB;
    poolB.setPersistence(&storeB);

    node::SlashingEvidenceSync syncA;
    node::SlashingEvidenceSync syncB;

    const auto inventory = syncA.tick(
        meshA,
        poolA,
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        kNow
    );
    assert(inventory.inventoriesBroadcast == 1);
    assert(meshA.flushOutbound(kNow).acceptedCount() == 1);
    assert(meshB.receiveAvailable(kNow).acceptedCount() == 1);

    const auto requested = syncB.tick(
        meshB,
        poolB,
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        kNow
    );
    assert(requested.requestsSent == 1);
    assert(syncB.pendingRequestCount() == 1);
    assert(meshB.flushOutbound(kNow).acceptedCount() == 1);
    assert(meshA.receiveAvailable(kNow).acceptedCount() == 1);

    const auto responded = syncA.tick(
        meshA,
        poolA,
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        kNow
    );
    assert(responded.responsesSent == 1);
    assert(meshA.flushOutbound(kNow).acceptedCount() == 1);
    assert(meshB.receiveAvailable(kNow).acceptedCount() == 1);

    const auto synchronized = syncB.tick(
        meshB,
        poolB,
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        kNow
    );
    assert(synchronized.evidenceAccepted == 1);
    assert(syncB.pendingRequestCount() == 0);
    assert(poolB.contains(evidence.evidenceId()));
    assert(storeB.contains(evidence.evidenceId()));

    (void)syncA.tick(
        meshA,
        poolA,
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        kNow + 30
    );
    assert(meshA.flushOutbound(kNow + 30).acceptedCount() == 1);
    assert(meshB.receiveAvailable(kNow + 30).acceptedCount() == 1);
    const auto duplicateInventory = syncB.tick(
        meshB,
        poolB,
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        kNow + 30
    );
    assert(duplicateInventory.requestsSent == 0);
    assert(poolB.size() == 1);
    assert(meshB.flushOutbound(kNow + 30).acceptedCount() == 1);
    assert(meshA.receiveAvailable(kNow + 30).acceptedCount() == 1);
    const auto mirroredInventory = syncA.tick(
        meshA,
        poolA,
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        kNow + 30
    );
    assert(mirroredInventory.requestsSent == 0);

    const node::SlashingEvidenceResponse unsolicited(
        "localnet", "chain-localnet", "node-a", evidence, kNow + 31
    );
    assert(meshA.sendTo(
        "node-b",
        p2p::NetworkMessageType::SLASHING_EVIDENCE_RESPONSE,
        unsolicited.serialize(),
        kNow + 31
    ).acceptedCount() == 1);
    assert(meshA.flushOutbound(kNow + 31).acceptedCount() == 1);
    assert(meshB.receiveAvailable(kNow + 31).acceptedCount() == 1);
    const auto unsolicitedResult = syncB.tick(
        meshB,
        poolB,
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        kNow + 31
    );
    assert(unsolicitedResult.rejectedMessages == 1);
    assert(poolB.size() == 1);
    assert(meshB.invalidMessageCountForPeer("node-a") == 1);
    assert(meshB.peerRegistry().peer("node-a") != nullptr);
    assert(meshB.peerRegistry().peer("node-a")->score() == -10);
    assert(!meshB.peerRegistry().peer("node-a")->quarantined());

    constexpr std::int64_t floodStartedAt = kNow + 100;
    for (std::size_t index = 0; index < 20; ++index) {
        const std::int64_t requestedAt =
            floodStartedAt + static_cast<std::int64_t>(index);
        const node::SlashingEvidenceRequest request(
            "localnet",
            "chain-localnet",
            "node-b",
            evidence.evidenceId(),
            requestedAt
        );
        assert(meshB.sendTo(
            "node-a",
            p2p::NetworkMessageType::SLASHING_EVIDENCE_REQUEST,
            request.serialize(),
            requestedAt
        ).acceptedCount() == 1);
    }
    assert(meshB.flushOutbound(floodStartedAt + 19).acceptedCount() == 20);
    assert(meshA.receiveAvailable(floodStartedAt + 19).acceptedCount() == 20);
    const auto rateLimited = syncA.tick(
        meshA,
        poolA,
        2,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        floodStartedAt + 19
    );
    assert(rateLimited.responsesSent == 16);
    assert(rateLimited.rateLimitedMessages == 4);
    assert(meshA.invalidMessageCountForPeer("node-b") == 4);
    assert(meshA.peerRegistry().peer("node-b") != nullptr);
    assert(meshA.peerRegistry().peer("node-b")->score() == -20);
    assert(meshA.peerRegistry().peer("node-b")->quarantined());

    std::filesystem::remove_all(evidenceDirectory, cleanupError);
}

} // namespace

int main() {
    testOfflineNodeSynchronizesAndPersistsEvidence();
    return 0;
}
