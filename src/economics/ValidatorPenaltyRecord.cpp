#include "economics/ValidatorPenaltyRecord.hpp"

#include "core/ValidatorProposalRegistry.hpp"
#include "crypto/hash.h"
#include "serialization/FieldCodec.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

namespace {

std::string hashString(const std::string &value) {
  char output[NODO_HASH_BUFFER_SIZE] = {0};
  nodo_hash_string(value.c_str(), output, sizeof(output));
  return std::string(output);
}

bool isSafeText(const std::string &value) {
  if (value.empty()) {
    return false;
  }

  for (const char character : value) {
    if (character == ';' || character == '{' || character == '}' ||
        character == '\n' || character == '\r' || character == '\t') {
      return false;
    }
  }

  return true;
}

} // namespace

std::string validatorPenaltyReasonToString(ValidatorPenaltyReason reason) {
  switch (reason) {
  case ValidatorPenaltyReason::DOUBLE_SIGN:
    return "DOUBLE_SIGN";
  case ValidatorPenaltyReason::INVALID_PROPOSAL:
    return "INVALID_PROPOSAL";
  case ValidatorPenaltyReason::INVALID_SIGNATURE:
    return "INVALID_SIGNATURE";
  case ValidatorPenaltyReason::MANUAL_REVIEW:
    return "MANUAL_REVIEW";
  default:
    return "UNKNOWN";
  }
}

ValidatorPenaltyReason
validatorPenaltyReasonFromString(const std::string &value) {
  if (value == "DOUBLE_SIGN") {
    return ValidatorPenaltyReason::DOUBLE_SIGN;
  }

  if (value == "INVALID_PROPOSAL") {
    return ValidatorPenaltyReason::INVALID_PROPOSAL;
  }

  if (value == "INVALID_SIGNATURE") {
    return ValidatorPenaltyReason::INVALID_SIGNATURE;
  }

  if (value == "MANUAL_REVIEW") {
    return ValidatorPenaltyReason::MANUAL_REVIEW;
  }

  throw std::invalid_argument("Unknown ValidatorPenaltyReason: " + value);
}

std::string validatorPenaltyActionToString(ValidatorPenaltyAction action) {
  switch (action) {
  case ValidatorPenaltyAction::SCORE_REDUCTION:
    return "SCORE_REDUCTION";
  case ValidatorPenaltyAction::SLASHING_REVIEW:
    return "SLASHING_REVIEW";
  case ValidatorPenaltyAction::SECURITY_LOCK_REVIEW:
    return "SECURITY_LOCK_REVIEW";
  default:
    return "UNKNOWN";
  }
}

ValidatorPenaltyAction
validatorPenaltyActionFromString(const std::string &value) {
  if (value == "SCORE_REDUCTION") {
    return ValidatorPenaltyAction::SCORE_REDUCTION;
  }

  if (value == "SLASHING_REVIEW") {
    return ValidatorPenaltyAction::SLASHING_REVIEW;
  }

  if (value == "SECURITY_LOCK_REVIEW") {
    return ValidatorPenaltyAction::SECURITY_LOCK_REVIEW;
  }

  throw std::invalid_argument("Unknown ValidatorPenaltyAction: " + value);
}

ValidatorPenaltyRecord::ValidatorPenaltyRecord()
    : m_validatorAddress(""), m_epoch(0), m_blockIndex(0), m_previousScore(0),
      m_newScore(0), m_reason(ValidatorPenaltyReason::UNKNOWN),
      m_action(ValidatorPenaltyAction::UNKNOWN), m_evidenceHash(""),
      m_firstBlockHash(""), m_conflictingBlockHash(""),
      m_firstSignatureDigest(""), m_conflictingSignatureDigest(""),
      m_timestamp(0) {}

ValidatorPenaltyRecord::ValidatorPenaltyRecord(
    std::string validatorAddress, std::uint64_t epoch, std::uint64_t blockIndex,
    std::int32_t previousScore, std::int32_t newScore,
    ValidatorPenaltyReason reason, ValidatorPenaltyAction action,
    std::string evidenceHash, std::string firstBlockHash,
    std::string conflictingBlockHash, std::string firstSignatureDigest,
    std::string conflictingSignatureDigest, std::int64_t timestamp)
    : m_validatorAddress(std::move(validatorAddress)), m_epoch(epoch),
      m_blockIndex(blockIndex), m_previousScore(previousScore),
      m_newScore(newScore), m_reason(reason), m_action(action),
      m_evidenceHash(std::move(evidenceHash)),
      m_firstBlockHash(std::move(firstBlockHash)),
      m_conflictingBlockHash(std::move(conflictingBlockHash)),
      m_firstSignatureDigest(std::move(firstSignatureDigest)),
      m_conflictingSignatureDigest(std::move(conflictingSignatureDigest)),
      m_timestamp(timestamp) {}

const std::string &ValidatorPenaltyRecord::validatorAddress() const {
  return m_validatorAddress;
}

