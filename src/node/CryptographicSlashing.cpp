#include "node/CryptographicSlashing.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

const char* noCryptographicSlashingEvidenceDigest() {
    return "NO_CRYPTOGRAPHIC_SLASHING_EVIDENCE";
}

bool votesConflict(
    const consensus::ValidatorVoteRecord& left,
    const consensus::ValidatorVoteRecord& right
) {
    return left.validatorAddress() == right.validatorAddress() &&
           left.blockIndex() == right.blockIndex() &&
           left.round() == right.round() &&
           (left.blockHash() != right.blockHash() ||
            left.previousHash() != right.previousHash() ||
            left.decision() != right.decision());
}

std::string orderedDigest(
    const std::string& left,
    const std::string& right
) {
    if (left <= right) {
        return left + "|conflict=" + right;
    }

    return right + "|conflict=" + left;
}

utils::Amount lockedStakeForValidator(
    const std::vector<LockedStakePosition>& positions,
    const std::string& validatorAddress
) {
    utils::Amount total;

    for (const LockedStakePosition& position : positions) {
        if (position.ownerAddress() == validatorAddress &&
            position.slashable()) {
            total = total + position.amount();
        }
    }

    return total;
}

utils::Amount applyBasisPoints(
    utils::Amount amount,
    std::uint32_t basisPoints
) {
    if (amount.isNegative()) {
        throw std::invalid_argument("Cannot apply penalty to negative stake.");
    }

    if (amount.isZero() || basisPoints == 0) {
        return utils::Amount();
    }

    return utils::Amount::fromRawUnits(
        (amount.rawUnits() / 10000) * static_cast<std::int64_t>(basisPoints) +
        ((amount.rawUnits() % 10000) * static_cast<std::int64_t>(basisPoints)) / 10000
    );
}

std::vector<CryptographicSlashingEvidenceRecord> buildEvidenceRecordsInternal(
    const std::vector<consensus::ValidatorVoteRecord>& votes,
    const crypto::CryptoPolicy* policy,
    const crypto::SignatureProvider* provider
) {
    std::vector<CryptographicSlashingEvidenceRecord> records;

    for (std::size_t leftIndex = 0; leftIndex < votes.size(); ++leftIndex) {
        const consensus::ValidatorVoteRecord& left = votes[leftIndex];

        if (policy != nullptr && provider != nullptr &&
            !left.verify(*policy, *provider)) {
            continue;
        }

        for (std::size_t rightIndex = leftIndex + 1; rightIndex < votes.size(); ++rightIndex) {
            const consensus::ValidatorVoteRecord& right = votes[rightIndex];

            if (policy != nullptr && provider != nullptr &&
                !right.verify(*policy, *provider)) {
                continue;
            }

            if (!votesConflict(left, right)) {
                continue;
            }

            const std::string leftDigest = left.serialize();
            const std::string rightDigest = right.serialize();

            records.emplace_back(
                left.validatorAddress(),
                left.blockIndex(),
                left.round(),
                CryptographicSlashing::DOUBLE_VOTE_EVIDENCE_TYPE,
                CryptographicSlashing::DOUBLE_VOTE_SEVERITY_SCORE,
                CryptographicSlashing::DOUBLE_VOTE_PENALTY_BASIS_POINTS,
                leftDigest <= rightDigest ? leftDigest : rightDigest,
                leftDigest <= rightDigest ? rightDigest : leftDigest,
                CryptographicSlashing::DOUBLE_VOTE_REASON,
                orderedDigest(leftDigest, rightDigest)
            );
        }
    }

    return records;
}

} // namespace

CryptographicSlashingEvidenceRecord::CryptographicSlashingEvidenceRecord()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_round(0),
      m_evidenceType(""),
      m_severityScore(0),
      m_penaltyBasisPoints(0),
      m_firstVoteDigest(""),
      m_secondVoteDigest(""),
      m_reason(""),
      m_sourceEvidenceDigest("") {}

