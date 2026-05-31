#include "node/FeeEconomics.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

utils::Amount basisPointShare(
    utils::Amount total,
    std::uint32_t basisPoints
) {
    if (total.isNegative()) {
        throw std::invalid_argument("Cannot split a negative fee amount.");
    }

    if (basisPoints == 0 || total.isZero()) {
        return utils::Amount();
    }

    const std::int64_t raw =
        total.rawUnits();

    const std::int64_t whole =
        raw / 10000;

    const std::int64_t remainder =
        raw % 10000;

    if (whole > std::numeric_limits<std::int64_t>::max() / static_cast<std::int64_t>(basisPoints)) {
        throw std::overflow_error("Fee split overflow.");
    }

    const std::int64_t wholePart =
        whole * static_cast<std::int64_t>(basisPoints);

    const std::int64_t remainderPart =
        (remainder * static_cast<std::int64_t>(basisPoints)) / 10000;

    if (wholePart > std::numeric_limits<std::int64_t>::max() - remainderPart) {
        throw std::overflow_error("Fee split overflow.");
    }

    return utils::Amount::fromRawUnits(
        wholePart + remainderPart
    );
}

} // namespace

FeeSplitPolicy::FeeSplitPolicy()
    : m_validatorRewardBasisPoints(0),
      m_treasuryBasisPoints(0),
      m_burnBasisPoints(0),
      m_policyId(""),
      m_reason("") {}

FeeSplitPolicy::FeeSplitPolicy(
    std::uint32_t validatorRewardBasisPoints,
    std::uint32_t treasuryBasisPoints,
    std::uint32_t burnBasisPoints,
    std::string policyId,
    std::string reason
)
    : m_validatorRewardBasisPoints(validatorRewardBasisPoints),
      m_treasuryBasisPoints(treasuryBasisPoints),
      m_burnBasisPoints(burnBasisPoints),
      m_policyId(std::move(policyId)),
      m_reason(std::move(reason)) {}

std::uint32_t FeeSplitPolicy::validatorRewardBasisPoints() const {
    return m_validatorRewardBasisPoints;
}

std::uint32_t FeeSplitPolicy::treasuryBasisPoints() const {
    return m_treasuryBasisPoints;
}

std::uint32_t FeeSplitPolicy::burnBasisPoints() const {
    return m_burnBasisPoints;
}

const std::string& FeeSplitPolicy::policyId() const {
    return m_policyId;
}

const std::string& FeeSplitPolicy::reason() const {
    return m_reason;
}

bool FeeSplitPolicy::isValid() const {
    return !m_policyId.empty() &&
           m_reason == FeeEconomics::FEE_SPLIT_POLICY_REASON &&
           m_validatorRewardBasisPoints +
               m_treasuryBasisPoints +
               m_burnBasisPoints == 10000;
}

std::string FeeSplitPolicy::deterministicId() const {
    return serialize();
}

std::string FeeSplitPolicy::serialize() const {
    std::ostringstream oss;

    oss << "FeeSplitPolicy{"
        << "validatorRewardBasisPoints=" << m_validatorRewardBasisPoints
        << ";treasuryBasisPoints=" << m_treasuryBasisPoints
        << ";burnBasisPoints=" << m_burnBasisPoints
        << ";policyId=" << m_policyId
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

FeeSplitPolicy FeeSplitPolicy::protocolDefault() {
    return FeeSplitPolicy(
        NODO_FEE_VALIDATOR_REWARD_BASIS_POINTS,
        NODO_FEE_TREASURY_BASIS_POINTS,
        NODO_FEE_BURN_BASIS_POINTS,
        "NODO_FEE_SPLIT_POLICY_V1",
        FeeEconomics::FEE_SPLIT_POLICY_REASON
    );
}

FeeEconomicBalance::FeeEconomicBalance()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_totalFee(),
      m_validatorRewardAmount(),
      m_treasuryAmount(),
      m_burnAmount(),
      m_policyId(""),
      m_reason(FeeEconomics::NOT_EVALUATED_REASON) {}

FeeEconomicBalance::FeeEconomicBalance(
    std::string status,
    std::uint64_t blockHeight,
    utils::Amount totalFee,
    utils::Amount validatorRewardAmount,
    utils::Amount treasuryAmount,
    utils::Amount burnAmount,
    std::string policyId,
    std::string reason
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_totalFee(totalFee),
      m_validatorRewardAmount(validatorRewardAmount),
      m_treasuryAmount(treasuryAmount),
      m_burnAmount(burnAmount),
      m_policyId(std::move(policyId)),
      m_reason(std::move(reason)) {}

FeeEconomicBalance FeeEconomicBalance::notEvaluated() {
    return FeeEconomicBalance();
}

const std::string& FeeEconomicBalance::status() const {
    return m_status;
}

std::uint64_t FeeEconomicBalance::blockHeight() const {
    return m_blockHeight;
}

utils::Amount FeeEconomicBalance::totalFee() const {
    return m_totalFee;
}

utils::Amount FeeEconomicBalance::validatorRewardAmount() const {
    return m_validatorRewardAmount;
}

utils::Amount FeeEconomicBalance::treasuryAmount() const {
    return m_treasuryAmount;
}

utils::Amount FeeEconomicBalance::burnAmount() const {
    return m_burnAmount;
}

const std::string& FeeEconomicBalance::policyId() const {
    return m_policyId;
}

const std::string& FeeEconomicBalance::reason() const {
    return m_reason;
}

bool FeeEconomicBalance::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool FeeEconomicBalance::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_blockHeight == 0 &&
               m_totalFee.isZero() &&
               m_validatorRewardAmount.isZero() &&
               m_treasuryAmount.isZero() &&
               m_burnAmount.isZero() &&
               m_policyId.empty() &&
               m_reason == FeeEconomics::NOT_EVALUATED_REASON;
    }

    if (m_status != "ACTIVE" ||
        m_blockHeight == 0 ||
        m_totalFee.isNegative() ||
        m_validatorRewardAmount.isNegative() ||
        m_treasuryAmount.isNegative() ||
        m_burnAmount.isNegative() ||
        m_validatorRewardAmount + m_treasuryAmount + m_burnAmount != m_totalFee ||
        m_policyId != FeeSplitPolicy::protocolDefault().deterministicId() ||
        m_reason != FeeEconomics::FEE_BALANCE_REASON) {
        return false;
    }

    return true;
}