std::uint64_t ValidatorPenaltyRecord::epoch() const { return m_epoch; }

std::uint64_t ValidatorPenaltyRecord::blockIndex() const {
  return m_blockIndex;
}

std::int32_t ValidatorPenaltyRecord::previousScore() const {
  return m_previousScore;
}

std::int32_t ValidatorPenaltyRecord::newScore() const { return m_newScore; }

ValidatorPenaltyReason ValidatorPenaltyRecord::reason() const {
  return m_reason;
}

ValidatorPenaltyAction ValidatorPenaltyRecord::action() const {
  return m_action;
}

const std::string &ValidatorPenaltyRecord::evidenceHash() const {
  return m_evidenceHash;
}

const std::string &ValidatorPenaltyRecord::firstBlockHash() const {
  return m_firstBlockHash;
}

const std::string &ValidatorPenaltyRecord::conflictingBlockHash() const {
  return m_conflictingBlockHash;
}

const std::string &ValidatorPenaltyRecord::firstSignatureDigest() const {
  return m_firstSignatureDigest;
}

const std::string &ValidatorPenaltyRecord::conflictingSignatureDigest() const {
  return m_conflictingSignatureDigest;
}

std::int64_t ValidatorPenaltyRecord::timestamp() const { return m_timestamp; }

std::int32_t ValidatorPenaltyRecord::scorePenalty() const {
  return m_previousScore - m_newScore;
}

bool ValidatorPenaltyRecord::isValid() const {
  if (!isSafeText(m_validatorAddress)) {
    return false;
  }

  if (m_epoch == 0) {
    return false;
  }

  if (!isScoreInRange(m_previousScore) || !isScoreInRange(m_newScore)) {
    return false;
  }

  if (m_reason == ValidatorPenaltyReason::UNKNOWN ||
      m_action == ValidatorPenaltyAction::UNKNOWN) {
    return false;
  }

  if (!isSafeText(m_evidenceHash) || !isSafeText(m_firstBlockHash) ||
      !isSafeText(m_conflictingBlockHash) ||
      !isSafeText(m_firstSignatureDigest) ||
      !isSafeText(m_conflictingSignatureDigest)) {
    return false;
  }

  if (m_firstBlockHash == m_conflictingBlockHash) {
    return false;
  }

  if (m_reason == ValidatorPenaltyReason::DOUBLE_SIGN &&
      m_newScore >= m_previousScore) {
    return false;
  }

  if (m_timestamp <= 0) {
    return false;
  }

  return true;
}

std::string ValidatorPenaltyRecord::deterministicId() const {
  if (!isValid()) {
    throw std::logic_error(
        "Invalid ValidatorPenaltyRecord has no deterministic id.");
  }

  return hashString("NODO_VALIDATOR_PENALTY_RECORD_V1|" + serialize());
}

ValidatorScoreRecord ValidatorPenaltyRecord::createScoreRecord() const {
  if (!isValid()) {
    throw std::logic_error(
        "Invalid ValidatorPenaltyRecord cannot create score record.");
  }

  ValidatorScoreRecord scoreRecord(m_validatorAddress, m_epoch, m_previousScore,
                                   m_newScore,
                                   ValidatorScoreReason::CONFLICTING_SIGNATURE,
                                   deterministicId(), m_timestamp);

  if (!scoreRecord.isValid()) {
    throw std::logic_error(
        "ValidatorPenaltyRecord created invalid ValidatorScoreRecord.");
  }

  return scoreRecord;
}

std::string ValidatorPenaltyRecord::serialize() const {
  std::ostringstream oss;

  oss << "ValidatorPenaltyRecord{"
      << "validator=" << m_validatorAddress << ";epoch=" << m_epoch
      << ";blockIndex=" << m_blockIndex << ";previousScore=" << m_previousScore
      << ";newScore=" << m_newScore
      << ";reason=" << validatorPenaltyReasonToString(m_reason)
      << ";action=" << validatorPenaltyActionToString(m_action)
      << ";evidenceHash=" << m_evidenceHash
      << ";firstBlockHash=" << m_firstBlockHash
      << ";conflictingBlockHash=" << m_conflictingBlockHash
      << ";firstSignatureDigest=" << m_firstSignatureDigest
      << ";conflictingSignatureDigest=" << m_conflictingSignatureDigest
      << ";timestamp=" << m_timestamp << "}";

  return oss.str();
}

