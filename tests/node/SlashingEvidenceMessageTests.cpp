#include "node/SlashingEvidenceMessages.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

nodo::crypto::PublicKey publicKey() {
    return nodo::crypto::PublicKey(
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        std::string(96, 'a')
    );
}

nodo::crypto::SignatureBundle signatureBundle() {
    nodo::crypto::SignatureBundle bundle;
    bundle.addSignature(
        nodo::crypto::Signature(
            nodo::crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
            nodo::crypto::SigningDomain::VALIDATOR_VOTE,
            nodo::crypto::CryptoAlgorithm::BLS12_381,
            publicKey(),
            std::string(192, 'b'),
            100
        )
    );
    return bundle;
}

nodo::consensus::ValidatorVoteRecord vote(const std::string& blockHash) {
    return nodo::consensus::ValidatorVoteRecord(
        "validator-alpha",
        publicKey(),
        7,
        blockHash,
        "previous-hash",
        2,
        nodo::consensus::ValidatorVoteDecision::PRECOMMIT,
        "reason-hash-alpha",
        100,
        signatureBundle()
    );
}

} // namespace

int main() {
    const nodo::consensus::DoubleVoteEvidence evidence(
        vote("block-hash-a"), vote("block-hash-b"), 200
    );
    const nodo::node::SlashingEvidenceAnnouncement announcement(
        "localnet",
        "chain-alpha",
        "node-alpha",
        evidence,
        201
    );

    assert(announcement.isValid());
    assert(announcement.evidence().serialize() == evidence.serialize());
    assert(announcement.record().evidenceId() == evidence.evidenceId());

    const auto restored =
        nodo::node::SlashingEvidenceAnnouncement::deserialize(
            announcement.serialize()
        );
    assert(restored.serialize() == announcement.serialize());
    const std::string firstHash = restored.evidence().firstVote().blockHash();
    const std::string secondHash = restored.evidence().secondVote().blockHash();
    assert(
        (firstHash == "block-hash-a" && secondHash == "block-hash-b") ||
        (firstHash == "block-hash-b" && secondHash == "block-hash-a")
    );

    std::string tampered = announcement.serialize();
    const std::size_t position = tampered.find("block-hash-b");
    assert(position != std::string::npos);
    tampered.replace(position, std::string("block-hash-b").size(), "block-hash-c");
    bool tamperRejected = false;
    try {
        (void)nodo::node::SlashingEvidenceAnnouncement::deserialize(tampered);
    } catch (const std::invalid_argument&) {
        tamperRejected = true;
    }
    assert(tamperRejected);

    const nodo::node::SlashingEvidenceInventory inventory(
        "localnet",
        "chain-alpha",
        "node-alpha",
        {std::string(64, 'b'), evidence.evidenceId()},
        202
    );
    assert(inventory.isValid());
    const auto restoredInventory =
        nodo::node::SlashingEvidenceInventory::deserialize(
            inventory.serialize()
        );
    assert(restoredInventory.serialize() == inventory.serialize());
    assert(restoredInventory.evidenceIds().size() == 2);

    const nodo::node::SlashingEvidenceRequest request(
        "localnet",
        "chain-alpha",
        "node-beta",
        evidence.evidenceId(),
        203
    );
    assert(request.isValid());
    assert(request.evidenceId() == evidence.evidenceId());
    assert(
        nodo::node::SlashingEvidenceRequest::deserialize(
            request.serialize()
        ).serialize() == request.serialize()
    );

    const nodo::node::SlashingEvidenceResponse response(
        "localnet", "chain-alpha", "node-alpha", evidence, 204
    );
    assert(response.isValid());
    assert(
        nodo::node::SlashingEvidenceResponse::deserialize(
            response.serialize()
        ).serialize() == response.serialize()
    );

    std::cout << "slashing evidence messages tests passed\n";
    return 0;
}
