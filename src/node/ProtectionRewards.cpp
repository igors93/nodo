#include "node/ProtectionRewards.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::uint16_t clampScore(
    std::int64_t value
) {
    if (value < NODO_PROTECTION_WORK_SCORE_MIN) {
        return NODO_PROTECTION_WORK_SCORE_MIN;
    }

    if (value > NODO_PROTECTION_WORK_SCORE_MAX) {
        return NODO_PROTECTION_WORK_SCORE_MAX;
    }

    return static_cast<std::uint16_t>(value);
}

const SecurityScoreRecord* findSecurityScore(
    const std::vector<SecurityScoreRecord>& records,
    const std::string& validatorAddress
) {
    for (const SecurityScoreRecord& record : records) {
        if (record.validatorAddress() == validatorAddress) {
            return &record;
        }
    }

    return nullptr;
}

const ValidatorRiskAssessment* findRiskAssessment(
    const std::vector<ValidatorRiskAssessment>& assessments,
    const std::string& validatorAddress
) {
    for (const ValidatorRiskAssessment& assessment : assessments) {
        if (assessment.validatorAddress() == validatorAddress) {
            return &assessment;
        }
    }

    return nullptr;
}

const ValidatorNetworkPolicy* findNetworkPolicy(
    const std::vector<ValidatorNetworkPolicy>& policies,
    const std::string& validatorAddress
) {
    for (const ValidatorNetworkPolicy& policy : policies) {
        if (policy.validatorAddress() == validatorAddress) {
            return &policy;
        }
    }

    return nullptr;
}

std::uint16_t riskPenaltyForLevel(
    const std::string& riskLevel
) {
    if (riskLevel == "HIGH") {
        return 300;
    }

    if (riskLevel == "ELEVATED") {
        return 160;
    }

    if (riskLevel == "MODERATE") {
        return 60;
    }

    if (riskLevel == "LOW") {
        return 0;
    }

    return 80;
}

std::uint16_t networkPenaltyForPolicy(
    const ValidatorNetworkPolicy* policy
) {
    if (policy == nullptr) {
        return 80;
    }

    if (policy->requiresManualReview()) {
        return 250;
    }

    if (policy->connectionPolicy() == "LIMITED_CONNECTION" ||
        policy->messagePolicy() == "RATE_LIMITED") {
        return 100;
    }

    return 0;
}

utils::Amount scoreWeightedAmount(
    utils::Amount plannedReward,
    std::uint16_t workScore
) {
    if (plannedReward.isNegative() ||
        workScore > NODO_PROTECTION_WORK_SCORE_MAX) {
        throw std::invalid_argument("Cannot calculate protection reward from invalid inputs.");
    }

    if (plannedReward.isZero() || workScore == 0) {
        return utils::Amount();
    }

    const std::int64_t plannedRaw = plannedReward.rawUnits();
    const std::int64_t divisor = static_cast<std::int64_t>(NODO_PROTECTION_WORK_SCORE_MAX);
    const std::int64_t multiplier = static_cast<std::int64_t>(workScore);

    const std::int64_t earnedRaw =
        (plannedRaw / divisor) * multiplier +
        ((plannedRaw % divisor) * multiplier) / divisor;

    if (earnedRaw <= 0) {
        return utils::Amount::fromRawUnits(1);
    }

    return utils::Amount::fromRawUnits(earnedRaw);
}

ProtectionWorkRecord findWorkRecordStrict(
    const std::vector<ProtectionWorkRecord>& workRecords,
    const std::string& validatorAddress
) {
    for (const ProtectionWorkRecord& work : workRecords) {
        if (work.validatorAddress() == validatorAddress) {
            return work;
        }
    }

    throw std::invalid_argument("Missing protection work record for validator.");
}

} // namespace

ProtectionWorkRecord::ProtectionWorkRecord()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_uptimeScore(0),
      m_correctVoteScore(0),
      m_attackDetectionScore(0),
      m_auditContributionScore(0),
      m_securityScore(0),
      m_riskPenaltyScore(0),
      m_totalWorkScore(0),
      m_reason(""),
      m_sourceSecurityDigest("") {}

