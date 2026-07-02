#include "economics/ValidatorPenaltyLedgerBuilder.hpp"

#include "core/ValidatorProposalRegistry.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

ValidatorPenaltyLedgerBuildResult::ValidatorPenaltyLedgerBuildResult()
    : m_penaltyRecord(), m_scoreRecord(), m_records() {}

ValidatorPenaltyLedgerBuildResult::ValidatorPenaltyLedgerBuildResult(
    ValidatorPenaltyRecord penaltyRecord, ValidatorScoreRecord scoreRecord,
    std::vector<core::LedgerRecord> records)
    : m_penaltyRecord(std::move(penaltyRecord)),
      m_scoreRecord(std::move(scoreRecord)), m_records(std::move(records)) {}

const ValidatorPenaltyRecord &
ValidatorPenaltyLedgerBuildResult::penaltyRecord() const {
  return m_penaltyRecord;
}

const ValidatorScoreRecord &
ValidatorPenaltyLedgerBuildResult::scoreRecord() const {
  return m_scoreRecord;
}

const std::vector<core::LedgerRecord> &
ValidatorPenaltyLedgerBuildResult::records() const {
  return m_records;
}

bool ValidatorPenaltyLedgerBuildResult::isValid() const {
  return ValidatorPenaltyLedgerBuilder::recordsMatchPenalty(
      m_penaltyRecord, m_scoreRecord, m_records);
}

std::string ValidatorPenaltyLedgerBuildResult::serialize() const {
  std::ostringstream oss;

  oss << "ValidatorPenaltyLedgerBuildResult{"
      << "penaltyId="
      << (m_penaltyRecord.isValid() ? m_penaltyRecord.deterministicId()
                                    : "INVALID")
      << ";recordCount=" << m_records.size() << "}";

  return oss.str();
}

ValidatorPenaltyLedgerBuildResult
ValidatorPenaltyLedgerBuilder::buildDoubleSignPenaltyRecords(
    const core::ValidatorDoubleSignEvidence &evidence,
    const ValidatorPenaltyPolicy &policy, std::uint64_t epoch,
    std::int32_t previousScore, std::int64_t timestamp) {
  if (!evidence.isValid()) {
    throw std::invalid_argument(
        "Invalid double-sign evidence rejected by penalty ledger builder.");
  }

  const ValidatorPenaltyRecord penaltyRecord =
      policy.createDoubleSignPenaltyRecord(evidence, epoch, previousScore,
                                           timestamp);

  const ValidatorScoreRecord scoreRecord = penaltyRecord.createScoreRecord();

  std::vector<core::LedgerRecord> records;
  records.push_back(
      core::LedgerRecord::fromValidatorPenaltyRecord(penaltyRecord, timestamp));
  records.push_back(
      core::LedgerRecord::fromValidatorScoreRecord(scoreRecord, timestamp));

  ValidatorPenaltyLedgerBuildResult result(penaltyRecord, scoreRecord, records);

  if (!result.isValid()) {
    throw std::logic_error(
        "ValidatorPenaltyLedgerBuilder produced invalid records.");
  }

  return result;
}

bool ValidatorPenaltyLedgerBuilder::recordsMatchPenalty(
    const ValidatorPenaltyRecord &penaltyRecord,
    const ValidatorScoreRecord &scoreRecord,
    const std::vector<core::LedgerRecord> &records) {
  if (!penaltyRecord.isValid() || !scoreRecord.isValid()) {
    return false;
  }

  if (records.size() != 2U) {
    return false;
  }

  if (!records[0].isValid() || !records[1].isValid()) {
    return false;
  }

  if (records[0].type() != core::LedgerRecordType::VALIDATOR_PENALTY) {
    return false;
  }

  if (records[1].type() != core::LedgerRecordType::VALIDATOR_SCORE) {
    return false;
  }

  if (records[0].sourceId() != penaltyRecord.deterministicId()) {
    return false;
  }

  if (records[0].payload() != penaltyRecord.serialize()) {
    return false;
  }

  if (records[1].payload() != scoreRecord.serialize()) {
    return false;
  }

  if (scoreRecord.evidenceHash() != penaltyRecord.deterministicId()) {
    return false;
  }

  if (scoreRecord.validatorAddress() != penaltyRecord.validatorAddress()) {
    return false;
  }

  if (scoreRecord.previousScore() != penaltyRecord.previousScore() ||
      scoreRecord.newScore() != penaltyRecord.newScore()) {
    return false;
  }

  return true;
}

} // namespace nodo::economics