ValidatorPenaltyRecord
ValidatorPenaltyRecord::deserialize(const std::string &serialized) {
  if (serialized.rfind("ValidatorPenaltyRecord{", 0) != 0) {
    throw std::invalid_argument(
        "Serialized data is not a ValidatorPenaltyRecord.");
  }

  ValidatorPenaltyRecord record(
      serialization::FieldCodec::extractField(serialized, "validator"),
      static_cast<std::uint64_t>(std::stoull(
          serialization::FieldCodec::extractField(serialized, "epoch"))),
      static_cast<std::uint64_t>(std::stoull(
          serialization::FieldCodec::extractField(serialized, "blockIndex"))),
      static_cast<std::int32_t>(
          std::stoi(serialization::FieldCodec::extractField(serialized,
                                                            "previousScore"))),
      static_cast<std::int32_t>(std::stoi(
          serialization::FieldCodec::extractField(serialized, "newScore"))),
      validatorPenaltyReasonFromString(
          serialization::FieldCodec::extractField(serialized, "reason")),
      validatorPenaltyActionFromString(
          serialization::FieldCodec::extractField(serialized, "action")),
      serialization::FieldCodec::extractField(serialized, "evidenceHash"),
      serialization::FieldCodec::extractField(serialized, "firstBlockHash"),
      serialization::FieldCodec::extractField(serialized,
                                              "conflictingBlockHash"),
      serialization::FieldCodec::extractField(serialized,
                                              "firstSignatureDigest"),
      serialization::FieldCodec::extractField(serialized,
                                              "conflictingSignatureDigest"),
      std::stoll(
          serialization::FieldCodec::extractField(serialized, "timestamp")));

  if (!record.isValid()) {
    throw std::invalid_argument(
        "Deserialized ValidatorPenaltyRecord is invalid.");
  }

  if (record.serialize() != serialized) {
    throw std::logic_error(
        "ValidatorPenaltyRecord round-trip serialization mismatch.");
  }

  return record;
}

bool ValidatorPenaltyRecord::isScoreInRange(std::int32_t score) {
  return score >= ValidatorScoreRecord::MIN_SCORE &&
         score <= ValidatorScoreRecord::MAX_SCORE;
}

ValidatorPenaltyPolicy ValidatorPenaltyPolicy::conservativeDefaultPolicy() {
  return ValidatorPenaltyPolicy(40);
}

ValidatorPenaltyPolicy::ValidatorPenaltyPolicy(
    std::int32_t doubleSignScorePenalty)
    : m_doubleSignScorePenalty(doubleSignScorePenalty) {}

std::int32_t ValidatorPenaltyPolicy::doubleSignScorePenalty() const {
  return m_doubleSignScorePenalty;
}

bool ValidatorPenaltyPolicy::isValid() const {
  return m_doubleSignScorePenalty > 0 &&
         m_doubleSignScorePenalty <= ValidatorScoreRecord::MAX_SCORE;
}

std::int32_t ValidatorPenaltyPolicy::applyDoubleSignPenalty(
    std::int32_t previousScore) const {
  if (!isValid()) {
    throw std::logic_error("Invalid ValidatorPenaltyPolicy.");
  }

  if (previousScore < ValidatorScoreRecord::MIN_SCORE ||
      previousScore > ValidatorScoreRecord::MAX_SCORE) {
    throw std::invalid_argument("Previous validator score is out of range.");
  }

  return std::max(ValidatorScoreRecord::MIN_SCORE,
                  previousScore - m_doubleSignScorePenalty);
}

ValidatorPenaltyRecord ValidatorPenaltyPolicy::createDoubleSignPenaltyRecord(
    const core::ValidatorDoubleSignEvidence &evidence, std::uint64_t epoch,
    std::int32_t previousScore, std::int64_t timestamp) const {
  if (!isValid()) {
    throw std::logic_error("Invalid ValidatorPenaltyPolicy.");
  }

  if (!evidence.isValid()) {
    throw std::invalid_argument(
        "Invalid double-sign evidence rejected by penalty policy.");
  }

  if (epoch == 0) {
    throw std::invalid_argument("Penalty epoch must be positive.");
  }

  if (timestamp <= 0) {
    throw std::invalid_argument("Penalty timestamp must be positive.");
  }

  const std::int32_t newScore = applyDoubleSignPenalty(previousScore);

  ValidatorPenaltyRecord record(
      evidence.validatorAddress(), epoch, evidence.blockIndex(), previousScore,
      newScore, ValidatorPenaltyReason::DOUBLE_SIGN,
      ValidatorPenaltyAction::SCORE_REDUCTION,
      hashString("NODO_DOUBLE_SIGN_EVIDENCE_V1|" + evidence.serialize()),
      evidence.firstProposal().blockHash(),
      evidence.conflictingProposal().blockHash(),
      evidence.firstProposal().signatureDigest(),
      evidence.conflictingProposal().signatureDigest(), timestamp);

  if (!record.isValid()) {
    throw std::logic_error(
        "Penalty policy produced invalid ValidatorPenaltyRecord.");
  }

  return record;
}

std::string ValidatorPenaltyPolicy::serialize() const {
  std::ostringstream oss;

  oss << "ValidatorPenaltyPolicy{"
      << "doubleSignScorePenalty=" << m_doubleSignScorePenalty << "}";

  return oss.str();
}

} // namespace nodo::economics