std::string FeeEconomicBalance::serialize() const {
    std::ostringstream oss;

    oss << "FeeEconomicBalance{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";totalFeeRawUnits=" << m_totalFee.rawUnits()
        << ";validatorRewardRawUnits=" << m_validatorRewardAmount.rawUnits()
        << ";treasuryRawUnits=" << m_treasuryAmount.rawUnits()
        << ";burnRawUnits=" << m_burnAmount.rawUnits()
        << ";policyId=" << m_policyId
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

FeeBurnRecord::FeeBurnRecord()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_burnAmount(),
      m_supplyBefore(),
      m_supplyAfter(),
      m_reason(FeeEconomics::NOT_EVALUATED_REASON),
      m_sourceFeeBalanceDigest("") {}

FeeBurnRecord::FeeBurnRecord(
    std::string status,
    std::uint64_t blockHeight,
    utils::Amount burnAmount,
    utils::Amount supplyBefore,
    utils::Amount supplyAfter,
    std::string reason,
    std::string sourceFeeBalanceDigest
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_burnAmount(burnAmount),
      m_supplyBefore(supplyBefore),
      m_supplyAfter(supplyAfter),
      m_reason(std::move(reason)),
      m_sourceFeeBalanceDigest(std::move(sourceFeeBalanceDigest)) {}

FeeBurnRecord FeeBurnRecord::notEvaluated() {
    return FeeBurnRecord();
}

const std::string& FeeBurnRecord::status() const {
    return m_status;
}

std::uint64_t FeeBurnRecord::blockHeight() const {
    return m_blockHeight;
}

utils::Amount FeeBurnRecord::burnAmount() const {
    return m_burnAmount;
}

utils::Amount FeeBurnRecord::supplyBefore() const {
    return m_supplyBefore;
}

utils::Amount FeeBurnRecord::supplyAfter() const {
    return m_supplyAfter;
}

const std::string& FeeBurnRecord::reason() const {
    return m_reason;
}

const std::string& FeeBurnRecord::sourceFeeBalanceDigest() const {
    return m_sourceFeeBalanceDigest;
}

bool FeeBurnRecord::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool FeeBurnRecord::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_reason == FeeEconomics::NOT_EVALUATED_REASON;
    }

    if (m_status != "ACTIVE" ||
        m_blockHeight == 0 ||
        m_burnAmount.isNegative() ||
        m_supplyBefore.isNegative() ||
        m_supplyAfter.isNegative() ||
        m_supplyAfter != m_supplyBefore - m_burnAmount ||
        m_reason != FeeEconomics::FEE_BURN_REASON ||
        m_sourceFeeBalanceDigest.empty()) {
        return false;
    }

    return true;
}

std::string FeeBurnRecord::serialize() const {
    std::ostringstream oss;

    oss << "FeeBurnRecord{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";burnAmountRawUnits=" << m_burnAmount.rawUnits()
        << ";supplyBeforeRawUnits=" << m_supplyBefore.rawUnits()
        << ";supplyAfterRawUnits=" << m_supplyAfter.rawUnits()
        << ";reason=" << m_reason
        << ";sourceFeeBalanceDigest=" << m_sourceFeeBalanceDigest
        << "}";

    return oss.str();
}

TreasuryFeeRecord::TreasuryFeeRecord()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_treasuryAddress(""),
      m_treasuryAmount(),
      m_reason(FeeEconomics::NOT_EVALUATED_REASON),
      m_sourceFeeBalanceDigest("") {}

