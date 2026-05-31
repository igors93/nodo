#include "node/SlashingEvidence.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

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

const ProtectionWorkRecord* findWorkRecord(
    const std::vector<ProtectionWorkRecord>& records,
    const std::string& validatorAddress
) {
    for (const ProtectionWorkRecord& record : records) {
        if (record.validatorAddress() == validatorAddress) {
            return &record;
        }
    }

    return nullptr;
}

utils::Amount lockedStakeForValidator(
    const std::vector<LockedStakePosition>& positions,
    const std::string& validatorAddress
) {
    utils::Amount total;

    for (const LockedStakePosition& position : positions) {
        if (position.ownerAddress() == validatorAddress) {
            total = total + position.amount();
        }
    }

    return total;
}

std::uint16_t severityForRiskLevel(
    const std::string& riskLevel
) {
    if (riskLevel == "HIGH") {
        return 900;
    }

    if (riskLevel == "ELEVATED") {
        return 650;
    }

    if (riskLevel == "MODERATE") {
        return 350;
    }

    if (riskLevel == "LOW") {
        return 0;
    }

    return 250;
}

bool shouldRecordRiskEvidence(
    const ValidatorRiskAssessment& risk,
    const ValidatorNetworkPolicy* policy,
    const ProtectionWorkRecord* work
) {
    if (risk.riskLevel() == "HIGH" ||
        risk.riskLevel() == "ELEVATED") {
        return true;
    }

    if (policy != nullptr && policy->requiresManualReview()) {
        return true;
    }

    if (work != nullptr && work->totalWorkScore() < 500) {
        return true;
    }

    return false;
}

std::string evidenceTypeFor(
    const ValidatorRiskAssessment& risk,
    const ValidatorNetworkPolicy* policy
) {
    if (policy != nullptr && policy->requiresManualReview()) {
        return "RISK_CONTAINMENT_SIGNAL";
    }

    if (risk.riskLevel() == "HIGH") {
        return "HIGH_RISK_SIGNAL";
    }

    if (risk.riskLevel() == "ELEVATED") {
        return "ELEVATED_RISK_SIGNAL";
    }

    return "SECURITY_REVIEW_SIGNAL";
}

std::string evidenceDigest(
    const ValidatorRiskAssessment& risk,
    const ValidatorNetworkPolicy* policy,
    const ProtectionWorkRecord* work
) {
    std::ostringstream oss;
    oss << risk.serialize();

    if (policy != nullptr) {
        oss << "|policy=" << policy->serialize();
    }

    if (work != nullptr) {
        oss << "|work=" << work->serialize();
    }

    return oss.str();
}

std::string preparationActionForSeverity(
    std::uint16_t maxSeverityScore,
    std::uint64_t slashableEvidenceCount
) {
    if (slashableEvidenceCount > 0 && maxSeverityScore >= 850) {
        return "PREPARE_SLASHING_REVIEW";
    }

    if (maxSeverityScore >= 850) {
        return "PREPARE_MANUAL_SECURITY_REVIEW";
    }

    if (maxSeverityScore >= 650) {
        return "PREPARE_RESTRICTIVE_REVIEW";
    }

    if (maxSeverityScore > 0) {
        return "PREPARE_OBSERVATION_REVIEW";
    }

    return "NO_ACTION";
}

utils::Amount preparedPenalty(
    utils::Amount lockedStake,
    std::uint16_t maxSeverityScore,
    std::uint64_t slashableEvidenceCount
) {
    if (lockedStake.isNegative() ||
        lockedStake.isZero() ||
        slashableEvidenceCount == 0 ||
        maxSeverityScore < 850) {
        return utils::Amount();
    }

    return utils::Amount::fromRawUnits(
        lockedStake.rawUnits() / 10
    );
}

} // namespace

SlashingEvidenceRecord::SlashingEvidenceRecord()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_evidenceType(""),
      m_severityScore(0),
      m_slashable(false),
      m_recommendedAction(""),
      m_reason(""),
      m_sourceSecurityDigest("") {}

