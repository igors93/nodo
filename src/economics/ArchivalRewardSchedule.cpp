#include "economics/ArchivalRewardSchedule.hpp"

#include "economics/ProtectionWorkType.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "serialization/V1EncodingPrimitives.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace nodo::economics {

namespace {

using Wide = unsigned __int128;

constexpr std::uint32_t kBasisPoints = config::HistoryParameters::kBasisPoints;
constexpr Wide kUnitWeight =
    static_cast<Wide>(kBasisPoints) * kBasisPoints * kBasisPoints;

Wide checkedAdd(Wide left, Wide right) {
  if (left > std::numeric_limits<Wide>::max() - right) {
    throw std::overflow_error("Archival reward weight overflow.");
  }
  return left + right;
}

Wide checkedMultiply(Wide left, Wide right) {
  if (right != 0 && left > std::numeric_limits<Wide>::max() / right) {
    throw std::overflow_error("Archival reward weight overflow.");
  }
  return left * right;
}

// floor(amount * part / whole) for part <= whole. Both terms are shifted
// right by the same deterministic amount until whole fits in 64 bits, so the
// product fits in 128 bits; the sum of results never exceeds `amount`.
std::int64_t proportional(std::int64_t amount, Wide part, Wide whole) {
  if (amount <= 0 || part == 0 || whole == 0) {
    return 0;
  }
  if (part > whole) {
    throw std::logic_error("Archival reward share exceeds the whole.");
  }
  while (whole >> 64 != 0) {
    part >>= 1;
    whole >>= 1;
  }
  return static_cast<std::int64_t>(
      (static_cast<Wide>(static_cast<std::uint64_t>(amount)) * part) / whole);
}

std::string hashHex(const char *domain,
                    const serialization::CanonicalWriter &writer) {
  return serialization::V1EncodingPrimitives::hex(
      serialization::V1EncodingPrimitives::hash(domain, writer.bytes()));
}

void writeWide(serialization::CanonicalWriter &writer, Wide value) {
  writer.writeUInt64(static_cast<std::uint64_t>(value >> 64));
  writer.writeUInt64(static_cast<std::uint64_t>(value));
}

} // namespace

bool ArchivalRewardSettlement::isValid() const {
  if (budget.isNegative() || distributed.isNegative() || unminted.isNegative() ||
      distributed > budget || distributed + unminted != budget ||
      demandBasisPoints > kBasisPoints) {
    return false;
  }
  utils::Amount sum;
  std::set<std::string> providers;
  for (const ArchivalProviderReward &reward : rewards) {
    if (!reward.amount.isPositive() || !providers.insert(reward.providerId).second) {
      return false;
    }
    sum = sum + reward.amount;
  }
  return sum == distributed;
}

std::string ArchivalRewardSettlement::digest() const {
  serialization::CanonicalWriter writer;
  writer.writeString("NODO_ARCHIVAL_REWARD_SETTLEMENT_V1");
  writer.writeUInt64(epoch);
  writer.writeInt64(budget.rawUnits());
  writer.writeInt64(distributed.rawUnits());
  writer.writeInt64(unminted.rawUnits());
  writer.writeUInt32(demandBasisPoints);
  writer.writeUInt32(static_cast<std::uint32_t>(rewards.size()));
  for (const ArchivalProviderReward &reward : rewards) {
    writer.writeString(reward.providerId);
    writer.writeString(reward.operatorId);
    writer.writeInt64(reward.amount.rawUnits());
    writer.writeString(reward.evidenceDigest);
  }
  return hashHex("ARCHIVE/REWARD-SETTLEMENT", writer);
}