TreasuryFeeRecord::TreasuryFeeRecord(
    std::string status,
    std::uint64_t blockHeight,
    std::string treasuryAddress,
    utils::Amount treasuryAmount,
    std::string reason,
    std::string sourceFeeBalanceDigest
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_treasuryAddress(std::move(treasuryAddress)),
      m_treasuryAmount(treasuryAmount),
      m_reason(std::move(reason)),
      m_sourceFeeBalanceDigest(std::move(sourceFeeBalanceDigest)) {}

TreasuryFeeRecord TreasuryFeeRecord::notEvaluated() {
    return TreasuryFeeRecord();
}

const std::string& TreasuryFeeRecord::status() const {
    return m_status;
}

std::uint64_t TreasuryFeeRecord::blockHeight() const {
    return m_blockHeight;
}

const std::string& TreasuryFeeRecord::treasuryAddress() const {
    return m_treasuryAddress;
}

utils::Amount TreasuryFeeRecord::treasuryAmount() const {
    return m_treasuryAmount;
}

const std::string& TreasuryFeeRecord::reason() const {
    return m_reason;
}

const std::string& TreasuryFeeRecord::sourceFeeBalanceDigest() const {
    return m_sourceFeeBalanceDigest;
}

bool TreasuryFeeRecord::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool TreasuryFeeRecord::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_reason == FeeEconomics::NOT_EVALUATED_REASON;
    }

    if (m_status != "ACTIVE" ||
        m_blockHeight == 0 ||
        m_treasuryAddress != ProtectionTreasury::TREASURY_ADDRESS ||
        m_treasuryAmount.isNegative() ||
        m_reason != FeeEconomics::TREASURY_FEE_REASON ||
        m_sourceFeeBalanceDigest.empty()) {
        return false;
    }

    return true;
}

std::string TreasuryFeeRecord::serialize() const {
    std::ostringstream oss;

    oss << "TreasuryFeeRecord{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";treasuryAddress=" << m_treasuryAddress
        << ";treasuryAmountRawUnits=" << m_treasuryAmount.rawUnits()
        << ";reason=" << m_reason
        << ";sourceFeeBalanceDigest=" << m_sourceFeeBalanceDigest
        << "}";

    return oss.str();
}

FeeEconomicBalance FeeEconomics::buildFeeEconomicBalance(
    std::uint64_t blockHeight,
    utils::Amount totalFee
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build fee economic balance at genesis height.");
    }

    if (totalFee.isNegative()) {
        throw std::invalid_argument("Cannot build fee economic balance from negative total fee.");
    }

    const FeeSplitPolicy policy =
        FeeSplitPolicy::protocolDefault();

    if (!policy.isValid()) {
        throw std::invalid_argument("Default fee split policy is invalid.");
    }

    const utils::Amount validatorReward =
        basisPointShare(
            totalFee,
            policy.validatorRewardBasisPoints()
        );

    const utils::Amount treasuryAmount =
        basisPointShare(
            totalFee,
            policy.treasuryBasisPoints()
        );

    const utils::Amount burnAmount =
        totalFee - validatorReward - treasuryAmount;

    return FeeEconomicBalance(
        "ACTIVE",
        blockHeight,
        totalFee,
        validatorReward,
        treasuryAmount,
        burnAmount,
        policy.deterministicId(),
        FEE_BALANCE_REASON
    );
}

FeeBurnRecord FeeEconomics::buildFeeBurnRecord(
    const FeeEconomicBalance& feeBalance,
    utils::Amount supplyBefore
) {
    if (!feeBalance.active() ||
        supplyBefore.isNegative() ||
        feeBalance.burnAmount() > supplyBefore) {
        throw std::invalid_argument("Cannot build fee burn record from invalid fee balance or supply.");
    }

    return FeeBurnRecord(
        "ACTIVE",
        feeBalance.blockHeight(),
        feeBalance.burnAmount(),
        supplyBefore,
        supplyBefore - feeBalance.burnAmount(),
        FEE_BURN_REASON,
        feeBalance.serialize()
    );
}

TreasuryFeeRecord FeeEconomics::buildTreasuryFeeRecord(
    const FeeEconomicBalance& feeBalance
) {
    if (!feeBalance.active()) {
        throw std::invalid_argument("Cannot build treasury fee record from inactive fee balance.");
    }

    return TreasuryFeeRecord(
        "ACTIVE",
        feeBalance.blockHeight(),
        ProtectionTreasury::TREASURY_ADDRESS,
        feeBalance.treasuryAmount(),
        TREASURY_FEE_REASON,
        feeBalance.serialize()
    );
}

bool FeeEconomics::sameBalance(
    const FeeEconomicBalance& left,
    const FeeEconomicBalance& right
) {
    return left.serialize() == right.serialize();
}

bool FeeEconomics::sameBurn(
    const FeeBurnRecord& left,
    const FeeBurnRecord& right
) {
    return left.serialize() == right.serialize();
}

bool FeeEconomics::sameTreasuryFee(
    const TreasuryFeeRecord& left,
    const TreasuryFeeRecord& right
) {
    return left.serialize() == right.serialize();
}

} // namespace nodo::node
