#include "consensus/VotePool.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"

#include <cassert>
#include <string>

namespace {

nodo::crypto::PublicKey fakeValidatorPublicKey(char fill) {
    return nodo::crypto::PublicKey(
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        std::string(96, fill)
    );
}

nodo::crypto::SignatureBundle fakeSignatureBundle(
    const nodo::crypto::PublicKey& publicKey,
    char fill
) {
    nodo::crypto::SignatureBundle bundle;
    bundle.addSignature(
        nodo::crypto::Signature(
            nodo::crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
            nodo::crypto::SigningDomain::VALIDATOR_VOTE,
            nodo::crypto::CryptoAlgorithm::BLS12_381,
            publicKey,
            std::string(192, fill),
            1000
        )
    );
    return bundle;
}

nodo::consensus::ValidatorVoteRecord makeVote(
    const std::string& validator,
    const std::string& blockHash,
    nodo::consensus::ValidatorVoteDecision decision
) {
    const auto publicKey = fakeValidatorPublicKey('a');
    return nodo::consensus::ValidatorVoteRecord(
        validator,
        publicKey,
        1,
        blockHash,
        "previous-hash",
        1,
        decision,
        "reason-hash",
        1000,
        fakeSignatureBundle(publicKey, 'b')
    );
}

} // namespace

int main() {
    nodo::consensus::VotePool pool;

    const auto vote = makeVote(
        "validator-A",
        "block-hash-A",
        nodo::consensus::ValidatorVoteDecision::APPROVE
    );

    const auto accepted = pool.submitVote(vote);
    assert(accepted.accepted());
    assert(pool.totalVoteCount() == 1);
    assert(pool.voteCountForBlock(1, "block-hash-A", 1) == 1);

    const auto duplicate = pool.submitVote(vote);
    assert(duplicate.duplicate());

    const auto conflicting = pool.submitVote(
        makeVote(
            "validator-A",
            "block-hash-B",
            nodo::consensus::ValidatorVoteDecision::APPROVE
        )
    );

    assert(conflicting.conflicting());
    assert(pool.hasConflictingVote("validator-A", 1, 1));

    return 0;
}