ArchivalRewardSettlement
ArchivalRewardSchedule::settle(const config::HistoryParameters &parameters,
                               const ArchivalRewardInputs &inputs) {
  if (!parameters.isValid() || inputs.epochEmissionCap.isNegative()) {
    throw std::invalid_argument("Archival rewards need valid parameters.");
  }
  ArchivalRewardSettlement settlement;
  settlement.epoch = inputs.epoch;
  settlement.budget =
      ProtectionBudgetSplit::withArchival(parameters.archiveRewardShareBasisPoints())
          .slice(inputs.epochEmissionCap, ProtectionWorkType::HISTORICAL_ARCHIVAL);

  std::map<std::uint64_t, ArchivalRewardSegment> segments;
  Wide target = 0;
  for (const ArchivalRewardSegment &segment : inputs.segments) {
    if (!segments.emplace(segment.segmentIndex, segment).second) {
      throw std::invalid_argument("Duplicate archival reward segment.");
    }
    target = checkedAdd(
        target, checkedMultiply(checkedMultiply(segment.totalBytes,
                                                parameters.archiveReplicationTarget()),
                                kUnitWeight));
  }

  struct ProviderWeight {
    std::string operatorId;
    Wide weight = 0;
    serialization::CanonicalWriter evidence;
  };
  std::map<std::string, ProviderWeight> providers;
  std::set<std::pair<std::string, std::uint64_t>> seenSlots;
  Wide total = 0;
  for (const ArchivalRewardSlot &slot : inputs.slots) {
    if (!seenSlots.insert({slot.providerId, slot.segmentIndex}).second ||
        slot.passed > slot.issued) {
      throw std::invalid_argument("Archival reward slots are inconsistent.");
    }
    const auto segment = segments.find(slot.segmentIndex);
    if (segment == segments.end()) {
      throw std::invalid_argument("Archival reward slot names no segment.");
    }
    if (slot.issued == 0 || slot.passed == 0 || slot.fraud) {
      continue; // No proof, no reward; fraud forfeits the slot.
    }
    const std::uint32_t availability = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(slot.passed) * kBasisPoints) / slot.issued);
    if (availability < parameters.archiveMinAvailabilityBasisPoints()) {
      continue;
    }
    const std::uint32_t scarcity =
        parameters.scarcityMultiplierBasisPoints(segment->second.provenReplicas);
    const auto reliabilityFound = inputs.reliabilityBasisPoints.find(slot.providerId);
    const std::uint32_t reliability =
        std::min(kBasisPoints, reliabilityFound == inputs.reliabilityBasisPoints.end()
                                   ? parameters.archiveReliabilityFloorBasisPoints()
                                   : reliabilityFound->second);
    const Wide weight = checkedMultiply(
        checkedMultiply(checkedMultiply(segment->second.totalBytes, scarcity),
                        availability),
        reliability);
    ProviderWeight &provider = providers[slot.providerId];
    provider.operatorId = slot.operatorId;
    provider.weight = checkedAdd(provider.weight, weight);
    provider.evidence.writeUInt64(slot.segmentIndex);
    provider.evidence.writeUInt32(slot.issued);
    provider.evidence.writeUInt32(slot.passed);
    provider.evidence.writeUInt32(scarcity);
    provider.evidence.writeUInt32(reliability);
    total = checkedAdd(total, weight);
  }

  const Wide denominator = std::max(target, total);
  settlement.demandBasisPoints =
      target == 0 ? 0
                  : static_cast<std::uint32_t>(
                        std::min<Wide>(kBasisPoints,
                                       checkedMultiply(total, kBasisPoints) / target));
  for (auto &[providerId, provider] : providers) {
    const std::int64_t amount =
        proportional(settlement.budget.rawUnits(), provider.weight, denominator);
    if (amount <= 0) {
      continue;
    }
    serialization::CanonicalWriter evidence;
    evidence.writeUInt64(inputs.epoch);
    evidence.writeString(providerId);
    writeWide(evidence, provider.weight);
    const std::vector<unsigned char> &slots = provider.evidence.bytes();
    evidence.writeBytes(slots);
    ArchivalProviderReward reward;
    reward.providerId = providerId;
    reward.operatorId = provider.operatorId;
    reward.amount = utils::Amount::fromRawUnits(amount);
    reward.evidenceDigest = hashHex("ARCHIVE/REWARD-EVIDENCE", evidence);
    settlement.distributed = settlement.distributed + reward.amount;
    settlement.rewards.push_back(std::move(reward));
  }
  settlement.unminted = settlement.budget - settlement.distributed;
  if (!settlement.isValid()) {
    throw std::logic_error("Archival reward settlement broke an invariant.");
  }
  return settlement;
}

bool ArchivalRewardSchedule::audit(const config::HistoryParameters &parameters,
                                   const ArchivalRewardInputs &inputs,
                                   const ArchivalRewardSettlement &settlement,
                                   std::string &reason) {
  try {
    if (!settlement.isValid()) {
      reason = "settlement violates a monetary invariant";
      return false;
    }
    if (settlement.budget > inputs.epochEmissionCap) {
      reason = "archival budget exceeds the epoch emission cap";
      return false;
    }
    const ArchivalRewardSettlement expected = settle(parameters, inputs);
    if (expected.digest() != settlement.digest()) {
      reason = "settlement does not match the proven archival work";
      return false;
    }
    reason.clear();
    return true;
  } catch (const std::exception &error) {
    reason = error.what();
    return false;
  }
}

} // namespace nodo::economics
