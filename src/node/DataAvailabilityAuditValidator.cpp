#include "node/DataAvailabilityAuditValidator.hpp"

#include <utility>

namespace nodo::node {

std::string dataAvailabilityAuditStatusToString(DataAvailabilityAuditStatus status) {
    switch (status) {
        case DataAvailabilityAuditStatus::PASSED:
            return "PASSED";
        case DataAvailabilityAuditStatus::RESPONSE_DIGEST_MISMATCH:
            return "RESPONSE_DIGEST_MISMATCH";
        case DataAvailabilityAuditStatus::CHALLENGE_MISSING:
            return "CHALLENGE_MISSING";
        case DataAvailabilityAuditStatus::RESPONSE_CHALLENGE_MISMATCH:
            return "RESPONSE_CHALLENGE_MISMATCH";
        case DataAvailabilityAuditStatus::FAILURE_EVIDENCE_MISSING_CHALLENGE:
            return "FAILURE_EVIDENCE_MISSING_CHALLENGE";
        case DataAvailabilityAuditStatus::ARTIFACT_DIGEST_MISMATCH:
            return "ARTIFACT_DIGEST_MISMATCH";
        default:
            return "UNKNOWN";
    }
}

DataAvailabilityAuditResult::DataAvailabilityAuditResult()
    : m_passed(false),
      m_status(DataAvailabilityAuditStatus::CHALLENGE_MISSING),
      m_reason("Uninitialized.") {}

DataAvailabilityAuditResult DataAvailabilityAuditResult::passed() {
    DataAvailabilityAuditResult r;
    r.m_passed = true;
    r.m_status = DataAvailabilityAuditStatus::PASSED;
    r.m_reason = "";
    return r;
}

DataAvailabilityAuditResult DataAvailabilityAuditResult::failed(
    DataAvailabilityAuditStatus status,
    std::string reason
) {
    DataAvailabilityAuditResult r;
    r.m_passed = false;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool DataAvailabilityAuditResult::isPassed() const { return m_passed; }
DataAvailabilityAuditStatus DataAvailabilityAuditResult::status() const { return m_status; }
const std::string& DataAvailabilityAuditResult::reason() const { return m_reason; }

DataAvailabilityAuditResult DataAvailabilityAuditValidator::validateResponse(
    const DataAvailabilityChallenge& challenge,
    const DataAvailabilityResponse& response
) {
    if (!challenge.isValid()) {
        return DataAvailabilityAuditResult::failed(
            DataAvailabilityAuditStatus::CHALLENGE_MISSING,
            "DataAvailabilityAuditValidator: challenge is invalid: " +
            challenge.rejectionReason()
        );
    }

    if (!response.isValid()) {
        return DataAvailabilityAuditResult::failed(
            DataAvailabilityAuditStatus::RESPONSE_CHALLENGE_MISMATCH,
            "DataAvailabilityAuditValidator: response is invalid: " +
            response.rejectionReason()
        );
    }

    if (response.challengeId() != challenge.challengeId()) {
        return DataAvailabilityAuditResult::failed(
            DataAvailabilityAuditStatus::RESPONSE_CHALLENGE_MISMATCH,
            "DataAvailabilityAuditValidator: response challengeId '" +
            response.challengeId() +
            "' does not match challenge challengeId '" +
            challenge.challengeId() + "'"
        );
    }

    if (response.artifactDigest() != challenge.artifactDigest()) {
        return DataAvailabilityAuditResult::failed(
            DataAvailabilityAuditStatus::RESPONSE_DIGEST_MISMATCH,
            "DataAvailabilityAuditValidator: response artifactDigest '" +
            response.artifactDigest() +
            "' does not match challenge artifactDigest '" +
            challenge.artifactDigest() + "'"
        );
    }

    return DataAvailabilityAuditResult::passed();
}

DataAvailabilityAuditResult DataAvailabilityAuditValidator::validateFailureEvidence(
    const DataAvailabilityChallenge& challenge,
    const DataAvailabilityFailureEvidence& evidence
) {
    if (!challenge.isValid()) {
        return DataAvailabilityAuditResult::failed(
            DataAvailabilityAuditStatus::FAILURE_EVIDENCE_MISSING_CHALLENGE,
            "DataAvailabilityAuditValidator: failure evidence requires a valid "
            "challenge; challenge is invalid: " + challenge.rejectionReason()
        );
    }

    if (!evidence.isValid()) {
        return DataAvailabilityAuditResult::failed(
            DataAvailabilityAuditStatus::FAILURE_EVIDENCE_MISSING_CHALLENGE,
            "DataAvailabilityAuditValidator: failure evidence is invalid: " +
            evidence.rejectionReason()
        );
    }

    if (evidence.challengeId() != challenge.challengeId()) {
        return DataAvailabilityAuditResult::failed(
            DataAvailabilityAuditStatus::FAILURE_EVIDENCE_MISSING_CHALLENGE,
            "DataAvailabilityAuditValidator: failure evidence challengeId '" +
            evidence.challengeId() +
            "' does not match challenge challengeId '" +
            challenge.challengeId() + "'"
        );
    }

    return DataAvailabilityAuditResult::passed();
}

DataAvailabilityAuditResult DataAvailabilityAuditValidator::validateArtifactDigest(
    const FinalizedBlockArtifact& artifact,
    const std::string& expectedDigest
) {
    if (expectedDigest.empty()) {
        return DataAvailabilityAuditResult::passed();
    }

    if (!artifact.isValid()) {
        return DataAvailabilityAuditResult::failed(
            DataAvailabilityAuditStatus::ARTIFACT_DIGEST_MISMATCH,
            "DataAvailabilityAuditValidator: artifact is invalid and digest "
            "cannot be verified"
        );
    }

    const std::string computed = artifact.artifactDigest();
    if (computed != expectedDigest) {
        return DataAvailabilityAuditResult::failed(
            DataAvailabilityAuditStatus::ARTIFACT_DIGEST_MISMATCH,
            "DataAvailabilityAuditValidator: recomputed artifact digest '" +
            computed + "' does not match expected digest '" +
            expectedDigest + "'"
        );
    }

    return DataAvailabilityAuditResult::passed();
}

} // namespace nodo::node
