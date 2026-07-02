#include "economics/ValidatorScoreRecord.hpp"

#include "serialization/FieldCodec.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

std::string validatorScoreReasonToString(ValidatorScoreReason reason) {
  switch (reason) {
  case ValidatorScoreReason::INITIAL_REGISTRATION:
    return "INITIAL_REGISTRATION";

  case ValidatorScoreReason::CONSISTENT_VALIDATION:
    return "CONSISTENT_VALIDATION";

  case ValidatorScoreReason::SUCCESSFUL_CHALLENGE_RESPONSE:
    return "SUCCESSFUL_CHALLENGE_RESPONSE";

  case ValidatorScoreReason::USEFUL_STORAGE_SERVICE:
    return "USEFUL_STORAGE_SERVICE";

  case ValidatorScoreReason::NETWORK_CLUSTER_PENALTY:
    return "NETWORK_CLUSTER_PENALTY";

  case ValidatorScoreReason::MISSED_CHALLENGE:
    return "MISSED_CHALLENGE";

  case ValidatorScoreReason::INVALID_WORK:
    return "INVALID_WORK";

  case ValidatorScoreReason::CONFLICTING_SIGNATURE:
    return "CONFLICTING_SIGNATURE";

  case ValidatorScoreReason::MANUAL_REVIEW:
    return "MANUAL_REVIEW";

  default:
    return "UNKNOWN";
  }
}

ValidatorScoreReason validatorScoreReasonFromString(const std::string &value) {
  if (value == "INITIAL_REGISTRATION")
    return ValidatorScoreReason::INITIAL_REGISTRATION;
  if (value == "CONSISTENT_VALIDATION")
    return ValidatorScoreReason::CONSISTENT_VALIDATION;
  if (value == "SUCCESSFUL_CHALLENGE_RESPONSE")
    return ValidatorScoreReason::SUCCESSFUL_CHALLENGE_RESPONSE;
  if (value == "USEFUL_STORAGE_SERVICE")
    return ValidatorScoreReason::USEFUL_STORAGE_SERVICE;
  if (value == "NETWORK_CLUSTER_PENALTY")
    return ValidatorScoreReason::NETWORK_CLUSTER_PENALTY;
  if (value == "MISSED_CHALLENGE")
    return ValidatorScoreReason::MISSED_CHALLENGE;
  if (value == "INVALID_WORK")
    return ValidatorScoreReason::INVALID_WORK;
  if (value == "CONFLICTING_SIGNATURE")
    return ValidatorScoreReason::CONFLICTING_SIGNATURE;
  if (value == "MANUAL_REVIEW")
    return ValidatorScoreReason::MANUAL_REVIEW;
  throw std::invalid_argument("Unknown ValidatorScoreReason: " + value);
}

ValidatorScoreRecord::ValidatorScoreRecord()
    : m_validatorAddress(""), m_epoch(0), m_previousScore(0), m_newScore(0),
      m_reason(ValidatorScoreReason::UNKNOWN), m_evidenceHash(""),
      m_timestamp(0) {}

ValidatorScoreRecord::ValidatorScoreRecord(std::string validatorAddress,
                                           std::uint64_t epoch,
                                           std::int32_t previousScore,
                                           std::int32_t newScore,
                                           ValidatorScoreReason reason,
                                           std::string evidenceHash,
                                           std::int64_t timestamp)
    : m_validatorAddress(std::move(validatorAddress)), m_epoch(epoch),
      m_previousScore(previousScore), m_newScore(newScore), m_reason(reason),
      m_evidenceHash(std::move(evidenceHash)), m_timestamp(timestamp) {}

const std::string &ValidatorScoreRecord::validatorAddress() const {
  return m_validatorAddress;
}

std::uint64_t ValidatorScoreRecord::epoch() const { return m_epoch; }

std::int32_t ValidatorScoreRecord::previousScore() const {
  return m_previousScore;
}

std::int32_t ValidatorScoreRecord::newScore() const { return m_newScore; }

ValidatorScoreReason ValidatorScoreRecord::reason() const { return m_reason; }

const std::string &ValidatorScoreRecord::evidenceHash() const {
  return m_evidenceHash;
}

std::int64_t ValidatorScoreRecord::timestamp() const { return m_timestamp; }

std::int32_t ValidatorScoreRecord::scoreDelta() const {
  return m_newScore - m_previousScore;
}

std::uint32_t ValidatorScoreRecord::trustFactorBasisPoints() const {
  if (!isScoreInRange(m_newScore)) {
    return 0;
  }

  return static_cast<std::uint32_t>(m_newScore * 100);
}

bool ValidatorScoreRecord::increased() const {
  return m_newScore > m_previousScore;
}

bool ValidatorScoreRecord::decreased() const {
  return m_newScore < m_previousScore;
}

bool ValidatorScoreRecord::isValid() const {
  if (m_validatorAddress.empty()) {
    return false;
  }

  if (m_epoch == 0) {
    return false;
  }

  if (!isScoreInRange(m_previousScore)) {
    return false;
  }

  if (!isScoreInRange(m_newScore)) {
    return false;
  }

  if (m_reason == ValidatorScoreReason::UNKNOWN) {
    return false;
  }

  if (m_evidenceHash.empty()) {
    return false;
  }

  if (m_timestamp <= 0) {
    return false;
  }

  return true;
}

std::string ValidatorScoreRecord::serialize() const {
  std::ostringstream oss;

  oss << "ValidatorScoreRecord{"
      << "validator=" << m_validatorAddress << ";epoch=" << m_epoch
      << ";previousScore=" << m_previousScore << ";newScore=" << m_newScore
      << ";reason=" << validatorScoreReasonToString(m_reason)
      << ";evidenceHash=" << m_evidenceHash << ";timestamp=" << m_timestamp
      << "}";

  return oss.str();
}

ValidatorScoreRecord
ValidatorScoreRecord::deserialize(const std::string &serialized) {
  if (serialized.rfind("ValidatorScoreRecord{", 0) != 0) {
    throw std::invalid_argument(
        "Serialized data is not a ValidatorScoreRecord.");
  }

  const auto field = [&serialized](const std::string &name) {
    return serialization::FieldCodec::extractField(serialized, name);
  };
  ValidatorScoreRecord record(
      field("validator"),
      static_cast<std::uint64_t>(std::stoull(field("epoch"))),
      static_cast<std::int32_t>(std::stoi(field("previousScore"))),
      static_cast<std::int32_t>(std::stoi(field("newScore"))),
      validatorScoreReasonFromString(field("reason")), field("evidenceHash"),
      std::stoll(field("timestamp")));
  if (!record.isValid() || record.serialize() != serialized) {
    throw std::invalid_argument("Non-canonical ValidatorScoreRecord rejected.");
  }
  return record;
}

bool ValidatorScoreRecord::isScoreInRange(std::int32_t score) {
  return score >= MIN_SCORE && score <= MAX_SCORE;
}

} // namespace nodo::economics
