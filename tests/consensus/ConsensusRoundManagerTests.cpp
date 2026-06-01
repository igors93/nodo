#include "consensus/ConsensusRoundManager.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "crypto/CryptoSuiteId.hpp"

#include <cassert>
#include <string>

namespace {

nodo::consensus::ValidatorVoteRecord makeVote(
    const std::string& validatorAddr,
    std::uint64_t blockIndex,
    std::uint64_t round,
    const std::string& blockHash = "hash-A"
) {
    const nodo::crypto::PublicKey pk(
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        std::string(96, 'a')
    );
    nodo::crypto::SignatureBundle bundle;
    bundle.addSignature(nodo::crypto::Signature(
        nodo::crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        nodo::crypto::SigningDomain::VALIDATOR_VOTE,
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        pk,
        std::string(192, 'b'),
        1000
    ));
    return nodo::consensus::ValidatorVoteRecord(
        validatorAddr,
        pk,
        blockIndex,
        blockHash,
        "prev-hash",
        round,
        nodo::consensus::ValidatorVoteDecision::APPROVE,
        "reason-hash",
        1000,
        bundle
    );
}

void testAdvanceAndCurrentState() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(5, 1, "validator-a", 1000000);

    assert(mgr.currentState().height() == 5);
    assert(mgr.currentState().round() == 1);
    assert(mgr.currentState().proposerAddress() == "validator-a");
    assert(mgr.isCurrentRound(5, 1));
    assert(!mgr.isCurrentRound(4, 1));
    assert(!mgr.isCurrentRound(5, 2));
}

void testVoteAcceptedForCurrentRound() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(5, 1, "validator-a", 1000000);

    const auto vote = makeVote("validator-x", 5, 1);
    const auto result = mgr.submitVote(vote);
    assert(result.accepted());
}

void testVoteRejectedForStaleHeight() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(5, 1, "validator-a", 1000000);

    const auto staleVote = makeVote("validator-x", 4, 1);
    const auto result = mgr.submitVote(staleVote);
    assert(!result.accepted());
    assert(result.status() == nodo::consensus::VoteCollectStatus::REJECTED_STALE_ROUND);
}

void testVoteRejectedForStaleRound() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(5, 3, "validator-a", 1000000);

    const auto staleVote = makeVote("validator-x", 5, 2);
    const auto result = mgr.submitVote(staleVote);
    assert(!result.accepted());
    assert(result.status() == nodo::consensus::VoteCollectStatus::REJECTED_STALE_ROUND);
}

void testDuplicateVoteRejectedAsReplay() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(5, 1, "validator-a", 1000000);

    const auto vote = makeVote("validator-x", 5, 1);
    const auto r1 = mgr.submitVote(vote);
    const auto r2 = mgr.submitVote(vote);

    assert(r1.accepted());
    assert(!r2.accepted());
    assert(r2.status() == nodo::consensus::VoteCollectStatus::REJECTED_REPLAY);
}

void testTimeoutExpires() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(1, 1, "validator-a", 1000, 30);

    assert(!mgr.isTimeoutExpired(1010));
    assert(!mgr.isTimeoutExpired(1029));
    assert(mgr.isTimeoutExpired(1030));
    assert(mgr.isTimeoutExpired(9999));
}

void testAdvanceRound() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(3, 1, "validator-a", 1000000);
    mgr.advanceRound(2, "validator-b", 1000001);

    assert(mgr.currentState().height() == 3);
    assert(mgr.currentState().round() == 2);
    assert(mgr.currentState().proposerAddress() == "validator-b");
}

void testRoundStateSerializes() {
    const nodo::consensus::ConsensusRoundState state(5, 2, "validator-a", 1000000);
    const std::string s = state.serialize();
    assert(!s.empty());
    assert(s.find("height=5") != std::string::npos);
    assert(s.find("round=2") != std::string::npos);
}

} // namespace

int main() {
    testAdvanceAndCurrentState();
    testVoteAcceptedForCurrentRound();
    testVoteRejectedForStaleHeight();
    testVoteRejectedForStaleRound();
    testDuplicateVoteRejectedAsReplay();
    testTimeoutExpires();
    testAdvanceRound();
    testRoundStateSerializes();
    return 0;
}
