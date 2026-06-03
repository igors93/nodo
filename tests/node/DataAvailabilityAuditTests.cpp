#include "node/DataAvailabilityAuditValidator.hpp"
#include "node/DataAvailabilityEvidence.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::node::DataAvailabilityAuditStatus;
using nodo::node::DataAvailabilityAuditValidator;
using nodo::node::DataAvailabilityChallenge;
using nodo::node::DataAvailabilityFailureEvidence;
using nodo::node::DataAvailabilityResponse;

DataAvailabilityChallenge validChallenge() {
    return DataAvailabilityChallenge(
        "challenge-001",
        100,
        "block-hash-abc",
        "artifact-digest-xyz",
        "challenger-node-01",
        1900000001
    );
}

// Test 24: Availability response with matching digest passes.
void testResponseMatchingDigestPasses() {
    const DataAvailabilityResponse response(
        "response-001",
        "challenge-001",
        "server-node-01",
        "artifact-digest-xyz",  // matches challenge
        1900000002
    );
    const auto result = DataAvailabilityAuditValidator::validateResponse(
        validChallenge(), response
    );
    assert(result.isPassed());
}

// Test 24: Availability response with wrong digest fails.
void testResponseWrongDigestFails() {
    const DataAvailabilityResponse response(
        "response-001",
        "challenge-001",
        "server-node-01",
        "wrong-artifact-digest",  // mismatch
        1900000002
    );
    const auto result = DataAvailabilityAuditValidator::validateResponse(
        validChallenge(), response
    );
    assert(!result.isPassed());
    assert(result.status() == DataAvailabilityAuditStatus::RESPONSE_DIGEST_MISMATCH);
    assert(result.reason().find("artifactDigest") != std::string::npos);
}

// Test 25: Availability response with wrong challenge ID fails.
void testResponseWrongChallengeIdFails() {
    const DataAvailabilityResponse response(
        "response-001",
        "challenge-WRONG",  // wrong challenge id
        "server-node-01",
        "artifact-digest-xyz",
        1900000002
    );
    const auto result = DataAvailabilityAuditValidator::validateResponse(
        validChallenge(), response
    );
    assert(!result.isPassed());
    assert(result.status() == DataAvailabilityAuditStatus::RESPONSE_CHALLENGE_MISMATCH);
}

// Test 26: Missing challenge fails failure-evidence validation.
void testFailureEvidenceMissingChallengeId() {
    const DataAvailabilityChallenge invalidChallenge;  // default-constructed, invalid
    const DataAvailabilityFailureEvidence evidence(
        "evidence-001",
        "challenge-001",
        100,
        "failed-node",
        "did not respond",
        1900000003
    );
    const auto result = DataAvailabilityAuditValidator::validateFailureEvidence(
        invalidChallenge, evidence
    );
    assert(!result.isPassed());
    assert(result.status() == DataAvailabilityAuditStatus::FAILURE_EVIDENCE_MISSING_CHALLENGE);
}

// Test 26: Failure evidence with mismatched challenge ID fails.
void testFailureEvidenceMismatchedChallengeId() {
    const DataAvailabilityFailureEvidence evidence(
        "evidence-001",
        "challenge-WRONG",  // wrong challenge id
        100,
        "failed-node",
        "did not respond",
        1900000003
    );
    const auto result = DataAvailabilityAuditValidator::validateFailureEvidence(
        validChallenge(), evidence
    );
    assert(!result.isPassed());
}

// Failure evidence referencing correct challenge passes.
void testFailureEvidenceValidPasses() {
    const DataAvailabilityFailureEvidence evidence(
        "evidence-001",
        "challenge-001",  // matches challenge
        100,
        "failed-node",
        "did not respond in time",
        1900000003
    );
    const auto result = DataAvailabilityAuditValidator::validateFailureEvidence(
        validChallenge(), evidence
    );
    assert(result.isPassed());
}

// validateArtifactDigest: empty expected digest always passes.
void testEmptyExpectedDigestPasses() {
    const nodo::node::FinalizedBlockArtifact artifact;
    const auto result = DataAvailabilityAuditValidator::validateArtifactDigest(
        artifact, ""
    );
    assert(result.isPassed());
}

// Status to string covers all cases.
void testStatusToString() {
    using nodo::node::dataAvailabilityAuditStatusToString;
    assert(dataAvailabilityAuditStatusToString(
               DataAvailabilityAuditStatus::PASSED) == "PASSED");
    assert(dataAvailabilityAuditStatusToString(
               DataAvailabilityAuditStatus::RESPONSE_DIGEST_MISMATCH) ==
           "RESPONSE_DIGEST_MISMATCH");
    assert(dataAvailabilityAuditStatusToString(
               DataAvailabilityAuditStatus::ARTIFACT_DIGEST_MISMATCH) ==
           "ARTIFACT_DIGEST_MISMATCH");
}

} // namespace

int main() {
    testResponseMatchingDigestPasses();
    testResponseWrongDigestFails();
    testResponseWrongChallengeIdFails();
    testFailureEvidenceMissingChallengeId();
    testFailureEvidenceMismatchedChallengeId();
    testFailureEvidenceValidPasses();
    testEmptyExpectedDigestPasses();
    testStatusToString();
    return 0;
}
