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

#include "consensus/EvidencePool.hpp"

#include <cassert>
#include <iostream>

int main() {
    const auto first = makeVote(
        "block-hash-a",
        "previous-hash",
        nodo::consensus::ValidatorVoteDecision::APPROVE
    );

    const auto second = makeVote(
        "block-hash-b",
        "previous-hash",
        nodo::consensus::ValidatorVoteDecision::APPROVE
    );

    nodo::consensus::DoubleVoteEvidence evidence(first, second, 200);
    nodo::consensus::EvidencePool pool;

    const auto accepted = pool.submitDoubleVoteEvidence(evidence);
    assert(accepted.accepted());
    assert(pool.size() == 1);
    assert(pool.contains(accepted.record().evidenceId()));
    assert(pool.countForValidator("validator-alpha") == 1);
    assert(pool.allDoubleVoteEvidence().size() == 1);

    const auto duplicate = pool.submitDoubleVoteEvidence(evidence);
    assert(duplicate.duplicate());
    assert(pool.size() == 1);
    assert(pool.isValid());
    assert(pool.removeEvidence(evidence.evidenceId()));
    assert(pool.size() == 0);
    assert(pool.allDoubleVoteEvidence().empty());
    assert(pool.isValid());

    std::cout << "evidence pool tests passed\n";
    return 0;
}
