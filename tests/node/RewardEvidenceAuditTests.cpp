#include "node/ProtectionRewards.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/RewardCategory.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::node::ProtectionRewards;
using nodo::node::ProtectionRewardSettlement;
using nodo::node::RewardCategory;
using nodo::node::RewardEvidenceAuditResult;
using nodo::utils::Amount;

ProtectionRewardSettlement
validSettlement(const std::string &addr = "validator-001",
                std::uint64_t block = 10, std::int64_t planned = 1000,
                std::int64_t earned = 800, std::int64_t deferred = 200,
                const std::string &workDigest = "work-digest-abc") {
  return ProtectionRewardSettlement(
      addr, block, Amount::fromRawUnits(planned), Amount::fromRawUnits(earned),
      Amount::fromRawUnits(deferred), 500, 700,
      ProtectionRewards::PROTECTION_SETTLEMENT_REASON, "grant-digest-001",
      workDigest);
}

// Test 15: Protection reward without evidence fails audit.
void testSettlementWithoutWorkDigestFailsAudit() {
  const ProtectionRewardSettlement noEvidence = ProtectionRewardSettlement(
      "validator-001", 10, Amount::fromRawUnits(1000),
      Amount::fromRawUnits(1000), Amount::fromRawUnits(0), 500, 700,
      ProtectionRewards::PROTECTION_SETTLEMENT_REASON, "grant-digest-001",
      "" // empty sourceWorkDigest — no evidence
  );
  // isValid() returns false when sourceWorkDigest is empty.
  assert(!noEvidence.isValid());

  const RewardEvidenceAuditResult result =
      ProtectionRewards::auditSettlementEvidence({noEvidence});
  assert(!result.isPassed());
  assert(!result.reason().empty());
}

// Test 15: Protection reward with evidence passes audit.
void testSettlementWithWorkDigestPassesAudit() {
  const ProtectionRewardSettlement withEvidence = validSettlement();
  assert(withEvidence.isValid());

  const RewardEvidenceAuditResult result =
      ProtectionRewards::auditSettlementEvidence({withEvidence});
  assert(result.isPassed());
}

// Empty settlement list always passes.
void testEmptySettlementsPassAudit() {
  const RewardEvidenceAuditResult result =
      ProtectionRewards::auditSettlementEvidence({});
  assert(result.isPassed());
}

// categoryForSettlement: PROTECTION when earned > 0 and deferred == 0.
void testCategoryProtectionWhenEarnedOnly() {
  const ProtectionRewardSettlement s = validSettlement("v1", 10, 1000, 1000, 0);
  assert(ProtectionRewards::categoryForSettlement(s) ==
         RewardCategory::PROTECTION);
}

// categoryForSettlement: DEFERRED_PROTECTION when deferred > 0.
void testCategoryDeferredWhenDeferredPositive() {
  const ProtectionRewardSettlement s =
      validSettlement("v1", 10, 1000, 800, 200);
  assert(ProtectionRewards::categoryForSettlement(s) ==
         RewardCategory::DEFERRED_PROTECTION);
}

// Test 17: Deferred reward is not double-counted.
// The settlement splits planned into earned + deferred. auditSettlementEvidence
// checks settlement validity which ensures earned + deferred == planned.
void testDeferredNotCountedTwice() {
  const ProtectionRewardSettlement s =
      validSettlement("v1", 10, 1000, 600, 400);
  assert(s.isValid());
  assert(s.earnedReward().rawUnits() + s.deferredReward().rawUnits() ==
         s.plannedReward().rawUnits());

  const RewardEvidenceAuditResult result =
      ProtectionRewards::auditSettlementEvidence({s});
  assert(result.isPassed());
}

// Multiple valid settlements all pass audit.
void testMultipleValidSettlementsPassAudit() {
  const std::vector<ProtectionRewardSettlement> settlements = {
      validSettlement("v1", 10, 500, 500, 0, "digest-v1"),
      validSettlement("v2", 10, 600, 400, 200, "digest-v2"),
      validSettlement("v3", 10, 700, 700, 0, "digest-v3")};
  const RewardEvidenceAuditResult result =
      ProtectionRewards::auditSettlementEvidence(settlements);
  assert(result.isPassed());
}

} // namespace

int main() {
  testSettlementWithoutWorkDigestFailsAudit();
  testSettlementWithWorkDigestPassesAudit();
  testEmptySettlementsPassAudit();
  testCategoryProtectionWhenEarnedOnly();
  testCategoryDeferredWhenDeferredPositive();
  testDeferredNotCountedTwice();
  testMultipleValidSettlementsPassAudit();
  return 0;
}
