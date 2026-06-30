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
#include <stdexcept>

namespace {

class TestPersistence final
    : public nodo::consensus::EvidencePoolPersistence {
public:
    bool failWrites = false;
    std::size_t persisted = 0;
    std::size_t erased = 0;

    void persist(
        const nodo::consensus::DoubleVoteEvidence&
    ) override {
        if (failWrites) {
            throw std::runtime_error("test persistence failure");
        }
        ++persisted;
    }

    void persist(
        const nodo::consensus::ProposerEquivocationEvidence&
    ) override {
        if (failWrites) {
            throw std::runtime_error("test persistence failure");
        }
        ++persisted;
    }

    bool erase(const std::string&) override {
        ++erased;
        return true;
    }
};

} // namespace

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
    TestPersistence persistence;
    nodo::consensus::EvidencePool pool;
    pool.setPersistence(&persistence);

    persistence.failWrites = true;
    const auto failedPersistence = pool.submitDoubleVoteEvidence(evidence);
    assert(failedPersistence.rejected());
    assert(pool.size() == 0);

    persistence.failWrites = false;

    const auto accepted = pool.submitDoubleVoteEvidence(evidence);
    assert(accepted.accepted());
    assert(persistence.persisted == 1);
    assert(pool.size() == 1);
    assert(pool.contains(accepted.record().evidenceId()));
    assert(pool.countForValidator("validator-alpha") == 1);
    assert(pool.allDoubleVoteEvidence().size() == 1);
    assert(pool.doubleVoteEvidenceById(evidence.evidenceId()) != nullptr);
    assert(pool.doubleVoteEvidenceBeforeHeight(7).empty());
    assert(pool.doubleVoteEvidenceBeforeHeight(8).size() == 1);

    const auto duplicate = pool.submitDoubleVoteEvidence(evidence);
    assert(duplicate.duplicate());
    assert(pool.size() == 1);
    assert(pool.isValid());
    assert(pool.removeEvidence(evidence.evidenceId()));
    assert(persistence.erased == 1);
    assert(pool.size() == 0);
    assert(pool.allDoubleVoteEvidence().empty());
    assert(pool.isValid());

    std::cout << "evidence pool tests passed\n";
    return 0;
}
