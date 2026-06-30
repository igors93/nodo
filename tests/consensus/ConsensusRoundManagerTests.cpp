#include "consensus/ConsensusRoundManager.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
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
    const std::string& blockHash = "hash-A",
    nodo::consensus::ValidatorVoteDecision decision = nodo::consensus::ValidatorVoteDecision::PRECOMMIT
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
        decision,
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

    const nodo::crypto::ProtocolCryptoContext ctx = nodo::crypto::ProtocolCryptoContext::localnet();
    const auto vote = makeVote("validator-x", 5, 1);
    const auto result = mgr.submitVote(vote, ctx.policy(), ctx.validatorSignatureProvider());
    assert(result.accepted());
}

void testVoteRejectedForStaleHeight() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(5, 1, "validator-a", 1000000);

    const nodo::crypto::ProtocolCryptoContext ctx = nodo::crypto::ProtocolCryptoContext::localnet();
    const auto staleVote = makeVote("validator-x", 4, 1);
    const auto result = mgr.submitVote(staleVote, ctx.policy(), ctx.validatorSignatureProvider());
    assert(!result.accepted());
    assert(result.status() == nodo::consensus::VoteCollectStatus::REJECTED_STALE_ROUND);
}

void testVoteRejectedForStaleRound() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(5, 3, "validator-a", 1000000);

    const nodo::crypto::ProtocolCryptoContext ctx = nodo::crypto::ProtocolCryptoContext::localnet();
    const auto staleVote = makeVote("validator-x", 5, 2);
    const auto result = mgr.submitVote(staleVote, ctx.policy(), ctx.validatorSignatureProvider());
    assert(!result.accepted());
    assert(result.status() == nodo::consensus::VoteCollectStatus::REJECTED_STALE_ROUND);
}

void testDuplicateVoteRejectedAsReplay() {
    nodo::consensus::ConsensusRoundManager mgr;
    mgr.advanceToHeight(5, 1, "validator-a", 1000000);

    const nodo::crypto::ProtocolCryptoContext ctx = nodo::crypto::ProtocolCryptoContext::localnet();
    const auto vote = makeVote("validator-x", 5, 1);
    const auto r1 = mgr.submitVote(vote, ctx.policy(), ctx.validatorSignatureProvider());
    const auto r2 = mgr.submitVote(vote, ctx.policy(), ctx.validatorSignatureProvider());

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

void testRoundStateSerializesLockFields() {
    const auto prevote = makeVote(
        "validator-x",
        7,
        3,
        "hashofblock",
        nodo::consensus::ValidatorVoteDecision::PREVOTE
    );
    const auto precommit = makeVote(
        "validator-x",
        7,
        3,
        "hashofblock",
        nodo::consensus::ValidatorVoteDecision::PRECOMMIT
    );
    const nodo::consensus::ConsensusRoundState state(
        7, 3, "validator-b", 2000000,
        "hashofblock", 3, true, true,
        prevote, precommit
    );
    const std::string s = state.serialize();
    assert(s.find("lockedBlockHash=hashofblock") != std::string::npos);
    assert(s.find("lockedRound=3") != std::string::npos);
    assert(s.find("votedPrevote=1") != std::string::npos);
    assert(s.find("votedPrecommit=1") != std::string::npos);
}

void testRoundStateDeserializeRoundTrip() {
    const auto prevote = makeVote(
        "validator-x",
        12,
        4,
        "abcdef123456",
        nodo::consensus::ValidatorVoteDecision::PREVOTE
    );
    const nodo::consensus::ConsensusRoundState original(
        12, 4, "validator-c", 3000000,
        "abcdef123456", 4, true, false,
        prevote, std::nullopt
    );
    const std::string serialized = original.serialize();
    const nodo::consensus::ConsensusRoundState recovered =
        nodo::consensus::ConsensusRoundState::deserialize(serialized);

    assert(recovered.height() == 12);
    assert(recovered.round() == 4);
    assert(recovered.proposerAddress() == "validator-c");
    assert(recovered.roundStartedAt() == 3000000);
    assert(recovered.lockedBlockHash() == "abcdef123456");
    assert(recovered.lockedRound() == 4);
    assert(recovered.votedPrevote() == true);
    assert(recovered.votedPrecommit() == false);
    assert(recovered.persistedPrevote().has_value());
    assert(recovered.persistedPrevote()->serialize() == prevote.serialize());
}


void testRoundStateDefaultLockFields() {
    const nodo::consensus::ConsensusRoundState state(1, 1, "validator-a", 1000000);
    assert(state.lockedBlockHash() == "");
    assert(state.lockedRound() == 0);
    assert(state.votedPrevote() == false);
    assert(state.votedPrecommit() == false);
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
    testRoundStateSerializesLockFields();
    testRoundStateDeserializeRoundTrip();
    testRoundStateDefaultLockFields();
    return 0;
}
