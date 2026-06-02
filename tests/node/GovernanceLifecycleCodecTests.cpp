#include "node/GovernanceLifecycleCodec.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/GovernanceLifecycleVerifier.hpp"

#include <cassert>
#include <exception>
#include <string>

namespace {

using nodo::economics::GovernanceLifecycleVerifier;
using nodo::node::GovernanceLifecycleCodec;
using nodo::tests::fixtures::validLifecycle;

void removeLine(std::string& encoded, const std::string& field) {
    const std::size_t pos = encoded.find(field + "=");
    assert(pos != std::string::npos);
    const std::size_t lineEnd = encoded.find('\n', pos);
    encoded.erase(pos, lineEnd - pos + 1);
}

void testRoundTripValidLifecycle() {
    const auto lifecycle = validLifecycle();
    const std::string encoded = GovernanceLifecycleCodec::encode(lifecycle);
    const auto decoded = GovernanceLifecycleCodec::decode(encoded);
    assert(decoded.isValid());
    assert(decoded.lifecycleId() == lifecycle.lifecycleId());
    assert(GovernanceLifecycleVerifier::verify(decoded).verified());
}

void testMissingVoteFieldRejected() {
    std::string encoded = GovernanceLifecycleCodec::encode(validLifecycle());
    removeLine(encoded, "vote.0.voteProof");

    bool threw = false;
    try {
        (void)GovernanceLifecycleCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testMissingProposalEnvelopeRejected() {
    std::string encoded = GovernanceLifecycleCodec::encode(validLifecycle());
    removeLine(encoded, "proposalEnvelope.governanceProposalId");

    bool threw = false;
    try {
        (void)GovernanceLifecycleCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testMissingVotingPolicyRejected() {
    std::string encoded = GovernanceLifecycleCodec::encode(validLifecycle());
    removeLine(encoded, "votingPolicy.minimumVotingPowerRawUnits");

    bool threw = false;
    try {
        (void)GovernanceLifecycleCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testMissingTallyFieldRejected() {
    std::string encoded = GovernanceLifecycleCodec::encode(validLifecycle());
    removeLine(encoded, "tally.tallyProof");

    bool threw = false;
    try {
        (void)GovernanceLifecycleCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testMissingDecisionFieldRejected() {
    std::string encoded = GovernanceLifecycleCodec::encode(validLifecycle());
    removeLine(encoded, "decision.decisionProof");

    bool threw = false;
    try {
        (void)GovernanceLifecycleCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testUnexpectedFieldRejected() {
    const std::string encoded =
        GovernanceLifecycleCodec::encode(validLifecycle()) +
        "unexpected=field\n";
    bool threw = false;
    try {
        (void)GovernanceLifecycleCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testVStyleSchemaRejected() {
    std::string encoded = GovernanceLifecycleCodec::encode(validLifecycle());
    const std::size_t firstNewline = encoded.find('\n');
    encoded.replace(0, firstNewline, "NODO_GOVERNANCE_LIFECYCLE_V1");

    bool threw = false;
    try {
        (void)GovernanceLifecycleCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testTamperedPersistedLifecycleRejectedByVerifier() {
    std::string encoded = GovernanceLifecycleCodec::encode(validLifecycle());
    const std::string from = "tally.yesVotingPowerRawUnits=60\n";
    const std::string to = "tally.yesVotingPowerRawUnits=61\n";
    const std::size_t pos = encoded.find(from);
    assert(pos != std::string::npos);
    encoded.replace(pos, from.size(), to);

    bool threw = false;
    try {
        (void)GovernanceLifecycleCodec::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

} // namespace

int main() {
    testRoundTripValidLifecycle();
    testMissingVoteFieldRejected();
    testMissingProposalEnvelopeRejected();
    testMissingVotingPolicyRejected();
    testMissingTallyFieldRejected();
    testMissingDecisionFieldRejected();
    testUnexpectedFieldRejected();
    testVStyleSchemaRejected();
    testTamperedPersistedLifecycleRejectedByVerifier();
    return 0;
}
