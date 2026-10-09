#ifndef NODO_ECONOMICS_ARCHIVAL_REWARD_SCHEDULE_HPP
#define NODO_ECONOMICS_ARCHIVAL_REWARD_SCHEDULE_HPP

#include "config/HistoryParameters.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::economics {

struct ArchivalRewardSegment {
  std::uint64_t segmentIndex = 0;
  std::uint64_t totalBytes = 0;
  // Distinct operators that proved the segment this epoch.
  std::uint32_t provenReplicas = 0;
};

struct ArchivalRewardSlot {
  std::string providerId;
  std::string operatorId;
  std::uint64_t segmentIndex = 0;
  std::uint32_t issued = 0;
  std::uint32_t passed = 0;
  bool fraud = false;
};

struct ArchivalRewardInputs {
  std::uint64_t epoch = 0;
  // EpochEmissionPolicy::calculateNewEmissionCap for the epoch: the ceiling
  // of every protection reward, archival included.
  utils::Amount epochEmissionCap;
  std::vector<ArchivalRewardSegment> segments;
  std::vector<ArchivalRewardSlot> slots;
  std::map<std::string, std::uint32_t> reliabilityBasisPoints;
};

struct ArchivalProviderReward {
  std::string providerId;
  std::string operatorId;
  utils::Amount amount;
  std::string evidenceDigest;
};

struct ArchivalRewardSettlement {
  std::uint64_t epoch = 0;
  utils::Amount budget;
  utils::Amount distributed;
  // Budget no proven work earned. It is never minted.
  utils::Amount unminted;
  std::uint32_t demandBasisPoints = 0;
  std::vector<ArchivalProviderReward> rewards;

  bool isValid() const;
  std::string digest() const;
};

/*
 * ArchivalRewardSchedule (ADR 0014): "no reward without measurable
 * protection work", bounded by monetary policy.
 *
 *   budget  = epochEmissionCap x archivalShare            (<= cap)
 *   weight  = bytes(segment) x scarcity(proven replicas)
 *             x availability(passed/issued) x reliability  per proven slot
 *   target  = sum(bytes) x replicationTarget x 1.0 x 1.0 x 1.0
 *   reward  = budget x weight / max(target, sum(weights))
 *
 * Below the target every weighted byte earns the same fixed rate, so a
 * provider's reward does not shrink when others join; scarce segments pay
 * more per byte. Above it the budget is shared pro rata. A slot with no
 * passed challenge, availability under the floor, or fraud earns nothing.
 * The sum of rewards never exceeds the budget; the remainder is not minted.
 * Integer arithmetic only; identical on every node.
 * This is a reference calculation: callers supply tallies. It is not an
 * authorization to mint. Activation requires the finalized, replayed ledger
 * and escrowed operator identities specified by roadmap 6.9.
 */
class ArchivalRewardSchedule {
public:
  static ArchivalRewardSettlement
  settle(const config::HistoryParameters &parameters,
         const ArchivalRewardInputs &inputs);

  // Recomputes the settlement and checks every monetary invariant; this is
  // the hook a supply audit uses.
  static bool audit(const config::HistoryParameters &parameters,
                    const ArchivalRewardInputs &inputs,
                    const ArchivalRewardSettlement &settlement,
                    std::string &reason);
};

} // namespace nodo::economics

#endif
