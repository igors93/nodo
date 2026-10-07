#include "consensus/VotePool.hpp"

#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace {

nodo::crypto::PublicKey fakeValidatorPublicKey(char fill) {
    return nodo::crypto::PublicKey(
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        std::string(96, fill)
    );
}

nodo::crypto::KeyPair validatorKey(const std::string& seed) {
    return nodo::crypto::KeyPair::createDeterministicBls12381KeyPair(
        "vote-pool-validator-" + seed
    );
}

std::string validatorAddress(const std::string& seed) {
    return nodo::crypto::AddressDerivation::deriveFromPublicKey(
        validatorKey(seed).publicKey()
    ).value();
}

nodo::core::ValidatorRegistry validatorRegistry() {
    nodo::core::ValidatorRegistry registry;
    for (const std::string& seed : {"A", "B", "C"}) {
        const auto key = validatorKey(seed);
        const nodo::core::ValidatorRegistrationRecord record(
            validatorAddress(seed),
            key.publicKey(),
            1,
            "vote-pool-meta-" + seed,
            1000
        );
        assert(registry.registerValidator(record).accepted());
    }
    return registry;
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
    const std::string& validatorSeed,
    const std::string& blockHash,
    nodo::consensus::ValidatorVoteDecision decision,
    std::int64_t createdAt = 1000
) {
    const auto publicKey = validatorKey(validatorSeed).publicKey();
    return nodo::consensus::ValidatorVoteRecord(
        validatorAddress(validatorSeed),
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
    const nodo::core::ValidatorRegistry registry = validatorRegistry();

    constexpr std::uint64_t stake =
        nodo::core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS;
    const nodo::consensus::VotePoolQuorumProgress forgedProgress(
        1, "block-hash-A", 1, 2 * stake, 2 * stake, 3 * stake);
    assert(!forgedProgress.isValid());
    assert(!forgedProgress.canCertify());

    const auto vote = makeVote(
        "A",
        "block-hash-A",
        nodo::consensus::ValidatorVoteDecision::PRECOMMIT
    );

    const auto accepted = pool.submitVote(vote, ctx.policy(), ctx.validatorSignatureProvider());
    assert(accepted.accepted());
    assert(pool.totalVoteCount() == 1);
    assert(pool.voteCountForBlock(1, "block-hash-A", 1) == 1);

    const auto partialProgress =
        pool.quorumProgressForBlock(1, "block-hash-A", 1, registry, 2, 3);

    assert(partialProgress.isValid());
    assert(partialProgress.acceptedVotingWeight() ==
           nodo::core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS);
    assert(partialProgress.requiredVotingWeight() ==
           2 * nodo::core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS + 1);
    assert(partialProgress.totalVotingWeight() ==
           3 * nodo::core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS);
    assert(!partialProgress.canCertify());

    const auto secondAccepted = pool.submitVote(
        makeVote(
            "B",
            "block-hash-A",
            nodo::consensus::ValidatorVoteDecision::PRECOMMIT
        ),
        ctx.policy(), ctx.validatorSignatureProvider()
    );

    assert(secondAccepted.accepted());
    assert(pool.totalVoteCount() == 2);

    const auto quorumProgress =
        pool.quorumProgressForBlock(1, "block-hash-A", 1, registry, 2, 3);

    assert(quorumProgress.isValid());
    assert(quorumProgress.acceptedVotingWeight() ==
           2 * nodo::core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS);
    assert(quorumProgress.requiredVotingWeight() ==
           2 * nodo::core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS + 1);
    assert(!quorumProgress.canCertify());

    const auto thirdAccepted = pool.submitVote(
        makeVote("C", "block-hash-A",
                 nodo::consensus::ValidatorVoteDecision::PRECOMMIT),
        ctx.policy(), ctx.validatorSignatureProvider());
    assert(thirdAccepted.accepted());
    const auto strictQuorumProgress =
        pool.quorumProgressForBlock(1, "block-hash-A", 1, registry, 2, 3);
    assert(strictQuorumProgress.acceptedVotingWeight() ==
           3 * nodo::core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS);
    assert(strictQuorumProgress.canCertify());

    const auto duplicate = pool.submitVote(vote, ctx.policy(), ctx.validatorSignatureProvider());
    assert(duplicate.duplicate());

    const auto replaySameSlot = pool.submitVote(
        makeVote(
            "A",
            "block-hash-A",
            nodo::consensus::ValidatorVoteDecision::PRECOMMIT,
            1001
        ),
        ctx.policy(), ctx.validatorSignatureProvider()
    );

    assert(replaySameSlot.duplicate());
    assert(pool.totalVoteCount() == 3);

    const auto conflicting = pool.submitVote(
        makeVote(
            "A",
            "block-hash-B",
            nodo::consensus::ValidatorVoteDecision::PRECOMMIT
        ),
        ctx.policy(), ctx.validatorSignatureProvider()
    );

    assert(conflicting.conflicting());
    assert(pool.hasConflictingVote(validatorAddress("A"), 1, 1));

    return 0;
}