SlashingEvidenceRecord::SlashingEvidenceRecord(
    std::string validatorAddress,
    std::uint64_t blockHeight,
    std::string evidenceType,
    std::uint16_t severityScore,
    bool slashable,
    std::string recommendedAction,
    std::string reason,
    std::string sourceSecurityDigest
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_evidenceType(std::move(evidenceType)),
      m_severityScore(severityScore),
      m_slashable(slashable),
      m_recommendedAction(std::move(recommendedAction)),
      m_reason(std::move(reason)),
      m_sourceSecurityDigest(std::move(sourceSecurityDigest)) {}

const std::string& SlashingEvidenceRecord::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t SlashingEvidenceRecord::blockHeight() const {
    return m_blockHeight;
}

const std::string& SlashingEvidenceRecord::evidenceType() const {
    return m_evidenceType;
}

std::uint16_t SlashingEvidenceRecord::severityScore() const {
    return m_severityScore;
}

bool SlashingEvidenceRecord::slashable() const {
    return m_slashable;
}

const std::string& SlashingEvidenceRecord::recommendedAction() const {
    return m_recommendedAction;
}

const std::string& SlashingEvidenceRecord::reason() const {
    return m_reason;
}

const std::string& SlashingEvidenceRecord::sourceSecurityDigest() const {
    return m_sourceSecurityDigest;
}

bool SlashingEvidenceRecord::isValid() const {
    return !m_validatorAddress.empty() &&
           m_blockHeight > 0 &&
           !m_evidenceType.empty() &&
           m_severityScore > 0 &&
           m_severityScore <= 1000 &&
           !m_recommendedAction.empty() &&
           m_reason == SlashingEvidence::RISK_CONTAINMENT_EVIDENCE_REASON &&
           !m_sourceSecurityDigest.empty();
}

std::string SlashingEvidenceRecord::serialize() const {
    std::ostringstream oss;
    oss << "SlashingEvidenceRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";evidenceType=" << m_evidenceType
        << ";severityScore=" << m_severityScore
        << ";slashable=" << (m_slashable ? "true" : "false")
        << ";recommendedAction=" << m_recommendedAction
        << ";reason=" << m_reason
        << ";sourceSecurityDigest=" << m_sourceSecurityDigest
        << "}";
    return oss.str();
}

SlashingPreparationRecord::SlashingPreparationRecord()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_evidenceCount(0),
      m_slashableEvidenceCount(0),
      m_maxSeverityScore(0),
      m_preparedPenaltyAmount(),
      m_enforcementAction(""),
      m_reason(""),
      m_sourceEvidenceDigest("") {}

SlashingPreparationRecord::SlashingPreparationRecord(
    std::string validatorAddress,
    std::uint64_t blockHeight,
    std::uint64_t evidenceCount,
    std::uint64_t slashableEvidenceCount,
    std::uint16_t maxSeverityScore,
    utils::Amount preparedPenaltyAmount,
    std::string enforcementAction,
    std::string reason,
    std::string sourceEvidenceDigest
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_evidenceCount(evidenceCount),
      m_slashableEvidenceCount(slashableEvidenceCount),
      m_maxSeverityScore(maxSeverityScore),
      m_preparedPenaltyAmount(preparedPenaltyAmount),
      m_enforcementAction(std::move(enforcementAction)),
      m_reason(std::move(reason)),
      m_sourceEvidenceDigest(std::move(sourceEvidenceDigest)) {}

const std::string& SlashingPreparationRecord::validatorAddress() const { return m_validatorAddress; }
std::uint64_t SlashingPreparationRecord::blockHeight() const { return m_blockHeight; }
std::uint64_t SlashingPreparationRecord::evidenceCount() const { return m_evidenceCount; }
std::uint64_t SlashingPreparationRecord::slashableEvidenceCount() const { return m_slashableEvidenceCount; }
std::uint16_t SlashingPreparationRecord::maxSeverityScore() const { return m_maxSeverityScore; }
utils::Amount SlashingPreparationRecord::preparedPenaltyAmount() const { return m_preparedPenaltyAmount; }
const std::string& SlashingPreparationRecord::enforcementAction() const { return m_enforcementAction; }
const std::string& SlashingPreparationRecord::reason() const { return m_reason; }
const std::string& SlashingPreparationRecord::sourceEvidenceDigest() const { return m_sourceEvidenceDigest; }

bool SlashingPreparationRecord::isValid() const {
    if (m_validatorAddress.empty() ||
        m_blockHeight == 0 ||
        m_evidenceCount == 0 ||
        m_slashableEvidenceCount > m_evidenceCount ||
        m_maxSeverityScore == 0 ||
        m_maxSeverityScore > 1000 ||
        m_preparedPenaltyAmount.isNegative() ||
        m_enforcementAction.empty() ||
        m_reason != SlashingEvidence::SLASHING_PREPARATION_REASON ||
        m_sourceEvidenceDigest.empty()) {
        return false;
    }

    if (m_slashableEvidenceCount == 0 && !m_preparedPenaltyAmount.isZero()) {
        return false;
    }

    return true;
}

std::string SlashingPreparationRecord::serialize() const {
    std::ostringstream oss;
    oss << "SlashingPreparationRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";evidenceCount=" << m_evidenceCount
        << ";slashableEvidenceCount=" << m_slashableEvidenceCount
        << ";maxSeverityScore=" << m_maxSeverityScore
        << ";preparedPenaltyRawUnits=" << m_preparedPenaltyAmount.rawUnits()
        << ";enforcementAction=" << m_enforcementAction
        << ";reason=" << m_reason
        << ";sourceEvidenceDigest=" << m_sourceEvidenceDigest
        << "}";
    return oss.str();
}

SlashingEvidenceSummary::SlashingEvidenceSummary()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_evidenceCount(0),
      m_slashableEvidenceCount(0),
      m_maxSeverityScore(0),
      m_preparedPenaltyTotal(),
      m_reason(SlashingEvidence::NOT_EVALUATED_REASON),
      m_sourcePreparationDigest("") {}

SlashingEvidenceSummary::SlashingEvidenceSummary(
    std::string status,
    std::uint64_t blockHeight,
    std::uint64_t evidenceCount,
    std::uint64_t slashableEvidenceCount,
    std::uint16_t maxSeverityScore,
    utils::Amount preparedPenaltyTotal,
    std::string reason,
    std::string sourcePreparationDigest
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_evidenceCount(evidenceCount),
      m_slashableEvidenceCount(slashableEvidenceCount),
      m_maxSeverityScore(maxSeverityScore),
      m_preparedPenaltyTotal(preparedPenaltyTotal),
      m_reason(std::move(reason)),
      m_sourcePreparationDigest(std::move(sourcePreparationDigest)) {}

SlashingEvidenceSummary SlashingEvidenceSummary::notEvaluated() {
    return SlashingEvidenceSummary();
}

const std::string& SlashingEvidenceSummary::status() const { return m_status; }
std::uint64_t SlashingEvidenceSummary::blockHeight() const { return m_blockHeight; }
std::uint64_t SlashingEvidenceSummary::evidenceCount() const { return m_evidenceCount; }
std::uint64_t SlashingEvidenceSummary::slashableEvidenceCount() const { return m_slashableEvidenceCount; }
std::uint16_t SlashingEvidenceSummary::maxSeverityScore() const { return m_maxSeverityScore; }
utils::Amount SlashingEvidenceSummary::preparedPenaltyTotal() const { return m_preparedPenaltyTotal; }
const std::string& SlashingEvidenceSummary::reason() const { return m_reason; }
const std::string& SlashingEvidenceSummary::sourcePreparationDigest() const { return m_sourcePreparationDigest; }

bool SlashingEvidenceSummary::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool SlashingEvidenceSummary::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_blockHeight == 0 &&
               m_evidenceCount == 0 &&
               m_slashableEvidenceCount == 0 &&
               m_maxSeverityScore == 0 &&
               m_preparedPenaltyTotal.isZero() &&
               m_reason == SlashingEvidence::NOT_EVALUATED_REASON;
    }

    if (m_status != "ACTIVE" ||
        m_blockHeight == 0 ||
        m_slashableEvidenceCount > m_evidenceCount ||
        m_maxSeverityScore > 1000 ||
        m_preparedPenaltyTotal.isNegative() ||
        m_reason != SlashingEvidence::SLASHING_SUMMARY_REASON) {
        return false;
    }

    if (m_evidenceCount == 0) {
        return m_slashableEvidenceCount == 0 &&
               m_maxSeverityScore == 0 &&
               m_preparedPenaltyTotal.isZero() &&
               m_sourcePreparationDigest.empty();
    }

    return !m_sourcePreparationDigest.empty();
}

std::string SlashingEvidenceSummary::serialize() const {
    std::ostringstream oss;
    oss << "SlashingEvidenceSummary{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";evidenceCount=" << m_evidenceCount
        << ";slashableEvidenceCount=" << m_slashableEvidenceCount
        << ";maxSeverityScore=" << m_maxSeverityScore
        << ";preparedPenaltyTotalRawUnits=" << m_preparedPenaltyTotal.rawUnits()
        << ";reason=" << m_reason
        << ";sourcePreparationDigest=" << m_sourcePreparationDigest
        << "}";
    return oss.str();
}

std::vector<SlashingEvidenceRecord> SlashingEvidence::buildEvidenceRecords(
    const std::vector<ValidatorRiskAssessment>& riskAssessments,
    const std::vector<ValidatorNetworkPolicy>& networkPolicies,
    const std::vector<ProtectionWorkRecord>& protectionWorkRecords
) {
    std::vector<SlashingEvidenceRecord> records;

    for (const ValidatorRiskAssessment& risk : riskAssessments) {
        if (!risk.isValid()) {
            throw std::invalid_argument("Cannot build slashing evidence from invalid risk assessment.");
        }

        const ValidatorNetworkPolicy* policy =
            findNetworkPolicy(networkPolicies, risk.validatorAddress());
        const ProtectionWorkRecord* work =
            findWorkRecord(protectionWorkRecords, risk.validatorAddress());

        if (!shouldRecordRiskEvidence(risk, policy, work)) {
            continue;
        }

        const std::uint16_t severity =
            std::max(
                severityForRiskLevel(risk.riskLevel()),
                work != nullptr && work->totalWorkScore() < 500 ? static_cast<std::uint16_t>(500 - work->totalWorkScore()) : static_cast<std::uint16_t>(0)
            );

        records.emplace_back(
            risk.validatorAddress(),
            risk.blockHeight(),
            evidenceTypeFor(risk, policy),
            severity == 0 ? static_cast<std::uint16_t>(1) : severity,
            false,
            risk.recommendedAction(),
            RISK_CONTAINMENT_EVIDENCE_REASON,
            evidenceDigest(risk, policy, work)
        );
    }

    return records;
}

std::vector<SlashingPreparationRecord> SlashingEvidence::buildPreparationRecords(
    const std::vector<SlashingEvidenceRecord>& evidenceRecords,
    const std::vector<LockedStakePosition>& lockedStakePositions
) {
    std::map<std::string, std::vector<SlashingEvidenceRecord>> byValidator;

    for (const SlashingEvidenceRecord& record : evidenceRecords) {
        if (!record.isValid()) {
            throw std::invalid_argument("Cannot prepare slashing review from invalid evidence record.");
        }
        byValidator[record.validatorAddress()].push_back(record);
    }

    std::vector<SlashingPreparationRecord> preparations;

    for (const auto& entry : byValidator) {
        const std::string& validatorAddress = entry.first;
        const std::vector<SlashingEvidenceRecord>& records = entry.second;
        std::uint64_t slashableCount = 0;
        std::uint16_t maxSeverity = 0;
        std::uint64_t blockHeight = 0;
        std::ostringstream digest;

        for (const SlashingEvidenceRecord& record : records) {
            if (record.slashable()) {
                ++slashableCount;
            }
            maxSeverity = std::max(maxSeverity, record.severityScore());
            blockHeight = std::max(blockHeight, record.blockHeight());
            digest << record.serialize();
        }

        const utils::Amount penalty =
            preparedPenalty(
                lockedStakeForValidator(lockedStakePositions, validatorAddress),
                maxSeverity,
                slashableCount
            );

        preparations.emplace_back(
            validatorAddress,
            blockHeight,
            static_cast<std::uint64_t>(records.size()),
            slashableCount,
            maxSeverity,
            penalty,
            preparationActionForSeverity(maxSeverity, slashableCount),
            SLASHING_PREPARATION_REASON,
            digest.str()
        );
    }

    return preparations;
}

SlashingEvidenceSummary SlashingEvidence::buildSummary(
    std::uint64_t blockHeight,
    const std::vector<SlashingEvidenceRecord>& evidenceRecords,
    const std::vector<SlashingPreparationRecord>& preparationRecords
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build slashing evidence summary at genesis height.");
    }

    std::uint64_t slashableCount = 0;
    std::uint16_t maxSeverity = 0;

    for (const SlashingEvidenceRecord& record : evidenceRecords) {
        if (!record.isValid()) {
            throw std::invalid_argument("Cannot summarize invalid slashing evidence record.");
        }
        if (record.slashable()) {
            ++slashableCount;
        }
        maxSeverity = std::max(maxSeverity, record.severityScore());
    }

    utils::Amount preparedPenaltyTotal;
    std::ostringstream digest;

    for (const SlashingPreparationRecord& record : preparationRecords) {
        if (!record.isValid()) {
            throw std::invalid_argument("Cannot summarize invalid slashing preparation record.");
        }
        preparedPenaltyTotal = preparedPenaltyTotal + record.preparedPenaltyAmount();
        digest << record.serialize();
    }

    return SlashingEvidenceSummary(
        "ACTIVE",
        blockHeight,
        static_cast<std::uint64_t>(evidenceRecords.size()),
        slashableCount,
        maxSeverity,
        preparedPenaltyTotal,
        SLASHING_SUMMARY_REASON,
        digest.str()
    );
}

bool SlashingEvidence::sameEvidenceRecords(
    const std::vector<SlashingEvidenceRecord>& left,
    const std::vector<SlashingEvidenceRecord>& right
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

bool SlashingEvidence::samePreparationRecords(
    const std::vector<SlashingPreparationRecord>& left,
    const std::vector<SlashingPreparationRecord>& right
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

bool SlashingEvidence::sameSummary(
    const SlashingEvidenceSummary& left,
    const SlashingEvidenceSummary& right
) {
    return left.serialize() == right.serialize();
}

} // namespace nodo::node