ProtectionWorkRecord::ProtectionWorkRecord(
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
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_uptimeScore(uptimeScore),
      m_correctVoteScore(correctVoteScore),
      m_attackDetectionScore(attackDetectionScore),
      m_auditContributionScore(auditContributionScore),
      m_securityScore(securityScore),
      m_riskPenaltyScore(riskPenaltyScore),
      m_totalWorkScore(totalWorkScore),
      m_reason(std::move(reason)),
      m_sourceSecurityDigest(std::move(sourceSecurityDigest)) {}

const std::string& ProtectionWorkRecord::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ProtectionWorkRecord::blockHeight() const {
    return m_blockHeight;
}

std::uint16_t ProtectionWorkRecord::uptimeScore() const {
    return m_uptimeScore;
}

std::uint16_t ProtectionWorkRecord::correctVoteScore() const {
    return m_correctVoteScore;
}

std::uint16_t ProtectionWorkRecord::attackDetectionScore() const {
    return m_attackDetectionScore;
}

std::uint16_t ProtectionWorkRecord::auditContributionScore() const {
    return m_auditContributionScore;
}

std::uint16_t ProtectionWorkRecord::securityScore() const {
    return m_securityScore;
}

std::uint16_t ProtectionWorkRecord::riskPenaltyScore() const {
    return m_riskPenaltyScore;
}

std::uint16_t ProtectionWorkRecord::totalWorkScore() const {
    return m_totalWorkScore;
}

const std::string& ProtectionWorkRecord::reason() const {
    return m_reason;
}

const std::string& ProtectionWorkRecord::sourceSecurityDigest() const {
    return m_sourceSecurityDigest;
}

bool ProtectionWorkRecord::isValid() const {
    return !m_validatorAddress.empty() &&
           m_blockHeight > 0 &&
           m_securityScore >= SECURITY_SCORE_MIN &&
           m_securityScore <= SECURITY_SCORE_MAX &&
           m_totalWorkScore >= NODO_PROTECTION_WORK_SCORE_MIN &&
           m_totalWorkScore <= NODO_PROTECTION_WORK_SCORE_MAX &&
           m_reason == ProtectionRewards::PROTECTION_WORK_REASON &&
           !m_sourceSecurityDigest.empty();
}

std::string ProtectionWorkRecord::serialize() const {
    std::ostringstream oss;

    oss << "ProtectionWorkRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";uptimeScore=" << m_uptimeScore
        << ";correctVoteScore=" << m_correctVoteScore
        << ";attackDetectionScore=" << m_attackDetectionScore
        << ";auditContributionScore=" << m_auditContributionScore
        << ";securityScore=" << m_securityScore
        << ";riskPenaltyScore=" << m_riskPenaltyScore
        << ";totalWorkScore=" << m_totalWorkScore
        << ";reason=" << m_reason
        << ";sourceSecurityDigest=" << m_sourceSecurityDigest
        << "}";

    return oss.str();
}

ProtectionRewardSettlement::ProtectionRewardSettlement()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_plannedReward(),
      m_earnedReward(),
      m_deferredReward(),
      m_workScore(0),
      m_securityScore(0),
      m_reason(""),
      m_sourceGrantDigest(""),
      m_sourceWorkDigest("") {}

ProtectionRewardSettlement::ProtectionRewardSettlement(
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
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_plannedReward(plannedReward),
      m_earnedReward(earnedReward),
      m_deferredReward(deferredReward),
      m_workScore(workScore),
      m_securityScore(securityScore),
      m_reason(std::move(reason)),
      m_sourceGrantDigest(std::move(sourceGrantDigest)),
      m_sourceWorkDigest(std::move(sourceWorkDigest)) {}

const std::string& ProtectionRewardSettlement::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ProtectionRewardSettlement::blockHeight() const {
    return m_blockHeight;
}

utils::Amount ProtectionRewardSettlement::plannedReward() const {
    return m_plannedReward;
}

utils::Amount ProtectionRewardSettlement::earnedReward() const {
    return m_earnedReward;
}

utils::Amount ProtectionRewardSettlement::deferredReward() const {
    return m_deferredReward;
}

std::uint16_t ProtectionRewardSettlement::workScore() const {
    return m_workScore;
}

std::uint16_t ProtectionRewardSettlement::securityScore() const {
    return m_securityScore;
}

const std::string& ProtectionRewardSettlement::reason() const {
    return m_reason;
}

const std::string& ProtectionRewardSettlement::sourceGrantDigest() const {
    return m_sourceGrantDigest;
}

const std::string& ProtectionRewardSettlement::sourceWorkDigest() const {
    return m_sourceWorkDigest;
}

bool ProtectionRewardSettlement::isValid() const {
    return !m_validatorAddress.empty() &&
           m_blockHeight > 0 &&
           !m_plannedReward.isNegative() &&
           !m_earnedReward.isNegative() &&
           !m_deferredReward.isNegative() &&
           m_earnedReward + m_deferredReward == m_plannedReward &&
           m_workScore >= NODO_PROTECTION_WORK_SCORE_MIN &&
           m_workScore <= NODO_PROTECTION_WORK_SCORE_MAX &&
           m_securityScore >= SECURITY_SCORE_MIN &&
           m_securityScore <= SECURITY_SCORE_MAX &&
           m_reason == ProtectionRewards::PROTECTION_SETTLEMENT_REASON &&
           !m_sourceGrantDigest.empty() &&
           !m_sourceWorkDigest.empty();
}

std::string ProtectionRewardSettlement::serialize() const {
    std::ostringstream oss;

    oss << "ProtectionRewardSettlement{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";plannedRewardRawUnits=" << m_plannedReward.rawUnits()
        << ";earnedRewardRawUnits=" << m_earnedReward.rawUnits()
        << ";deferredRewardRawUnits=" << m_deferredReward.rawUnits()
        << ";workScore=" << m_workScore
        << ";securityScore=" << m_securityScore
        << ";reason=" << m_reason
        << ";sourceGrantDigest=" << m_sourceGrantDigest
        << ";sourceWorkDigest=" << m_sourceWorkDigest
        << "}";

    return oss.str();
}

ProtectionRewardSummary::ProtectionRewardSummary()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_plannedTotal(),
      m_earnedTotal(),
      m_deferredTotal(),
      m_beneficiaryCount(0),
      m_reason(ProtectionRewards::NOT_EVALUATED_REASON),
      m_sourceBudgetDigest("") {}

ProtectionRewardSummary::ProtectionRewardSummary(
    std::string status,
    std::uint64_t blockHeight,
    utils::Amount plannedTotal,
    utils::Amount earnedTotal,
    utils::Amount deferredTotal,
    std::uint64_t beneficiaryCount,
    std::string reason,
    std::string sourceBudgetDigest
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_plannedTotal(plannedTotal),
      m_earnedTotal(earnedTotal),
      m_deferredTotal(deferredTotal),
      m_beneficiaryCount(beneficiaryCount),
      m_reason(std::move(reason)),
      m_sourceBudgetDigest(std::move(sourceBudgetDigest)) {}

ProtectionRewardSummary ProtectionRewardSummary::notEvaluated() {
    return ProtectionRewardSummary();
}

const std::string& ProtectionRewardSummary::status() const {
    return m_status;
}

std::uint64_t ProtectionRewardSummary::blockHeight() const {
    return m_blockHeight;
}

utils::Amount ProtectionRewardSummary::plannedTotal() const {
    return m_plannedTotal;
}

utils::Amount ProtectionRewardSummary::earnedTotal() const {
    return m_earnedTotal;
}

utils::Amount ProtectionRewardSummary::deferredTotal() const {
    return m_deferredTotal;
}

std::uint64_t ProtectionRewardSummary::beneficiaryCount() const {
    return m_beneficiaryCount;
}

const std::string& ProtectionRewardSummary::reason() const {
    return m_reason;
}

const std::string& ProtectionRewardSummary::sourceBudgetDigest() const {
    return m_sourceBudgetDigest;
}

bool ProtectionRewardSummary::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool ProtectionRewardSummary::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_reason == ProtectionRewards::NOT_EVALUATED_REASON;
    }

    if (m_status != "ACTIVE" ||
        m_blockHeight == 0 ||
        m_plannedTotal.isNegative() ||
        m_earnedTotal.isNegative() ||
        m_deferredTotal.isNegative() ||
        m_earnedTotal + m_deferredTotal != m_plannedTotal ||
        m_reason != ProtectionRewards::PROTECTION_SUMMARY_REASON ||
        m_sourceBudgetDigest.empty()) {
        return false;
    }

    if (m_plannedTotal.isZero()) {
        return m_beneficiaryCount == 0;
    }

    return m_beneficiaryCount > 0;
}

std::string ProtectionRewardSummary::serialize() const {
    std::ostringstream oss;

    oss << "ProtectionRewardSummary{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";plannedTotalRawUnits=" << m_plannedTotal.rawUnits()
        << ";earnedTotalRawUnits=" << m_earnedTotal.rawUnits()
        << ";deferredTotalRawUnits=" << m_deferredTotal.rawUnits()
        << ";beneficiaryCount=" << m_beneficiaryCount
        << ";reason=" << m_reason
        << ";sourceBudgetDigest=" << m_sourceBudgetDigest
        << "}";

    return oss.str();
}

ProtectionWorkRecord ProtectionRewards::buildWorkRecord(
    const ProtectionRewardGrant& grant,
    const std::vector<SecurityScoreRecord>& securityScoreRecords,
    const std::vector<ValidatorRiskAssessment>& riskAssessments,
    const std::vector<ValidatorNetworkPolicy>& networkPolicies
) {
    if (!grant.isValid()) {
        throw std::invalid_argument("Cannot build protection work record from invalid grant.");
    }

    const SecurityScoreRecord* score =
        findSecurityScore(
            securityScoreRecords,
            grant.validatorAddress()
        );

    const ValidatorRiskAssessment* risk =
        findRiskAssessment(
            riskAssessments,
            grant.validatorAddress()
        );

    const ValidatorNetworkPolicy* policy =
        findNetworkPolicy(
            networkPolicies,
            grant.validatorAddress()
        );

    if (score == nullptr || risk == nullptr || policy == nullptr) {
        throw std::invalid_argument("Cannot build protection work record without security, risk and network policy context.");
    }

    const std::uint16_t uptimeScore = 200;
    const std::uint16_t correctVoteScore = 300;
    const std::uint16_t attackDetectionScore = 0;
    const std::uint16_t auditContributionScore =
        static_cast<std::uint16_t>(std::min<std::uint16_t>(200, score->score() / 5));

    const std::uint16_t riskPenaltyScore =
        static_cast<std::uint16_t>(riskPenaltyForLevel(risk->riskLevel()) + networkPenaltyForPolicy(policy));

    const std::int64_t rawWorkScore =
        static_cast<std::int64_t>(uptimeScore) +
        static_cast<std::int64_t>(correctVoteScore) +
        static_cast<std::int64_t>(attackDetectionScore) +
        static_cast<std::int64_t>(auditContributionScore) +
        static_cast<std::int64_t>(std::min<std::uint16_t>(250, score->score() / 4)) -
        static_cast<std::int64_t>(riskPenaltyScore);

    return ProtectionWorkRecord(
        grant.validatorAddress(),
        grant.blockHeight(),
        uptimeScore,
        correctVoteScore,
        attackDetectionScore,
        auditContributionScore,
        score->score(),
        riskPenaltyScore,
        clampScore(rawWorkScore),
        PROTECTION_WORK_REASON,
        score->serialize() + "|" + risk->serialize() + "|" + policy->serialize()
    );
}

std::vector<ProtectionWorkRecord> ProtectionRewards::buildWorkRecords(
    const std::vector<ProtectionRewardGrant>& grants,
    const std::vector<SecurityScoreRecord>& securityScoreRecords,
    const std::vector<ValidatorRiskAssessment>& riskAssessments,
    const std::vector<ValidatorNetworkPolicy>& networkPolicies
) {
    std::vector<ProtectionWorkRecord> records;
    records.reserve(grants.size());

    for (const ProtectionRewardGrant& grant : grants) {
        records.push_back(
            buildWorkRecord(
                grant,
                securityScoreRecords,
                riskAssessments,
                networkPolicies
            )
        );
    }

    return records;
}

ProtectionRewardSettlement ProtectionRewards::buildSettlement(
    const ProtectionRewardGrant& grant,
    const ProtectionWorkRecord& workRecord
) {
    if (!grant.isValid() || !workRecord.isValid()) {
        throw std::invalid_argument("Cannot settle protection reward from invalid inputs.");
    }

    if (grant.validatorAddress() != workRecord.validatorAddress() ||
        grant.blockHeight() != workRecord.blockHeight()) {
        throw std::invalid_argument("Protection reward settlement context does not match grant.");
    }

    const utils::Amount earned =
        scoreWeightedAmount(
            grant.plannedReward(),
            workRecord.totalWorkScore()
        );

    return ProtectionRewardSettlement(
        grant.validatorAddress(),
        grant.blockHeight(),
        grant.plannedReward(),
        earned,
        grant.plannedReward() - earned,
        workRecord.totalWorkScore(),
        workRecord.securityScore(),
        PROTECTION_SETTLEMENT_REASON,
        grant.serialize(),
        workRecord.serialize()
    );
}

std::vector<ProtectionRewardSettlement> ProtectionRewards::buildSettlements(
    const std::vector<ProtectionRewardGrant>& grants,
    const std::vector<ProtectionWorkRecord>& workRecords
) {
    std::vector<ProtectionRewardSettlement> settlements;
    settlements.reserve(grants.size());

    for (const ProtectionRewardGrant& grant : grants) {
        settlements.push_back(
            buildSettlement(
                grant,
                findWorkRecordStrict(
                    workRecords,
                    grant.validatorAddress()
                )
            )
        );
    }

    return settlements;
}

ProtectionRewardSummary ProtectionRewards::buildSummary(
    const ProtectionRewardBudget& budget,
    const std::vector<ProtectionRewardSettlement>& settlements
) {
    if (!budget.active()) {
        throw std::invalid_argument("Cannot summarize protection rewards from inactive budget.");
    }

    utils::Amount plannedTotal;
    utils::Amount earnedTotal;
    utils::Amount deferredTotal;

    for (const ProtectionRewardSettlement& settlement : settlements) {
        if (!settlement.isValid()) {
            throw std::invalid_argument("Cannot summarize invalid protection reward settlement.");
        }

        plannedTotal = plannedTotal + settlement.plannedReward();
        earnedTotal = earnedTotal + settlement.earnedReward();
        deferredTotal = deferredTotal + settlement.deferredReward();
    }

    if (plannedTotal != budget.plannedTotal()) {
        throw std::invalid_argument("Protection reward settlements do not match planned budget total.");
    }

    return ProtectionRewardSummary(
        "ACTIVE",
        budget.blockHeight(),
        plannedTotal,
        earnedTotal,
        deferredTotal,
        static_cast<std::uint64_t>(settlements.size()),
        PROTECTION_SUMMARY_REASON,
        budget.serialize()
    );
}

bool ProtectionRewards::sameWorkRecords(
    const std::vector<ProtectionWorkRecord>& left,
    const std::vector<ProtectionWorkRecord>& right
) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].serialize() != right[index].serialize()) {
            return false;
        }
    }

    return true;
}

bool ProtectionRewards::sameSettlements(
    const std::vector<ProtectionRewardSettlement>& left,
    const std::vector<ProtectionRewardSettlement>& right
) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].serialize() != right[index].serialize()) {
            return false;
        }
    }

    return true;
}

bool ProtectionRewards::sameSummary(
    const ProtectionRewardSummary& left,
    const ProtectionRewardSummary& right
) {
    return left.serialize() == right.serialize();
}

} // namespace nodo::node
