#ifndef NODO_NODE_FEE_ECONOMICS_HPP
#define NODO_NODE_FEE_ECONOMICS_HPP

#include "node/ProtectionTreasury.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

constexpr std::uint32_t NODO_FEE_VALIDATOR_REWARD_BASIS_POINTS = 5000;
constexpr std::uint32_t NODO_FEE_TREASURY_BASIS_POINTS = 3000;
constexpr std::uint32_t NODO_FEE_BURN_BASIS_POINTS = 2000;

class FeeSplitPolicy {
public:
  FeeSplitPolicy();

  FeeSplitPolicy(std::uint32_t validatorRewardBasisPoints,
                 std::uint32_t treasuryBasisPoints,
                 std::uint32_t burnBasisPoints, std::string policyId,
                 std::string reason);

  std::uint32_t validatorRewardBasisPoints() const;
  std::uint32_t treasuryBasisPoints() const;
  std::uint32_t burnBasisPoints() const;
  const std::string &policyId() const;
  const std::string &reason() const;

  bool isValid() const;
  std::string deterministicId() const;
  std::string serialize() const;

  static FeeSplitPolicy protocolDefault();

private:
  std::uint32_t m_validatorRewardBasisPoints;
  std::uint32_t m_treasuryBasisPoints;
  std::uint32_t m_burnBasisPoints;
  std::string m_policyId;
  std::string m_reason;
};

class FeeEconomicBalance {
public:
  FeeEconomicBalance();

  FeeEconomicBalance(std::string status, std::uint64_t blockHeight,
                     utils::Amount totalFee,
                     utils::Amount validatorRewardAmount,
                     utils::Amount treasuryAmount, utils::Amount burnAmount,
                     std::string policyId, std::string reason);

  static FeeEconomicBalance notEvaluated();

  const std::string &status() const;
  std::uint64_t blockHeight() const;
  utils::Amount totalFee() const;
  utils::Amount validatorRewardAmount() const;
  utils::Amount treasuryAmount() const;
  utils::Amount burnAmount() const;
  const std::string &policyId() const;
  const std::string &reason() const;

  bool active() const;
  bool isValid() const;
  std::string serialize() const;

private:
  std::string m_status;
  std::uint64_t m_blockHeight;
  utils::Amount m_totalFee;
  utils::Amount m_validatorRewardAmount;
  utils::Amount m_treasuryAmount;
  utils::Amount m_burnAmount;
  std::string m_policyId;
  std::string m_reason;
};

class FeeBurnRecord {
public:
  FeeBurnRecord();

  FeeBurnRecord(std::string status, std::uint64_t blockHeight,
                utils::Amount burnAmount, utils::Amount supplyBefore,
                utils::Amount supplyAfter, std::string reason,
                std::string sourceFeeBalanceDigest);

  static FeeBurnRecord notEvaluated();

  const std::string &status() const;
  std::uint64_t blockHeight() const;
  utils::Amount burnAmount() const;
  utils::Amount supplyBefore() const;
  utils::Amount supplyAfter() const;
  const std::string &reason() const;
  const std::string &sourceFeeBalanceDigest() const;

  bool active() const;
  bool isValid() const;
  std::string serialize() const;

private:
  std::string m_status;
  std::uint64_t m_blockHeight;
  utils::Amount m_burnAmount;
  utils::Amount m_supplyBefore;
  utils::Amount m_supplyAfter;
  std::string m_reason;
  std::string m_sourceFeeBalanceDigest;
};

class TreasuryFeeRecord {
public:
  TreasuryFeeRecord();

  TreasuryFeeRecord(std::string status, std::uint64_t blockHeight,
                    std::string treasuryAddress, utils::Amount treasuryAmount,
                    std::string reason, std::string sourceFeeBalanceDigest);

  static TreasuryFeeRecord notEvaluated();

  const std::string &status() const;
  std::uint64_t blockHeight() const;
  const std::string &treasuryAddress() const;
  utils::Amount treasuryAmount() const;
  const std::string &reason() const;
  const std::string &sourceFeeBalanceDigest() const;

  bool active() const;
  bool isValid() const;
  std::string serialize() const;

private:
  std::string m_status;
  std::uint64_t m_blockHeight;
  std::string m_treasuryAddress;
  utils::Amount m_treasuryAmount;
  std::string m_reason;
  std::string m_sourceFeeBalanceDigest;
};

class FeeEconomics {
public:
  static constexpr const char *FEE_SPLIT_POLICY_REASON = "FEE_SPLIT_POLICY_V1";

  static constexpr const char *FEE_BALANCE_REASON = "FEE_ECONOMIC_BALANCE";

  static constexpr const char *FEE_BURN_REASON = "PARTIAL_FEE_BURN";

  static constexpr const char *TREASURY_FEE_REASON = "TREASURY_FEE_ALLOCATION";

  static constexpr const char *NOT_EVALUATED_REASON =
      "FEE_ECONOMICS_NOT_EVALUATED";

  static FeeEconomicBalance buildFeeEconomicBalance(std::uint64_t blockHeight,
                                                    utils::Amount totalFee);

  static FeeBurnRecord buildFeeBurnRecord(const FeeEconomicBalance &feeBalance,
                                          utils::Amount supplyBefore);

  static TreasuryFeeRecord
  buildTreasuryFeeRecord(const FeeEconomicBalance &feeBalance);

  static bool sameBalance(const FeeEconomicBalance &left,
                          const FeeEconomicBalance &right);

  static bool sameBurn(const FeeBurnRecord &left, const FeeBurnRecord &right);

  static bool sameTreasuryFee(const TreasuryFeeRecord &left,
                              const TreasuryFeeRecord &right);
};

} // namespace nodo::node

#endif
