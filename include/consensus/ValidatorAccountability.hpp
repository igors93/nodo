#ifndef NODO_CONSENSUS_VALIDATOR_ACCOUNTABILITY_HPP
#define NODO_CONSENSUS_VALIDATOR_ACCOUNTABILITY_HPP

#include "consensus/SlashingEvidence.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace nodo::consensus {

enum class ValidatorAccountabilityStatus {
    CLEAN,
    UNDER_REVIEW,
    SLASHABLE
};

std::string validatorAccountabilityStatusToString(
    ValidatorAccountabilityStatus status
);

class ValidatorAccountabilityReport {
public:
    ValidatorAccountabilityReport();

    ValidatorAccountabilityReport(
        std::string validatorAddress,
        ValidatorAccountabilityStatus status,
        std::size_t evidenceCount,
        std::size_t slashableEvidenceCount,
        std::string reason
    );

    const std::string& validatorAddress() const;
    ValidatorAccountabilityStatus status() const;
    std::size_t evidenceCount() const;
    std::size_t slashableEvidenceCount() const;
    const std::string& reason() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    ValidatorAccountabilityStatus m_status;
    std::size_t m_evidenceCount;
    std::size_t m_slashableEvidenceCount;
    std::string m_reason;
};

class ValidatorAccountabilityTracker {
public:
    ValidatorAccountabilityTracker();

    void submitEvidence(const SlashingEvidenceRecord& evidence);

    ValidatorAccountabilityReport reportForValidator(
        const std::string& validatorAddress
    ) const;

    std::vector<ValidatorAccountabilityReport> allReports() const;

    std::size_t evidenceCountForValidator(
        const std::string& validatorAddress
    ) const;

    std::size_t slashableEvidenceCountForValidator(
        const std::string& validatorAddress
    ) const;

    bool validatorIsSlashable(
        const std::string& validatorAddress
    ) const;

    std::string serialize() const;

private:
    std::map<std::string, std::vector<SlashingEvidenceRecord>> m_evidenceByValidator;
};

} // namespace nodo::consensus

#endif
