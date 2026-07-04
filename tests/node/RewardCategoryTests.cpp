#include "node/RewardCategory.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::node::RewardCategory;
using nodo::node::rewardCategoryBlockedInDefenseMode;
using nodo::node::rewardCategoryFromString;
using nodo::node::rewardCategoryRequiresWorkEvidence;
using nodo::node::rewardCategoryToString;

void testRoundTrip() {
  const RewardCategory categories[] = {
      RewardCategory::NORMAL_BLOCK, RewardCategory::PROTECTION,
      RewardCategory::DEFERRED_PROTECTION, RewardCategory::EXTRAORDINARY,
      RewardCategory::REJECTED};
  for (const auto cat : categories) {
    RewardCategory out;
    assert(rewardCategoryFromString(rewardCategoryToString(cat), out));
    assert(out == cat);
  }
}

void testUnknownStringRejected() {
  RewardCategory out = RewardCategory::NORMAL_BLOCK;
  assert(!rewardCategoryFromString("UNKNOWN_CATEGORY", out));
  assert(!rewardCategoryFromString("", out));
}

void testProtectionRequiresWorkEvidence() {
  assert(rewardCategoryRequiresWorkEvidence(RewardCategory::PROTECTION));
  assert(
      rewardCategoryRequiresWorkEvidence(RewardCategory::DEFERRED_PROTECTION));
  assert(!rewardCategoryRequiresWorkEvidence(RewardCategory::NORMAL_BLOCK));
  assert(!rewardCategoryRequiresWorkEvidence(RewardCategory::EXTRAORDINARY));
  assert(!rewardCategoryRequiresWorkEvidence(RewardCategory::REJECTED));
}

void testExtraordinaryBlockedInDefenseMode() {
  assert(rewardCategoryBlockedInDefenseMode(RewardCategory::EXTRAORDINARY));
  assert(!rewardCategoryBlockedInDefenseMode(RewardCategory::NORMAL_BLOCK));
  assert(!rewardCategoryBlockedInDefenseMode(RewardCategory::PROTECTION));
  assert(
      !rewardCategoryBlockedInDefenseMode(RewardCategory::DEFERRED_PROTECTION));
  assert(!rewardCategoryBlockedInDefenseMode(RewardCategory::REJECTED));
}

void testToString() {
  assert(rewardCategoryToString(RewardCategory::NORMAL_BLOCK) ==
         "NORMAL_BLOCK");
  assert(rewardCategoryToString(RewardCategory::PROTECTION) == "PROTECTION");
  assert(rewardCategoryToString(RewardCategory::DEFERRED_PROTECTION) ==
         "DEFERRED_PROTECTION");
  assert(rewardCategoryToString(RewardCategory::EXTRAORDINARY) ==
         "EXTRAORDINARY");
  assert(rewardCategoryToString(RewardCategory::REJECTED) == "REJECTED");
}

} // namespace

int main() {
  testRoundTrip();
  testUnknownStringRejected();
  testProtectionRequiresWorkEvidence();
  testExtraordinaryBlockedInDefenseMode();
  testToString();
  return 0;
}
