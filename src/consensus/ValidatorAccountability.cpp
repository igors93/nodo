#include "consensus/ValidatorAccountability.hpp"

#include <sstream>
#include <utility>

namespace nodo::consensus {

namespace {

bool isSafeScalar(const std::string& value, std::size_t maxSize = 512) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        if (character == '\n' ||
            character == '\r' ||
            character == '\t' ||
            character == '{' ||
            character == '}') {
            return false;
        }
    }

    return true;
}

} // namespace

std::string validatorAccountabilityStatusToString(
    ValidatorAccountabilityStatus status
) {
    switch (status) {
        case ValidatorAccountabilityStatus::CLEAN:
            return "CLEAN";
        case ValidatorAccountabilityStatus::UNDER_REVIEW:
            return "UNDER_REVIEW";
        case ValidatorAccountabilityStatus::SLASHABLE:
            return "SLASHABLE";
        default:
            return "UNDER_REVIEW";
    }
}

ValidatorAccountabilityReport::ValidatorAccountabilityReport()
    : m_validatorAddress(),
      m_status(ValidatorAccountabilityStatus::CLEAN),
      m_evidenceCount(0),
      m_slashableEvidenceCount(0),
      m_reason("No evidence recorded.") {}

ValidatorAccountabilityReport::ValidatorAccountabilityReport(
    std::string validatorAddress,
    ValidatorAccountabilityStatus status,
    std::size_t evidenceCount,
    std::size_t slashableEvidenceCount,
    std::string reason
) : m_validatorAddress(std::move(validatorAddress)),
    m_status(status),
    m_evidenceCount(evidenceCount),
    m_slashableEvidenceCount(slashableEvidenceCount),
    m_reason(std::move(reason)) {}

const std::string& ValidatorAccountabilityReport::validatorAddress() const {
    return m_validatorAddress;
}

ValidatorAccountabilityStatus ValidatorAccountabilityReport::status() const {
    return m_status;
}

std::size_t ValidatorAccountabilityReport::evidenceCount() const {
    return m_evidenceCount;
}

std::size_t ValidatorAccountabilityReport::slashableEvidenceCount() const {
    return m_slashableEvidenceCount;
}

const std::string& ValidatorAccountabilityReport::reason() const {
    return m_reason;
}

bool ValidatorAccountabilityReport::isValid() const {
    return isSafeScalar(m_validatorAddress) &&
           !m_reason.empty() &&
           m_slashableEvidenceCount <= m_evidenceCount;
}

std::string ValidatorAccountabilityReport::serialize() const {
    std::ostringstream output;
    output << "ValidatorAccountabilityReport{"
           << "validatorAddress=" << m_validatorAddress
           << ";status=" << validatorAccountabilityStatusToString(m_status)
           << ";evidenceCount=" << m_evidenceCount
           << ";slashableEvidenceCount=" << m_slashableEvidenceCount
           << ";reason=" << m_reason
           << "}";
    return output.str();
}

ValidatorAccountabilityTracker::ValidatorAccountabilityTracker()
    : m_evidenceByValidator() {}

void ValidatorAccountabilityTracker::submitEvidence(
    const SlashingEvidenceRecord& evidence
) {
    if (!evidence.isValid()) {
        return;
    }

    auto& entries = m_evidenceByValidator[evidence.validatorAddress()];

    for (const auto& existing : entries) {
        if (existing.evidenceId() == evidence.evidenceId()) {
            return;
        }
    }

    entries.push_back(evidence);
}

ValidatorAccountabilityReport ValidatorAccountabilityTracker::reportForValidator(
    const std::string& validatorAddress
) const {
    const auto found = m_evidenceByValidator.find(validatorAddress);

    if (found == m_evidenceByValidator.end() || found->second.empty()) {
        return ValidatorAccountabilityReport(
            validatorAddress,
            ValidatorAccountabilityStatus::CLEAN,
            0,
            0,
            "No slashing evidence recorded for validator."
        );
    }

    std::size_t slashableCount = 0;
    for (const auto& evidence : found->second) {
        if (evidence.severity() == SlashingEvidenceSeverity::SLASHABLE) {
            ++slashableCount;
        }
    }

    const ValidatorAccountabilityStatus status =
        slashableCount > 0
            ? ValidatorAccountabilityStatus::SLASHABLE
            : ValidatorAccountabilityStatus::UNDER_REVIEW;

    const std::string reason =
        slashableCount > 0
            ? "Validator has slashable evidence."
            : "Validator has non-slashable evidence under review.";

    return ValidatorAccountabilityReport(
        validatorAddress,
        status,
        found->second.size(),
        slashableCount,
        reason
    );
}

std::vector<ValidatorAccountabilityReport> ValidatorAccountabilityTracker::allReports() const {
    std::vector<ValidatorAccountabilityReport> reports;
    reports.reserve(m_evidenceByValidator.size());

    for (const auto& entry : m_evidenceByValidator) {
        reports.push_back(reportForValidator(entry.first));
    }

    return reports;
}

std::size_t ValidatorAccountabilityTracker::evidenceCountForValidator(
    const std::string& validatorAddress
) const {
    const auto found = m_evidenceByValidator.find(validatorAddress);
    return found == m_evidenceByValidator.end() ? 0 : found->second.size();
}

std::size_t ValidatorAccountabilityTracker::slashableEvidenceCountForValidator(
    const std::string& validatorAddress
) const {
    return reportForValidator(validatorAddress).slashableEvidenceCount();
}

bool ValidatorAccountabilityTracker::validatorIsSlashable(
    const std::string& validatorAddress
) const {
    return reportForValidator(validatorAddress).status() ==
           ValidatorAccountabilityStatus::SLASHABLE;
}

std::string ValidatorAccountabilityTracker::serialize() const {
    std::ostringstream output;
    output << "ValidatorAccountabilityTracker{validatorCount="
           << m_evidenceByValidator.size()
           << ";reports=[";

    bool first = true;
    for (const auto& entry : m_evidenceByValidator) {
        if (!first) {
            output << ",";
        }

        output << reportForValidator(entry.first).serialize();
        first = false;
    }

    output << "]}";
    return output.str();
}

} // namespace nodo::consensus
