#include "consensus/VotePool.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
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
    nodo::consensus::ValidatorVoteDecision decision,
    std::int64_t createdAt = 1000
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
        createdAt,
        fakeSignatureBundle(publicKey, 'b')
    );
}

} // namespace

int main() {
    const nodo::crypto::ProtocolCryptoContext ctx = nodo::crypto::ProtocolCryptoContext::localnet();
    nodo::consensus::VotePool pool;

    const auto vote = makeVote(
        "validator-A",
        "block-hash-A",
        nodo::consensus::ValidatorVoteDecision::APPROVE
    );

    const auto accepted = pool.submitVote(vote, ctx.policy(), ctx.validatorSignatureProvider());
    assert(accepted.accepted());
    assert(pool.totalVoteCount() == 1);
    assert(pool.voteCountForBlock(1, "block-hash-A", 1) == 1);

    const auto partialProgress =
        pool.quorumProgressForBlock(1, "block-hash-A", 1, 3, 2, 3);

    assert(partialProgress.isValid());
    assert(partialProgress.acceptedVoteCount() == 1);
    assert(partialProgress.requiredVoteCount() == 2);
    assert(!partialProgress.canCertify());

    const auto secondAccepted = pool.submitVote(
        makeVote(
            "validator-B",
            "block-hash-A",
            nodo::consensus::ValidatorVoteDecision::APPROVE
        ),
        ctx.policy(), ctx.validatorSignatureProvider()
    );

    assert(secondAccepted.accepted());
    assert(pool.totalVoteCount() == 2);

    const auto quorumProgress =
        pool.quorumProgressForBlock(1, "block-hash-A", 1, 3, 2, 3);

    assert(quorumProgress.isValid());
    assert(quorumProgress.acceptedVoteCount() == 2);
    assert(quorumProgress.requiredVoteCount() == 2);
    assert(quorumProgress.canCertify());

    const auto duplicate = pool.submitVote(vote, ctx.policy(), ctx.validatorSignatureProvider());
    assert(duplicate.duplicate());

    const auto replaySameSlot = pool.submitVote(
        makeVote(
            "validator-A",
            "block-hash-A",
            nodo::consensus::ValidatorVoteDecision::APPROVE,
            1001
        ),
        ctx.policy(), ctx.validatorSignatureProvider()
    );

    assert(replaySameSlot.duplicate());
    assert(pool.totalVoteCount() == 2);

    const auto conflicting = pool.submitVote(
        makeVote(
            "validator-A",
            "block-hash-B",
            nodo::consensus::ValidatorVoteDecision::APPROVE
        ),
        ctx.policy(), ctx.validatorSignatureProvider()
    );

    assert(conflicting.conflicting());
    assert(pool.hasConflictingVote("validator-A", 1, 1));

    return 0;
}
