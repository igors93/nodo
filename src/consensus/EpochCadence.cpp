#include "consensus/EpochCadence.hpp"

#include <limits>

namespace nodo::consensus {

EpochCadence::EpochCadence(std::uint64_t epochLengthBlocks,
                           std::uint64_t targetBlockSeconds)
    : m_epochLengthBlocks(epochLengthBlocks),
      m_targetBlockSeconds(targetBlockSeconds) {}

std::optional<EpochCadence> EpochCadence::create(
    std::uint64_t epochLengthBlocks, std::uint64_t targetBlockSeconds) {
  if (epochLengthBlocks == 0 || targetBlockSeconds == 0 ||
      targetBlockSeconds > kMaxTargetBlockSeconds ||
      epochLengthBlocks >
          kMaxNominalEpochSeconds / targetBlockSeconds ||
      epochLengthBlocks * targetBlockSeconds < kMinNominalEpochSeconds) {
    return std::nullopt;
  }
  return EpochCadence(epochLengthBlocks, targetBlockSeconds);
}

std::uint64_t EpochCadence::epochLengthBlocks() const {
  return m_epochLengthBlocks;
}

std::uint64_t EpochCadence::targetBlockSeconds() const {
  return m_targetBlockSeconds;
}

std::uint64_t EpochCadence::nominalEpochSeconds() const {
  return m_epochLengthBlocks * m_targetBlockSeconds;
}

std::optional<std::uint64_t> EpochCadence::epochForHeight(
    std::uint64_t height) const {
  if (height == 0) {
    return std::nullopt;
  }
  return (height - 1) / m_epochLengthBlocks;
}

bool EpochCadence::isFinalBlockOfEpoch(std::uint64_t height) const {
  return height != 0 && height % m_epochLengthBlocks == 0;
}

std::optional<std::uint64_t> EpochCadence::firstHeight(
    std::uint64_t epoch) const {
  constexpr std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
  if (epoch > (max - 1) / m_epochLengthBlocks) {
    return std::nullopt;
  }
  return epoch * m_epochLengthBlocks + 1;
}

std::optional<std::uint64_t> EpochCadence::lastHeight(
    std::uint64_t epoch) const {
  constexpr std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
  if (epoch == max || epoch + 1 > max / m_epochLengthBlocks) {
    return std::nullopt;
  }
  return (epoch + 1) * m_epochLengthBlocks;
}

std::optional<std::int64_t> EpochCadence::earliestHeaderTime(
    std::int64_t genesisTime, std::uint64_t height) const {
  if (genesisTime <= 0) {
    return std::nullopt;
  }
  const std::uint64_t available = static_cast<std::uint64_t>(
      std::numeric_limits<std::int64_t>::max() - genesisTime);
  if (height > available / m_targetBlockSeconds) {
    return std::nullopt;
  }
  return genesisTime +
         static_cast<std::int64_t>(height * m_targetBlockSeconds);
}

} // namespace nodo::consensus
