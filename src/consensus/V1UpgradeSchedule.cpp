#include "consensus/V1UpgradeSchedule.hpp"

#include "serialization/V1EncodingPrimitives.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace nodo::consensus {

namespace {

bool isZero(const V1UpgradeSchedule::Digest &digest) {
  return std::all_of(digest.begin(), digest.end(),
                     [](unsigned char byte) { return byte == 0; });
}

} // namespace

V1UpgradeSchedule::V1UpgradeSchedule(
    std::uint64_t epochLengthBlocks, const Digest &initialRulesHash)
    : m_epochLengthBlocks(epochLengthBlocks),
      m_initialRulesHash(initialRulesHash) {}

std::optional<V1UpgradeSchedule> V1UpgradeSchedule::create(
    std::uint64_t epochLengthBlocks, const Digest &initialRulesHash) {
  if (epochLengthBlocks == 0 || isZero(initialRulesHash)) {
    return std::nullopt;
  }
  return V1UpgradeSchedule(epochLengthBlocks, initialRulesHash);
}

V1UpgradeSchedule::Digest V1UpgradeSchedule::ruleSetHash(
    std::uint16_t version, const Digest &bundleHash,
    const Digest &vectorsHash, const Digest &migrationHash) {
  if (version == 0 || isZero(bundleHash) || isZero(vectorsHash)) {
    throw std::invalid_argument("Invalid v1 upgrade manifest commitment.");
  }
  std::vector<unsigned char> bytes;
  bytes.reserve(2 + 3 * 32);
  bytes.push_back(static_cast<unsigned char>(version >> 8));
  bytes.push_back(static_cast<unsigned char>(version));
  bytes.insert(bytes.end(), bundleHash.begin(), bundleHash.end());
  bytes.insert(bytes.end(), vectorsHash.begin(), vectorsHash.end());
  bytes.insert(bytes.end(), migrationHash.begin(), migrationHash.end());
  return serialization::V1EncodingPrimitives::hash("RULESET", bytes);
}

std::optional<std::uint64_t> V1UpgradeSchedule::earliestActivationHeight(
    std::uint64_t executionHeight) const {
  if (executionHeight == 0) {
    return std::nullopt;
  }
  const std::uint64_t executionEpoch =
      (executionHeight - 1) / m_epochLengthBlocks;
  constexpr std::uint64_t maximum =
      std::numeric_limits<std::uint64_t>::max();
  if (executionEpoch > maximum - kFullNoticeEpochs - 1) {
    return std::nullopt;
  }
  const std::uint64_t earliestEpoch =
      executionEpoch + kFullNoticeEpochs + 1;
  if (earliestEpoch > (maximum - 1) / m_epochLengthBlocks) {
    return std::nullopt;
  }
  return earliestEpoch * m_epochLengthBlocks + 1;
}

bool V1UpgradeSchedule::hasPendingAt(std::uint64_t height) const {
  return std::any_of(m_upgrades.begin(), m_upgrades.end(),
                     [height](const Upgrade &upgrade) {
                       return !upgrade.canceled &&
                              upgrade.activationHeight > height;
                     });
}

std::optional<V1UpgradeSchedule::Rules> V1UpgradeSchedule::schedule(
    std::uint64_t executionHeight, std::uint64_t activationHeight,
    std::uint16_t nextVersion, const Digest &bundleHash,
    const Digest &vectorsHash, const Digest &migrationHash) {
  const auto earliest = earliestActivationHeight(executionHeight);
  if (!earliest || executionHeight <= m_lastMutationHeight ||
      activationHeight < *earliest || activationHeight <= executionHeight ||
      (activationHeight - 1) % m_epochLengthBlocks != 0 ||
      hasPendingAt(executionHeight) ||
      std::any_of(m_upgrades.begin(), m_upgrades.end(),
                  [activationHeight](const Upgrade &upgrade) {
                    return upgrade.activationHeight == activationHeight;
                  })) {
    return std::nullopt;
  }
  const Rules current = activeAt(executionHeight);
  if (current.version == std::numeric_limits<std::uint16_t>::max() ||
      nextVersion != static_cast<std::uint16_t>(current.version + 1)) {
    return std::nullopt;
  }
  try {
    const Rules next{nextVersion,
                     ruleSetHash(nextVersion, bundleHash, vectorsHash,
                                 migrationHash)};
    m_upgrades.push_back({activationHeight, next, false});
    m_lastMutationHeight = executionHeight;
    return next;
  } catch (const std::invalid_argument &) {
    return std::nullopt;
  }
}

bool V1UpgradeSchedule::cancelPending(std::uint64_t executionHeight,
                                      std::uint64_t activationHeight,
                                      const Digest &ruleSetHash) {
  if (executionHeight == 0 || executionHeight <= m_lastMutationHeight) {
    return false;
  }
  for (auto it = m_upgrades.rbegin(); it != m_upgrades.rend(); ++it) {
    if (!it->canceled && it->activationHeight > executionHeight &&
        it->activationHeight == activationHeight &&
        it->rules.hash == ruleSetHash) {
      it->canceled = true;
      m_lastMutationHeight = executionHeight;
      return true;
    }
  }
  return false;
}

V1UpgradeSchedule::Rules
V1UpgradeSchedule::activeAt(std::uint64_t height) const {
  Rules active{1, m_initialRulesHash};
  for (const Upgrade &upgrade : m_upgrades) {
    if (!upgrade.canceled && upgrade.activationHeight <= height) {
      active = upgrade.rules;
    }
  }
  return active;
}

std::optional<V1UpgradeSchedule::HeaderRules>
V1UpgradeSchedule::headerRules(std::uint64_t height) const {
  if (height == std::numeric_limits<std::uint64_t>::max()) {
    return std::nullopt;
  }
  return HeaderRules{activeAt(height), activeAt(height + 1)};
}

} // namespace nodo::consensus