CryptographicSlashingEvidenceRecord::CryptographicSlashingEvidenceRecord(
    std::string validatorAddress,
    std::uint64_t blockHeight,
    std::uint64_t round,
    std::string evidenceType,
    std::uint16_t severityScore,
    std::uint32_t penaltyBasisPoints,
    std::string firstVoteDigest,
    std::string secondVoteDigest,
    std::string reason,
    std::string sourceEvidenceDigest
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_round(round),
      m_evidenceType(std::move(evidenceType)),
      m_severityScore(severityScore),
      m_penaltyBasisPoints(penaltyBasisPoints),
      m_firstVoteDigest(std::move(firstVoteDigest)),
      m_secondVoteDigest(std::move(secondVoteDigest)),
      m_reason(std::move(reason)),
      m_sourceEvidenceDigest(std::move(sourceEvidenceDigest)) {}

const std::string& CryptographicSlashingEvidenceRecord::validatorAddress() const { return m_validatorAddress; }
std::uint64_t CryptographicSlashingEvidenceRecord::blockHeight() const { return m_blockHeight; }
std::uint64_t CryptographicSlashingEvidenceRecord::round() const { return m_round; }
const std::string& CryptographicSlashingEvidenceRecord::evidenceType() const { return m_evidenceType; }
std::uint16_t CryptographicSlashingEvidenceRecord::severityScore() const { return m_severityScore; }
std::uint32_t CryptographicSlashingEvidenceRecord::penaltyBasisPoints() const { return m_penaltyBasisPoints; }
const std::string& CryptographicSlashingEvidenceRecord::firstVoteDigest() const { return m_firstVoteDigest; }
const std::string& CryptographicSlashingEvidenceRecord::secondVoteDigest() const { return m_secondVoteDigest; }
const std::string& CryptographicSlashingEvidenceRecord::reason() const { return m_reason; }
const std::string& CryptographicSlashingEvidenceRecord::sourceEvidenceDigest() const { return m_sourceEvidenceDigest; }

bool CryptographicSlashingEvidenceRecord::isValid() const {
    return !m_validatorAddress.empty() &&
           m_blockHeight > 0 &&
           m_round > 0 &&
           m_evidenceType == CryptographicSlashing::DOUBLE_VOTE_EVIDENCE_TYPE &&
           m_severityScore > 0 &&
           m_severityScore <= 1000 &&
           m_penaltyBasisPoints > 0 &&
           m_penaltyBasisPoints <= 10000 &&
           !m_firstVoteDigest.empty() &&
           !m_secondVoteDigest.empty() &&
           m_firstVoteDigest != m_secondVoteDigest &&
           m_reason == CryptographicSlashing::DOUBLE_VOTE_REASON &&
           !m_sourceEvidenceDigest.empty();
}

std::string CryptographicSlashingEvidenceRecord::serialize() const {
    std::ostringstream oss;
    oss << "CryptographicSlashingEvidenceRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";round=" << m_round
        << ";evidenceType=" << m_evidenceType
        << ";severityScore=" << m_severityScore
        << ";penaltyBasisPoints=" << m_penaltyBasisPoints
        << ";firstVoteDigest=" << m_firstVoteDigest
        << ";secondVoteDigest=" << m_secondVoteDigest
        << ";reason=" << m_reason
        << ";sourceEvidenceDigest=" << m_sourceEvidenceDigest
        << "}";
    return oss.str();
}

StakePenaltyRecord::StakePenaltyRecord()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_lockedStakeBefore(),
      m_penaltyAmount(),
      m_lockedStakeAfter(),
      m_evidenceCount(0),
      m_reason(""),
      m_sourceEvidenceDigest("") {}

StakePenaltyRecord::StakePenaltyRecord(
    std::string validatorAddress,
    std::uint64_t blockHeight,
    utils::Amount lockedStakeBefore,
    utils::Amount penaltyAmount,
    utils::Amount lockedStakeAfter,
    std::uint64_t evidenceCount,
    std::string reason,
    std::string sourceEvidenceDigest
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_lockedStakeBefore(lockedStakeBefore),
      m_penaltyAmount(penaltyAmount),
      m_lockedStakeAfter(lockedStakeAfter),
      m_evidenceCount(evidenceCount),
      m_reason(std::move(reason)),
      m_sourceEvidenceDigest(std::move(sourceEvidenceDigest)) {}

const std::string& StakePenaltyRecord::validatorAddress() const { return m_validatorAddress; }
std::uint64_t StakePenaltyRecord::blockHeight() const { return m_blockHeight; }
utils::Amount StakePenaltyRecord::lockedStakeBefore() const { return m_lockedStakeBefore; }
utils::Amount StakePenaltyRecord::penaltyAmount() const { return m_penaltyAmount; }
utils::Amount StakePenaltyRecord::lockedStakeAfter() const { return m_lockedStakeAfter; }
std::uint64_t StakePenaltyRecord::evidenceCount() const { return m_evidenceCount; }
const std::string& StakePenaltyRecord::reason() const { return m_reason; }
const std::string& StakePenaltyRecord::sourceEvidenceDigest() const { return m_sourceEvidenceDigest; }

bool StakePenaltyRecord::isValid() const {
    return !m_validatorAddress.empty() &&
           m_blockHeight > 0 &&
           !m_lockedStakeBefore.isNegative() &&
           !m_penaltyAmount.isNegative() &&
           !m_lockedStakeAfter.isNegative() &&
           m_penaltyAmount <= m_lockedStakeBefore &&
           m_lockedStakeAfter == m_lockedStakeBefore - m_penaltyAmount &&
           m_evidenceCount > 0 &&
           m_reason == CryptographicSlashing::STAKE_PENALTY_REASON &&
           !m_sourceEvidenceDigest.empty();
}

std::string StakePenaltyRecord::serialize() const {
    std::ostringstream oss;
    oss << "StakePenaltyRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";lockedStakeBeforeRawUnits=" << m_lockedStakeBefore.rawUnits()
        << ";penaltyAmountRawUnits=" << m_penaltyAmount.rawUnits()
        << ";lockedStakeAfterRawUnits=" << m_lockedStakeAfter.rawUnits()
        << ";evidenceCount=" << m_evidenceCount
        << ";reason=" << m_reason
        << ";sourceEvidenceDigest=" << m_sourceEvidenceDigest
        << "}";
    return oss.str();
}

CryptographicSlashingSummary::CryptographicSlashingSummary()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_evidenceCount(0),
      m_slashableEvidenceCount(0),
      m_maxSeverityScore(0),
      m_penaltyTotal(),
      m_reason(CryptographicSlashing::NOT_EVALUATED_REASON),
      m_sourcePenaltyDigest("") {}

CryptographicSlashingSummary::CryptographicSlashingSummary(
    std::string status,
    std::uint64_t blockHeight,
    std::uint64_t evidenceCount,
    std::uint64_t slashableEvidenceCount,
    std::uint16_t maxSeverityScore,
    utils::Amount penaltyTotal,
    std::string reason,
    std::string sourcePenaltyDigest
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_evidenceCount(evidenceCount),
      m_slashableEvidenceCount(slashableEvidenceCount),
      m_maxSeverityScore(maxSeverityScore),
      m_penaltyTotal(penaltyTotal),
      m_reason(std::move(reason)),
      m_sourcePenaltyDigest(std::move(sourcePenaltyDigest)) {}

CryptographicSlashingSummary CryptographicSlashingSummary::notEvaluated() {
    return CryptographicSlashingSummary();
}

const std::string& CryptographicSlashingSummary::status() const { return m_status; }
std::uint64_t CryptographicSlashingSummary::blockHeight() const { return m_blockHeight; }
std::uint64_t CryptographicSlashingSummary::evidenceCount() const { return m_evidenceCount; }
std::uint64_t CryptographicSlashingSummary::slashableEvidenceCount() const { return m_slashableEvidenceCount; }
std::uint16_t CryptographicSlashingSummary::maxSeverityScore() const { return m_maxSeverityScore; }
utils::Amount CryptographicSlashingSummary::penaltyTotal() const { return m_penaltyTotal; }
const std::string& CryptographicSlashingSummary::reason() const { return m_reason; }
const std::string& CryptographicSlashingSummary::sourcePenaltyDigest() const { return m_sourcePenaltyDigest; }

bool CryptographicSlashingSummary::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool CryptographicSlashingSummary::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_blockHeight == 0 &&
               m_evidenceCount == 0 &&
               m_slashableEvidenceCount == 0 &&
               m_maxSeverityScore == 0 &&
               m_penaltyTotal.isZero() &&
               m_reason == CryptographicSlashing::NOT_EVALUATED_REASON;
    }

    if (m_status != "ACTIVE" ||
        m_blockHeight == 0 ||
        m_slashableEvidenceCount > m_evidenceCount ||
        m_maxSeverityScore > 1000 ||
        m_penaltyTotal.isNegative() ||
        m_reason != CryptographicSlashing::SLASHING_SUMMARY_REASON) {
        return false;
    }

    if (m_evidenceCount == 0) {
        return m_slashableEvidenceCount == 0 &&
            m_maxSeverityScore == 0 &&
            m_penaltyTotal.isZero() &&
            m_sourcePenaltyDigest == noCryptographicSlashingEvidenceDigest();
    }

    return !m_sourcePenaltyDigest.empty();
}

std::string CryptographicSlashingSummary::serialize() const {
    std::ostringstream oss;
    oss << "CryptographicSlashingSummary{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";evidenceCount=" << m_evidenceCount
        << ";slashableEvidenceCount=" << m_slashableEvidenceCount
        << ";maxSeverityScore=" << m_maxSeverityScore
        << ";penaltyTotalRawUnits=" << m_penaltyTotal.rawUnits()
        << ";reason=" << m_reason
        << ";sourcePenaltyDigest=" << m_sourcePenaltyDigest
        << "}";
    return oss.str();
}

std::vector<CryptographicSlashingEvidenceRecord> CryptographicSlashing::buildEvidenceRecords(
    const std::vector<consensus::ValidatorVoteRecord>& votes,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    return buildEvidenceRecordsInternal(
        votes,
        &policy,
        &provider
    );
}

std::vector<CryptographicSlashingEvidenceRecord> CryptographicSlashing::buildEvidenceRecordsFromCertifiedVotes(
    const std::vector<consensus::ValidatorVoteRecord>& votes
) {
    return buildEvidenceRecordsInternal(
        votes,
        nullptr,
        nullptr
    );
}

std::vector<StakePenaltyRecord> CryptographicSlashing::buildStakePenaltyRecords(
    const std::vector<CryptographicSlashingEvidenceRecord>& evidenceRecords,
    const std::vector<LockedStakePosition>& lockedStakePositions
) {
    std::map<std::string, std::vector<CryptographicSlashingEvidenceRecord>> byValidator;

    for (const CryptographicSlashingEvidenceRecord& record : evidenceRecords) {
        if (!record.isValid()) {
            throw std::invalid_argument("Cannot build stake penalty from invalid cryptographic evidence.");
        }
        byValidator[record.validatorAddress()].push_back(record);
    }

    std::vector<StakePenaltyRecord> penalties;

    for (const auto& entry : byValidator) {
        const std::string& validatorAddress = entry.first;
        const std::vector<CryptographicSlashingEvidenceRecord>& records = entry.second;

        std::uint64_t blockHeight = 0;
        std::uint32_t maxPenaltyBasisPoints = 0;
        std::ostringstream digest;

        for (const CryptographicSlashingEvidenceRecord& record : records) {
            blockHeight = std::max(blockHeight, record.blockHeight());
            maxPenaltyBasisPoints = std::max(maxPenaltyBasisPoints, record.penaltyBasisPoints());
            digest << record.serialize();
        }

        const utils::Amount lockedStake =
            lockedStakeForValidator(
                lockedStakePositions,
                validatorAddress
            );

        const utils::Amount penalty =
            applyBasisPoints(
                lockedStake,
                maxPenaltyBasisPoints
            );

        if (penalty.isZero()) {
            continue;
        }

        penalties.emplace_back(
            validatorAddress,
            blockHeight,
            lockedStake,
            penalty,
            lockedStake - penalty,
            static_cast<std::uint64_t>(records.size()),
            STAKE_PENALTY_REASON,
            digest.str()
        );
    }

    return penalties;
}

CryptographicSlashingSummary CryptographicSlashing::buildSummary(
    std::uint64_t blockHeight,
    const std::vector<CryptographicSlashingEvidenceRecord>& evidenceRecords,
    const std::vector<StakePenaltyRecord>& penaltyRecords
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build cryptographic slashing summary at genesis height.");
    }

    std::uint64_t slashableCount = 0;
    std::uint16_t maxSeverity = 0;

    for (const CryptographicSlashingEvidenceRecord& record : evidenceRecords) {
        if (!record.isValid()) {
            throw std::invalid_argument("Cannot summarize invalid cryptographic slashing evidence.");
        }
        ++slashableCount;
        maxSeverity = std::max(maxSeverity, record.severityScore());
    }

    utils::Amount penaltyTotal;
    std::ostringstream digest;

    for (const StakePenaltyRecord& penalty : penaltyRecords) {
        if (!penalty.isValid()) {
            throw std::invalid_argument("Cannot summarize invalid stake penalty record.");
        }
        penaltyTotal = penaltyTotal + penalty.penaltyAmount();
        digest << penalty.serialize();
    }

    if (penaltyRecords.empty()) {
        for (const CryptographicSlashingEvidenceRecord& record : evidenceRecords) {
            digest << record.serialize();
        }
    }

    if (digest.str().empty()) {
        digest << "NO_CRYPTOGRAPHIC_SLASHING_EVIDENCE";
    }

    const std::string sourcePenaltyDigest =
        digest.str().empty()
            ? noCryptographicSlashingEvidenceDigest()
            : digest.str();

    return CryptographicSlashingSummary(
        "ACTIVE",
        blockHeight,
        static_cast<std::uint64_t>(evidenceRecords.size()),
        slashableCount,
        maxSeverity,
        penaltyTotal,
        SLASHING_SUMMARY_REASON,
        sourcePenaltyDigest
    );
}

bool CryptographicSlashing::sameEvidenceRecords(
    const std::vector<CryptographicSlashingEvidenceRecord>& left,
    const std::vector<CryptographicSlashingEvidenceRecord>& right
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

bool CryptographicSlashing::sameStakePenaltyRecords(
    const std::vector<StakePenaltyRecord>& left,
    const std::vector<StakePenaltyRecord>& right
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

bool CryptographicSlashing::sameSummary(
    const CryptographicSlashingSummary& left,
    const CryptographicSlashingSummary& right
) {
    return left.serialize() == right.serialize();
}

} // namespace nodo::node
