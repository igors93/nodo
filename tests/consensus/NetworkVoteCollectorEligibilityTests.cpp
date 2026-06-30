#include "consensus/NetworkVoteCollector.hpp"
#include "consensus/ValidatorVoteRecord.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::consensus::NetworkVoteCollector;
using nodo::consensus::ValidatorVoteDecision;
using nodo::consensus::ValidatorVoteRecord;
using nodo::consensus::VoteCollectStatus;

void requireCondition(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

nodo::crypto::PublicKey fakeKey(char fill) {
    return nodo::crypto::PublicKey(
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        std::string(96, fill)
    );
}

nodo::crypto::SignatureBundle fakeSig(const nodo::crypto::PublicKey& pk, char fill) {
    nodo::crypto::SignatureBundle bundle;
    bundle.addSignature(
        nodo::crypto::Signature(
            nodo::crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
            nodo::crypto::SigningDomain::VALIDATOR_VOTE,
            nodo::crypto::CryptoAlgorithm::BLS12_381,
            pk,
            std::string(192, fill),
            1000
        )
    );
    return bundle;
}

ValidatorVoteRecord makeVote(
    const std::string& validatorAddr,
    std::uint64_t blockIndex = 1,
    std::uint64_t round = 1
) {
    const auto pk = fakeKey('a');
    return ValidatorVoteRecord(
        validatorAddr,
        pk,
        blockIndex,
        "block-hash-abc",
        "prev-hash-abc",
        round,
        ValidatorVoteDecision::PRECOMMIT,
        "reason-hash",
        1000,
        fakeSig(pk, 'b')
    );
}

nodo::crypto::CryptoPolicy fakePolicy() {
    return nodo::crypto::CryptoPolicy::developmentPolicy();
}

nodo::crypto::Ed25519SignatureProvider fakeProvider() {
    return nodo::crypto::Ed25519SignatureProvider();
}

// ── 1. No eligible list set → all votes pass (backward-compatible) ────────────

void testNoEligibleListAllowsAll() {
    NetworkVoteCollector collector(1, 1);

    const auto result = collector.submitNetworkVote(
        makeVote("validator-x"),
        fakePolicy(),
        fakeProvider()
    );

    // ACCEPTED or pool-level rejection, but NOT REJECTED_NOT_ELIGIBLE
    requireCondition(
        result.status() != VoteCollectStatus::REJECTED_NOT_ELIGIBLE,
        "Without eligibility list, vote must not be rejected as ineligible"
    );
}

// ── 2. Eligible list set → ineligible validator is rejected ──────────────────

void testIneligibleValidatorIsRejected() {
    NetworkVoteCollector collector(1, 1);
    collector.setEligibleValidators({"validator-eligible"});

    const auto result = collector.submitNetworkVote(
        makeVote("validator-not-eligible"),
        fakePolicy(),
        fakeProvider()
    );

    requireCondition(
        result.status() == VoteCollectStatus::REJECTED_NOT_ELIGIBLE,
        "Validator not in eligible list must be rejected as not eligible"
    );
}

// ── 3. Eligible list set → eligible validator passes the guard ────────────────

void testEligibleValidatorPassesGuard() {
    NetworkVoteCollector collector(1, 1);
    collector.setEligibleValidators({"validator-eligible", "validator-b"});

    const auto result = collector.submitNetworkVote(
        makeVote("validator-eligible"),
        fakePolicy(),
        fakeProvider()
    );

    // Must not be rejected for eligibility (may be accepted or fail pool checks)
    requireCondition(
        result.status() != VoteCollectStatus::REJECTED_NOT_ELIGIBLE,
        "Validator in eligible list must not be rejected as ineligible"
    );
}

// ── 4. setEligibleValidators replaces previous list ──────────────────────────

void testSetEligibleValidatorsReplaces() {
    NetworkVoteCollector collector(1, 1);
    collector.setEligibleValidators({"validator-old"});

    // Replace with new list that excludes the old one
    collector.setEligibleValidators({"validator-new"});

    const auto result = collector.submitNetworkVote(
        makeVote("validator-old"),
        fakePolicy(),
        fakeProvider()
    );

    requireCondition(
        result.status() == VoteCollectStatus::REJECTED_NOT_ELIGIBLE,
        "Replacing eligible list must evict previously eligible validators"
    );
}

// ── 5. hasDoubleVote returns false before any vote ────────────────────────────

void testHasDoubleVoteInitiallyFalse() {
    NetworkVoteCollector collector(1, 1);
    const auto vote = makeVote("validator-dvcheck");

    requireCondition(
        !collector.hasDoubleVote(vote),
        "hasDoubleVote must return false before any vote is submitted"
    );
}

// ── 6. Stale-round votes rejected ─────────────────────────────────────────────

void testStaleRoundRejected() {
    NetworkVoteCollector collector(5, 2);

    const auto result = collector.submitNetworkVote(
        makeVote("validator-stale", 4, 1),
        fakePolicy(),
        fakeProvider()
    );

    requireCondition(
        result.status() == VoteCollectStatus::REJECTED_STALE_ROUND,
        "Vote with block index below current height must be rejected as stale"
    );
}

// ── 7. Empty eligible list after reset allows all ─────────────────────────────

void testClearEligibleListAllowsAll() {
    NetworkVoteCollector collector(1, 1);
    collector.setEligibleValidators({"validator-x"});
    collector.setEligibleValidators({});  // empty = no filter

    const auto result = collector.submitNetworkVote(
        makeVote("validator-y"),
        fakePolicy(),
        fakeProvider()
    );

    requireCondition(
        result.status() != VoteCollectStatus::REJECTED_NOT_ELIGIBLE,
        "Empty eligible list must not reject validators"
    );
}

} // namespace

int main() {
    try {
        testNoEligibleListAllowsAll();
        testIneligibleValidatorIsRejected();
        testEligibleValidatorPassesGuard();
        testSetEligibleValidatorsReplaces();
        testHasDoubleVoteInitiallyFalse();
        testStaleRoundRejected();
        testClearEligibleListAllowsAll();

        std::cout << "Nodo NetworkVoteCollector eligibility tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo NetworkVoteCollector eligibility tests FAILED: "
                  << e.what() << "\n";
        return 1;
    }
}
