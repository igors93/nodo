#ifndef NODO_NODE_SLASHING_EVIDENCE_HPP
#define NODO_NODE_SLASHING_EVIDENCE_HPP

#include "node/LockedStakePosition.hpp"
#include "node/ProtectionRewards.hpp"
#include "node/ValidatorNetworkPolicy.hpp"
#include "node/ValidatorRiskAssessment.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class SlashingEvidenceRecord {
public:
    SlashingEvidenceRecord();

    SlashingEvidenceRecord(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::string evidenceType,
        std::uint16_t severityScore,
        bool slashable,
        std::string recommendedAction,
        std::string reason,
        std::string sourceSecurityDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    const std::string& evidenceType() const;
    std::uint16_t severityScore() const;
    bool slashable() const;
    const std::string& recommendedAction() const;
    const std::string& reason() const;
    const std::string& sourceSecurityDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::string m_evidenceType;
    std::uint16_t m_severityScore;
    bool m_slashable;
    std::string m_recommendedAction;
    std::string m_reason;
    std::string m_sourceSecurityDigest;
};

class SlashingPreparationRecord {
public:
    SlashingPreparationRecord();

    SlashingPreparationRecord(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::uint64_t evidenceCount,
        std::uint64_t slashableEvidenceCount,
        std::uint16_t maxSeverityScore,
        utils::Amount preparedPenaltyAmount,
        std::string enforcementAction,
        std::string reason,
        std::string sourceEvidenceDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    std::uint64_t evidenceCount() const;
    std::uint64_t slashableEvidenceCount() const;
    std::uint16_t maxSeverityScore() const;
    utils::Amount preparedPenaltyAmount() const;
    const std::string& enforcementAction() const;
    const std::string& reason() const;
    const std::string& sourceEvidenceDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::uint64_t m_evidenceCount;
    std::uint64_t m_slashableEvidenceCount;
    std::uint16_t m_maxSeverityScore;
    utils::Amount m_preparedPenaltyAmount;
    std::string m_enforcementAction;
    std::string m_reason;
    std::string m_sourceEvidenceDigest;
};

class SlashingEvidenceSummary {
public:
    SlashingEvidenceSummary();

    SlashingEvidenceSummary(
        std::string status,
        std::uint64_t blockHeight,
        std::uint64_t evidenceCount,
        std::uint64_t slashableEvidenceCount,
        std::uint16_t maxSeverityScore,
        utils::Amount preparedPenaltyTotal,
        std::string reason,
        std::string sourcePreparationDigest
    );

    static SlashingEvidenceSummary notEvaluated();

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    std::uint64_t evidenceCount() const;
    std::uint64_t slashableEvidenceCount() const;
    std::uint16_t maxSeverityScore() const;
    utils::Amount preparedPenaltyTotal() const;
    const std::string& reason() const;
    const std::string& sourcePreparationDigest() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    std::uint64_t m_evidenceCount;
    std::uint64_t m_slashableEvidenceCount;
    std::uint16_t m_maxSeverityScore;
    utils::Amount m_preparedPenaltyTotal;
    std::string m_reason;
    std::string m_sourcePreparationDigest;
};

class SlashingEvidence {
public:
    static constexpr const char* RISK_CONTAINMENT_EVIDENCE_REASON =
        "RISK_CONTAINMENT_EVIDENCE";

    static constexpr const char* SLASHING_PREPARATION_REASON =
        "SLASHING_PREPARATION_REVIEW";

    static constexpr const char* SLASHING_SUMMARY_REASON =
        "SLASHING_EVIDENCE_SUMMARY";

    static constexpr const char* NOT_EVALUATED_REASON =
        "SLASHING_EVIDENCE_NOT_EVALUATED";

    static std::vector<SlashingEvidenceRecord> buildEvidenceRecords(
        const std::vector<ValidatorRiskAssessment>& riskAssessments,
        const std::vector<ValidatorNetworkPolicy>& networkPolicies,
        const std::vector<ProtectionWorkRecord>& protectionWorkRecords
    );

    static std::vector<SlashingPreparationRecord> buildPreparationRecords(
        const std::vector<SlashingEvidenceRecord>& evidenceRecords,
        const std::vector<LockedStakePosition>& lockedStakePositions
    );

    static SlashingEvidenceSummary buildSummary(
        std::uint64_t blockHeight,
        const std::vector<SlashingEvidenceRecord>& evidenceRecords,
        const std::vector<SlashingPreparationRecord>& preparationRecords
    );

    static bool sameEvidenceRecords(
        const std::vector<SlashingEvidenceRecord>& left,
        const std::vector<SlashingEvidenceRecord>& right
    );

    static bool samePreparationRecords(
        const std::vector<SlashingPreparationRecord>& left,
        const std::vector<SlashingPreparationRecord>& right
    );

    static bool sameSummary(
        const SlashingEvidenceSummary& left,
        const SlashingEvidenceSummary& right
    );
};

} // namespace nodo::node

#endif
