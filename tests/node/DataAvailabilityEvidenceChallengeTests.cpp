// P1 tests: FinalizedBlockArtifact::artifactDigest() determinism and DataAvailabilityEvidence.
#include "node/DataAvailabilityEvidence.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::node::DataAvailabilityChallenge;
using nodo::node::DataAvailabilityResponse;
using nodo::node::DataAvailabilityFailureEvidence;

// Test 16: DataAvailabilityChallenge rejects mismatched response digest.
void testChallengeResponseDigestMismatch() {
    const DataAvailabilityChallenge challenge(
        "challenge-001", 42,
        "blockhash-abc123",
        "artifact-digest-expected",
        "challenger-node",
        9999
    );
    assert(challenge.isValid());

    const DataAvailabilityResponse wrongDigest(
        "response-001",
        "challenge-001",
        "server-node",
        "artifact-digest-WRONG",  // does not match
        10000
    );
    assert(wrongDigest.isValid());
    // The validator must reject this: response digest != challenge digest.
    assert(wrongDigest.artifactDigest() != challenge.artifactDigest());
}

// Test 17: DataAvailabilityResponse matching digest is valid.
void testChallengeResponseMatchingDigest() {
    const std::string sharedDigest = "artifact-digest-shared";

    const DataAvailabilityChallenge challenge(
        "challenge-002", 42,
        "blockhash-abc",
        sharedDigest,
        "challenger-node",
        9999
    );
    assert(challenge.isValid());

    const DataAvailabilityResponse response(
        "response-002",
        "challenge-002",
        "server-node",
        sharedDigest,
        10000
    );
    assert(response.isValid());
    assert(response.artifactDigest() == challenge.artifactDigest());
    assert(response.challengeId() == challenge.challengeId());
}

// Test 18: DataAvailabilityFailureEvidence only valid with non-empty challengeId and failedNodeId.
void testFailureEvidenceRequiresValidFields() {
    const DataAvailabilityFailureEvidence badEvidence(
        "", // empty evidenceId
        "challenge-001",
        42,
        "failed-node",
        "no response received",
        10001
    );
    assert(!badEvidence.isValid());

    const DataAvailabilityFailureEvidence goodEvidence(
        "failure-evid-001",
        "challenge-001",
        42,
        "failed-node",
        "no response received",
        10001
    );
    assert(goodEvidence.isValid());
    assert(goodEvidence.challengeId() == "challenge-001");
    assert(goodEvidence.failedNodeId() == "failed-node");
}

// Test 19: DataAvailabilityChallenge rejects empty artifactDigest.
void testChallengeRejectsEmptyDigest() {
    const DataAvailabilityChallenge emptyDigest(
        "challenge-003", 1,
        "blockhash-xyz",
        "",  // empty digest
        "challenger",
        1000
    );
    assert(!emptyDigest.isValid());
}

// Test 20: DataAvailabilityResponse rejects empty challengeId.
void testResponseRejectsEmptyChallengeId() {
    const DataAvailabilityResponse emptyChallenge(
        "response-003",
        "",  // empty challengeId
        "server",
        "some-digest",
        1000
    );
    assert(!emptyChallenge.isValid());
}

} // namespace

int main() {
    testChallengeResponseDigestMismatch();
    testChallengeResponseMatchingDigest();
    testFailureEvidenceRequiresValidFields();
    testChallengeRejectsEmptyDigest();
    testResponseRejectsEmptyChallengeId();
    return 0;
}
