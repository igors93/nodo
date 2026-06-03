#ifndef NODO_NODE_PROTECTION_REWARDS_HPP
#define NODO_NODE_PROTECTION_REWARDS_HPP

#include "node/ProtectionTreasury.hpp"
#include "node/RewardCategory.hpp"
#include "node/SecurityScore.hpp"
#include "node/ValidatorNetworkPolicy.hpp"
#include "node/ValidatorRiskAssessment.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

constexpr std::uint16_t NODO_PROTECTION_WORK_SCORE_MIN = 1;
constexpr std::uint16_t NODO_PROTECTION_WORK_SCORE_MAX = 1000;

class ProtectionWorkRecord {
public:
    ProtectionWorkRecord();

    ProtectionWorkRecord(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::uint16_t uptimeScore,
        std::uint16_t correctVoteScore,
        std::uint16_t attackDetectionScore,
        std::uint16_t auditContributionScore,
        std::uint16_t securityScore,
        std::uint16_t riskPenaltyScore,
        std::uint16_t totalWorkScore,
        std::string reason,
        std::string sourceSecurityDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    std::uint16_t uptimeScore() const;
    std::uint16_t correctVoteScore() const;
    std::uint16_t attackDetectionScore() const;
    std::uint16_t auditContributionScore() const;
    std::uint16_t securityScore() const;
    std::uint16_t riskPenaltyScore() const;
    std::uint16_t totalWorkScore() const;
    const std::string& reason() const;
    const std::string& sourceSecurityDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::uint16_t m_uptimeScore;
    std::uint16_t m_correctVoteScore;
    std::uint16_t m_attackDetectionScore;
    std::uint16_t m_auditContributionScore;
    std::uint16_t m_securityScore;
    std::uint16_t m_riskPenaltyScore;
    std::uint16_t m_totalWorkScore;
    std::string m_reason;
    std::string m_sourceSecurityDigest;
};

class ProtectionRewardSettlement {
public:
    ProtectionRewardSettlement();

    ProtectionRewardSettlement(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        utils::Amount plannedReward,
        utils::Amount earnedReward,
        utils::Amount deferredReward,
        std::uint16_t workScore,
        std::uint16_t securityScore,
        std::string reason,
        std::string sourceGrantDigest,
        std::string sourceWorkDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    utils::Amount plannedReward() const;
    utils::Amount earnedReward() const;
    utils::Amount deferredReward() const;
    std::uint16_t workScore() const;
    std::uint16_t securityScore() const;
    const std::string& reason() const;
    const std::string& sourceGrantDigest() const;
    const std::string& sourceWorkDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    utils::Amount m_plannedReward;
    utils::Amount m_earnedReward;
    utils::Amount m_deferredReward;
    std::uint16_t m_workScore;
    std::uint16_t m_securityScore;
    std::string m_reason;
    std::string m_sourceGrantDigest;
    std::string m_sourceWorkDigest;
};

class ProtectionRewardSummary {
public:
    ProtectionRewardSummary();

    ProtectionRewardSummary(
        std::string status,
        std::uint64_t blockHeight,
        utils::Amount plannedTotal,
        utils::Amount earnedTotal,
        utils::Amount deferredTotal,
        std::uint64_t beneficiaryCount,
        std::string reason,
        std::string sourceBudgetDigest
    );

    static ProtectionRewardSummary notEvaluated();

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    utils::Amount plannedTotal() const;
    utils::Amount earnedTotal() const;
    utils::Amount deferredTotal() const;
    std::uint64_t beneficiaryCount() const;
    const std::string& reason() const;
    const std::string& sourceBudgetDigest() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    utils::Amount m_plannedTotal;
    utils::Amount m_earnedTotal;
    utils::Amount m_deferredTotal;
    std::uint64_t m_beneficiaryCount;
    std::string m_reason;
    std::string m_sourceBudgetDigest;
};

/*
 * RewardEvidenceAuditResult reports whether protection reward settlements
 * satisfy evidence requirements. Chain audit calls this to prevent
 * evidence-free reward issuance.
 */
class RewardEvidenceAuditResult {
public:
    RewardEvidenceAuditResult();
    static RewardEvidenceAuditResult passed();
    static RewardEvidenceAuditResult failed(std::string reason);

    bool isPassed() const;
    const std::string& reason() const;

private:
    bool m_passed;
    std::string m_reason;
};

class ProtectionRewards {
public:
    static constexpr const char* PROTECTION_WORK_REASON =
        "PROTECTION_WORK_SCORE";

    static constexpr const char* PROTECTION_SETTLEMENT_REASON =
        "PROTECTION_REWARD_SETTLEMENT";

    static constexpr const char* PROTECTION_SUMMARY_REASON =
        "REAL_PROTECTION_REWARD_SUMMARY";

    static constexpr const char* NOT_EVALUATED_REASON =
        "PROTECTION_REWARDS_NOT_EVALUATED";

    static ProtectionWorkRecord buildWorkRecord(
        const ProtectionRewardGrant& grant,
        const std::vector<SecurityScoreRecord>& securityScoreRecords,
        const std::vector<ValidatorRiskAssessment>& riskAssessments,
        const std::vector<ValidatorNetworkPolicy>& networkPolicies
    );

    static std::vector<ProtectionWorkRecord> buildWorkRecords(
        const std::vector<ProtectionRewardGrant>& grants,
        const std::vector<SecurityScoreRecord>& securityScoreRecords,
        const std::vector<ValidatorRiskAssessment>& riskAssessments,
        const std::vector<ValidatorNetworkPolicy>& networkPolicies
    );

    static ProtectionRewardSettlement buildSettlement(
        const ProtectionRewardGrant& grant,
        const ProtectionWorkRecord& workRecord
    );

    static std::vector<ProtectionRewardSettlement> buildSettlements(
        const std::vector<ProtectionRewardGrant>& grants,
        const std::vector<ProtectionWorkRecord>& workRecords
    );

    static ProtectionRewardSummary buildSummary(
        const ProtectionRewardBudget& budget,
        const std::vector<ProtectionRewardSettlement>& settlements
    );

    static bool sameWorkRecords(
        const std::vector<ProtectionWorkRecord>& left,
        const std::vector<ProtectionWorkRecord>& right
    );

    static bool sameSettlements(
        const std::vector<ProtectionRewardSettlement>& left,
        const std::vector<ProtectionRewardSettlement>& right
    );

    static bool sameSummary(
        const ProtectionRewardSummary& left,
        const ProtectionRewardSummary& right
    );

    // Derive the reward category from a settlement record.
    // PROTECTION when earned > 0 and no deferred portion.
    // DEFERRED_PROTECTION when deferred > 0.
    // REJECTED when earned == 0 and deferred == 0 but planned > 0.
    static RewardCategory categoryForSettlement(
        const ProtectionRewardSettlement& settlement
    );

    // Validate that all protection reward settlements in a block carry
    // verifiable evidence (sourceWorkDigest). Returns failed if any
    // settlement lacks evidence.
    static RewardEvidenceAuditResult auditSettlementEvidence(
        const std::vector<ProtectionRewardSettlement>& settlements
    );
};

} // namespace nodo::node

#endif
