#ifndef NODO_NODE_CRYPTOGRAPHIC_SLASHING_HPP
#define NODO_NODE_CRYPTOGRAPHIC_SLASHING_HPP

#include "consensus/ValidatorVoteRecord.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "node/LockedStakePosition.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class CryptographicSlashingEvidenceRecord {
public:
  CryptographicSlashingEvidenceRecord();

  CryptographicSlashingEvidenceRecord(
      std::string validatorAddress, std::uint64_t blockHeight,
      std::uint64_t round, std::string evidenceType,
      std::uint16_t severityScore, std::uint32_t penaltyBasisPoints,
      std::string firstVoteDigest, std::string secondVoteDigest,
      std::string reason, std::string sourceEvidenceDigest);

  const std::string &validatorAddress() const;
  std::uint64_t blockHeight() const;
  std::uint64_t round() const;
  const std::string &evidenceType() const;
  std::uint16_t severityScore() const;
  std::uint32_t penaltyBasisPoints() const;
  const std::string &firstVoteDigest() const;
  const std::string &secondVoteDigest() const;
  const std::string &reason() const;
  const std::string &sourceEvidenceDigest() const;

  bool isValid() const;
  std::string serialize() const;

private:
  std::string m_validatorAddress;
  std::uint64_t m_blockHeight;
  std::uint64_t m_round;
  std::string m_evidenceType;
  std::uint16_t m_severityScore;
  std::uint32_t m_penaltyBasisPoints;
  std::string m_firstVoteDigest;
  std::string m_secondVoteDigest;
  std::string m_reason;
  std::string m_sourceEvidenceDigest;
};

class StakePenaltyRecord {
public:
  StakePenaltyRecord();

  StakePenaltyRecord(std::string validatorAddress, std::uint64_t blockHeight,
                     utils::Amount lockedStakeBefore,
                     utils::Amount penaltyAmount,
                     utils::Amount lockedStakeAfter,
                     std::uint64_t evidenceCount, std::string reason,
                     std::string sourceEvidenceDigest);

  const std::string &validatorAddress() const;
  std::uint64_t blockHeight() const;
  utils::Amount lockedStakeBefore() const;
  utils::Amount penaltyAmount() const;
  utils::Amount lockedStakeAfter() const;
  std::uint64_t evidenceCount() const;
  const std::string &reason() const;
  const std::string &sourceEvidenceDigest() const;

  bool isValid() const;
  std::string serialize() const;

private:
  std::string m_validatorAddress;
  std::uint64_t m_blockHeight;
  utils::Amount m_lockedStakeBefore;
  utils::Amount m_penaltyAmount;
  utils::Amount m_lockedStakeAfter;
  std::uint64_t m_evidenceCount;
  std::string m_reason;
  std::string m_sourceEvidenceDigest;
};

class CryptographicSlashingSummary {
public:
  CryptographicSlashingSummary();

  CryptographicSlashingSummary(std::string status, std::uint64_t blockHeight,
                               std::uint64_t evidenceCount,
                               std::uint64_t slashableEvidenceCount,
                               std::uint16_t maxSeverityScore,
                               utils::Amount penaltyTotal, std::string reason,
                               std::string sourcePenaltyDigest);

  static CryptographicSlashingSummary notEvaluated();

  const std::string &status() const;
  std::uint64_t blockHeight() const;
  std::uint64_t evidenceCount() const;
  std::uint64_t slashableEvidenceCount() const;
  std::uint16_t maxSeverityScore() const;
  utils::Amount penaltyTotal() const;
  const std::string &reason() const;
  const std::string &sourcePenaltyDigest() const;

  bool active() const;
  bool isValid() const;
  std::string serialize() const;

private:
  std::string m_status;
  std::uint64_t m_blockHeight;
  std::uint64_t m_evidenceCount;
  std::uint64_t m_slashableEvidenceCount;
  std::uint16_t m_maxSeverityScore;
  utils::Amount m_penaltyTotal;
  std::string m_reason;
  std::string m_sourcePenaltyDigest;
};

class CryptographicSlashing {
public:
  static constexpr const char *DOUBLE_VOTE_EVIDENCE_TYPE =
      "DOUBLE_SIGNED_VALIDATOR_VOTE";

  static constexpr const char *DOUBLE_VOTE_REASON =
      "CRYPTOGRAPHIC_DOUBLE_VOTE_EVIDENCE";

  static constexpr const char *STAKE_PENALTY_REASON =
      "CRYPTOGRAPHIC_SLASHING_STAKE_PENALTY";

  static constexpr const char *SLASHING_SUMMARY_REASON =
      "CRYPTOGRAPHIC_SLASHING_SUMMARY";

  static constexpr const char *NOT_EVALUATED_REASON =
      "CRYPTOGRAPHIC_SLASHING_NOT_EVALUATED";

  static constexpr std::uint16_t DOUBLE_VOTE_SEVERITY_SCORE = 1000;
  static constexpr std::uint32_t DOUBLE_VOTE_PENALTY_BASIS_POINTS = 1000;

  static std::vector<CryptographicSlashingEvidenceRecord>
  buildEvidenceRecords(const std::vector<consensus::ValidatorVoteRecord> &votes,
                       const crypto::CryptoPolicy &policy,
                       const crypto::SignatureProvider &provider);

  static std::vector<CryptographicSlashingEvidenceRecord>
  buildEvidenceRecordsFromCertifiedVotes(
      const std::vector<consensus::ValidatorVoteRecord> &votes);

  static std::vector<StakePenaltyRecord> buildStakePenaltyRecords(
      const std::vector<CryptographicSlashingEvidenceRecord> &evidenceRecords,
      const std::vector<LockedStakePosition> &lockedStakePositions);

  static CryptographicSlashingSummary buildSummary(
      std::uint64_t blockHeight,
      const std::vector<CryptographicSlashingEvidenceRecord> &evidenceRecords,
      const std::vector<StakePenaltyRecord> &penaltyRecords);

  static bool sameEvidenceRecords(
      const std::vector<CryptographicSlashingEvidenceRecord> &left,
      const std::vector<CryptographicSlashingEvidenceRecord> &right);

  static bool
  sameStakePenaltyRecords(const std::vector<StakePenaltyRecord> &left,
                          const std::vector<StakePenaltyRecord> &right);

  static bool sameSummary(const CryptographicSlashingSummary &left,
                          const CryptographicSlashingSummary &right);
};

} // namespace nodo::node

#endif
