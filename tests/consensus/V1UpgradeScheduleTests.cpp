#include "../common/TestFramework.hpp"
#include "consensus/V1UpgradeSchedule.hpp"
#include "serialization/V1EncodingPrimitives.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

using nodo::consensus::V1UpgradeSchedule;
using nodo::serialization::V1EncodingPrimitives;
using nodo::test::require;

V1UpgradeSchedule::Digest filled(unsigned char byte) {
  V1UpgradeSchedule::Digest result{};
  result.fill(byte);
  return result;
}

void testRuleCommitment() {
  const auto digest = V1UpgradeSchedule::ruleSetHash(
      2, filled(0x11), filled(0x22), {});
  require(V1EncodingPrimitives::hex(digest) ==
              "6deaf29d503086905c6e1739276667cc20b29a0b9802bbb2ff935fbb81f3c851",
          "rule set hash must match independent binary domain vector");
  bool rejected = false;
  try {
    (void)V1UpgradeSchedule::ruleSetHash(0, filled(0x11), filled(0x22), {});
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "version zero must not identify a rule set");
  rejected = false;
  try {
    (void)V1UpgradeSchedule::ruleSetHash(2, {}, filled(0x22), {});
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "a missing bundle commitment must fail closed");
}

void testActivationBoundaryAndNotice() {
  require(!V1UpgradeSchedule::create(0, filled(0x33)) &&
              !V1UpgradeSchedule::create(10, {}),
          "zero epoch length or missing genesis rules must fail");
  auto schedule = V1UpgradeSchedule::create(10, filled(0x33));
  require(schedule.has_value(), "the reference genesis must be valid");
  require(!schedule->earliestActivationHeight(0) &&
              schedule->earliestActivationHeight(1) == 51 &&
              schedule->earliestActivationHeight(10) == 51 &&
              schedule->earliestActivationHeight(11) == 61,
          "four complete future epochs must elapse before activation");
  require(!schedule->schedule(12, 51, 2, filled(0x11), filled(0x22), {}) &&
              !schedule->schedule(12, 60, 2, filled(0x11), filled(0x22), {}) &&
              !schedule->schedule(12, 61, 3, filled(0x11), filled(0x22), {}),
          "early, non-boundary and skipped versions must fail");
  const auto next = schedule->schedule(12, 61, 2, filled(0x11), filled(0x22), {});
  require(next && next->version == 2 &&
              next->hash == V1UpgradeSchedule::ruleSetHash(
                                2, filled(0x11), filled(0x22), {}),
          "only the authorized next version can be scheduled");
  require(!schedule->schedule(13, 71, 2, filled(0x11), filled(0x22), {}),
          "two simultaneous pending upgrades must fail");
  const auto before = schedule->headerRules(59);
  const auto boundary = schedule->headerRules(60);
  const auto after = schedule->headerRules(61);
  require(before && boundary && after &&
              before->current.version == 1 && before->next.version == 1 &&
              boundary->current.version == 1 && boundary->next.version == 2 &&
              after->current.version == 2 && after->next.version == 2,
          "the old-rule boundary must commit the exact next rules");
  require(!schedule->headerRules(std::numeric_limits<std::uint64_t>::max()),
          "a next-height overflow must fail closed");
}

void testCancellationAndHistoricalReplay() {
  auto schedule = V1UpgradeSchedule::create(10, filled(0x33));
  require(schedule.has_value(), "the reference genesis must be valid");
  require(schedule->schedule(12, 61, 2, filled(0x11), filled(0x22), {})
              .has_value(),
          "the first upgrade must schedule");
  const auto ruleHash = V1UpgradeSchedule::ruleSetHash(
      2, filled(0x11), filled(0x22), {});
  require(!schedule->cancelPending(13, 71, ruleHash) &&
              !schedule->cancelPending(14, 61, filled(0x99)) &&
              !schedule->cancelPending(12, 61, ruleHash) &&
              schedule->cancelPending(60, 61, ruleHash),
          "cancellation must target the exact pending entry before activation");
  require(schedule->headerRules(60)->next.version == 1 &&
              schedule->activeAt(61).version == 1 &&
              !schedule->cancelPending(61, 61, ruleHash),
          "canceled rules must never become active or cancel twice");
  require(schedule->schedule(62, 111, 2, filled(0x11), filled(0x22), {})
              .has_value(),
          "a new approved plan can replace a canceled plan after notice");
  require(schedule->activeAt(110).version == 1 &&
              schedule->activeAt(111).version == 2,
          "activation must select rules solely by finalized height");
  require(schedule->schedule(112, 161, 3, filled(0x44), filled(0x55), {})
              .has_value(),
          "later versions can use the same mechanism after activation");
  require(schedule->activeAt(50).version == 1 &&
              schedule->activeAt(120).version == 2 &&
              schedule->activeAt(161).version == 3,
          "historical rules cannot be rewritten by a later upgrade");
}

void testOverflow() {
  auto schedule = V1UpgradeSchedule::create(
      std::numeric_limits<std::uint64_t>::max(), filled(0x33));
  require(schedule && !schedule->earliestActivationHeight(1) &&
              !schedule->schedule(1, 2, 2, filled(0x11), filled(0x22), {}),
          "epoch arithmetic overflow must reject a plan before state mutation");
}

void testCanceledHeightIsNeverReused() {
  auto schedule = V1UpgradeSchedule::create(10, filled(0x33));
  require(schedule.has_value(), "the reference genesis must be valid");
  const auto first = schedule->schedule(12, 101, 2, filled(0x11),
                                      filled(0x22), {});
  require(first.has_value() && schedule->cancelPending(13, 101, first->hash),
          "the initial plan can be canceled before its boundary");
  require(!schedule->schedule(14, 101, 2, filled(0x44), filled(0x55), {}),
          "a canceled schedule key remains an auditable tombstone");
  require(schedule->schedule(14, 111, 2, filled(0x44), filled(0x55), {})
              .has_value(),
          "a canceled version can be reproposed at a fresh height");
}

} // namespace

int main() {
  try {
    testRuleCommitment();
    testActivationBoundaryAndNotice();
    testCancellationAndHistoricalReplay();
    testOverflow();
    testCanceledHeightIsNeverReused();
    std::cout << "V1 upgrade schedule tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "V1 upgrade schedule tests failed: " << error.what()
              << '\n';
    return 1;
  }
}
