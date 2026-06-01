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

#include "storage/SlashingEvidenceStore.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        "nodo_slashing_evidence_store_test";

    std::filesystem::remove_all(directory);

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

    nodo::storage::SlashingEvidenceStore store(directory);
    store.save(record);

    assert(store.contains(record.evidenceId()));
    assert(store.count() == 1);

    const auto loaded = store.load(record.evidenceId());
    assert(loaded.evidenceId() == record.evidenceId());
    assert(loaded.validatorAddress() == record.validatorAddress());
    assert(loaded.payloadHash() == record.payloadHash());

    const auto all = store.loadAll();
    assert(all.size() == 1);

    std::filesystem::remove_all(directory);

    std::cout << "slashing evidence store tests passed\n";
    return 0;
}
