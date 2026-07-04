#include "node/FeeEconomics.hpp"

#include "node/MonetaryFirewall.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::node::FeeBurnRecord;
using nodo::node::FeeEconomicBalance;
using nodo::node::FeeEconomics;
using nodo::node::FeeSplitPolicy;
using nodo::node::TreasuryFeeRecord;
using nodo::utils::Amount;

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testDefaultFeeSplitPolicy() {
  const FeeSplitPolicy policy = FeeSplitPolicy::protocolDefault();

  requireCondition(policy.isValid() &&
                       policy.validatorRewardBasisPoints() == 5000 &&
                       policy.treasuryBasisPoints() == 3000 &&
                       policy.burnBasisPoints() == 2000,
                   "Default fee split policy should allocate 50/30/20.");
}

void testBuildsFeeEconomicBalance() {
  const FeeEconomicBalance balance =
      FeeEconomics::buildFeeEconomicBalance(1, Amount::fromRawUnits(100));

  requireCondition(balance.active() && balance.totalFee().rawUnits() == 100 &&
                       balance.validatorRewardAmount().rawUnits() == 50 &&
                       balance.treasuryAmount().rawUnits() == 30 &&
                       balance.burnAmount().rawUnits() == 20,
                   "Fee economic balance should split fees into validator "
                   "reward, treasury and burn.");
}

void testBuildsBurnAndTreasuryRecords() {
  const FeeEconomicBalance balance =
      FeeEconomics::buildFeeEconomicBalance(1, Amount::fromRawUnits(100));

  const FeeBurnRecord burn =
      FeeEconomics::buildFeeBurnRecord(balance, Amount::fromRawUnits(1000));

  const TreasuryFeeRecord treasury =
      FeeEconomics::buildTreasuryFeeRecord(balance);

  requireCondition(
      burn.active() && burn.burnAmount().rawUnits() == 20 &&
          burn.supplyBefore().rawUnits() == 1000 &&
          burn.supplyAfter().rawUnits() == 980,
      "Fee burn record should reduce audited supply by the burned amount.");

  requireCondition(treasury.active() &&
                       treasury.treasuryAmount().rawUnits() == 30 &&
                       treasury.treasuryAddress() ==
                           nodo::node::ProtectionTreasury::TREASURY_ADDRESS,
                   "Treasury fee record should allocate the treasury share to "
                   "the protocol treasury.");
}

void testZeroFeesRemainValid() {
  const FeeEconomicBalance balance =
      FeeEconomics::buildFeeEconomicBalance(1, Amount::fromRawUnits(0));

  requireCondition(
      balance.active() && balance.validatorRewardAmount().rawUnits() == 0 &&
          balance.treasuryAmount().rawUnits() == 0 &&
          balance.burnAmount().rawUnits() == 0,
      "Zero-fee blocks should still produce an active zero-value fee balance.");
}

} // namespace

int main() {
  try {
    testDefaultFeeSplitPolicy();
    testBuildsFeeEconomicBalance();
    testBuildsBurnAndTreasuryRecords();
    testZeroFeesRemainValid();

    std::cout << "Nodo fee economics tests passed.\\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo fee economics tests failed: " << error.what() << "\\n";
    return 1;
  }
}
