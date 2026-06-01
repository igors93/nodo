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

#include "consensus/ValidatorAccountability.hpp"

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

    const nodo::consensus::DoubleVoteEvidence evidence(first, second, 200);
    const auto record = evidence.toRecord();

    nodo::consensus::ValidatorAccountabilityTracker tracker;
    tracker.submitEvidence(record);
    tracker.submitEvidence(record);

    const auto report = tracker.reportForValidator("validator-alpha");

    assert(report.isValid());
    assert(report.status() == nodo::consensus::ValidatorAccountabilityStatus::SLASHABLE);
    assert(report.evidenceCount() == 1);
    assert(report.slashableEvidenceCount() == 1);
    assert(tracker.validatorIsSlashable("validator-alpha"));

    const auto cleanReport = tracker.reportForValidator("validator-beta");
    assert(cleanReport.status() == nodo::consensus::ValidatorAccountabilityStatus::CLEAN);

    std::cout << "validator accountability tests passed\n";
    return 0;
}
