#include "consensus/SlashingEvidence.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"

namespace {

nodo::crypto::PublicKey testValidatorPublicKey() {
    return nodo::crypto::PublicKey(
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        std::string(96, 'a')
    );
}

nodo::crypto::SignatureBundle testSignatureBundle() {
    nodo::crypto::SignatureBundle bundle;
    bundle.addSignature(
        nodo::crypto::Signature(
            nodo::crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
            nodo::crypto::SigningDomain::VALIDATOR_VOTE,
            nodo::crypto::CryptoAlgorithm::BLS12_381,
            testValidatorPublicKey(),
            std::string(96, 'b'),
            100
        )
    );
    return bundle;
}

nodo::consensus::ValidatorVoteRecord makeVote(
    const std::string& blockHash,
    const std::string& previousHash,
    nodo::consensus::ValidatorVoteDecision decision
) {
    return nodo::consensus::ValidatorVoteRecord(
        "validator-alpha",
        testValidatorPublicKey(),
        7,
        blockHash,
        previousHash,
        2,
        decision,
        "reason-hash-alpha",
        100,
        testSignatureBundle()
    );
}

} // namespace

#include <cassert>
#include <iostream>

int main() {
    const auto first = makeVote(
        "block-hash-a",
        "previous-hash",
        nodo::consensus::ValidatorVoteDecision::PRECOMMIT
    );

    const auto second = makeVote(
        "block-hash-b",
        "previous-hash",
        nodo::consensus::ValidatorVoteDecision::PRECOMMIT
    );

    nodo::consensus::DoubleVoteEvidence evidence(first, second, 200);

    assert(evidence.isConflictPair());
    assert(evidence.validatorAddress() == "validator-alpha");
    assert(!evidence.payloadHash().empty());
    assert(!evidence.evidenceId().empty());

    const nodo::consensus::DoubleVoteEvidence reversed(
        second, first, 201
    );
    assert(
        reversed.evidenceId() == evidence.evidenceId() &&
        reversed.payloadHash() == evidence.payloadHash()
    );

    const auto restored =
        nodo::consensus::DoubleVoteEvidence::deserialize(
            evidence.serialize()
        );
    assert(restored.serialize() == evidence.serialize());

    const auto result =
        nodo::consensus::SlashingEvidenceVerifier::validateDoubleVoteStructure(evidence);

    assert(result.accepted());
    assert(result.record().isValid());
    assert(result.record().type() == nodo::consensus::SlashingEvidenceType::DOUBLE_VOTE);
    assert(result.record().severity() == nodo::consensus::SlashingEvidenceSeverity::SLASHABLE);

    std::cout << "slashing double vote evidence tests passed\n";
    return 0;
}
