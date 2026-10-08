#ifndef NODO_CONSENSUS_EPOCH_CADENCE_HPP
#define NODO_CONSENSUS_EPOCH_CADENCE_HPP

#include <cstdint>
#include <optional>

namespace nodo::consensus {

// Checked reference for the v1 height-based epoch schedule. All inputs are
// immutable genesis parameters; local clocks and observed block intervals do
// not affect epoch membership.
class EpochCadence {
public:
  static constexpr std::uint64_t kMaxTargetBlockSeconds = 300;
  static constexpr std::uint64_t kMinNominalEpochSeconds = 24 * 60 * 60;
  static constexpr std::uint64_t kMaxNominalEpochSeconds = 7 * 24 * 60 * 60;

  static std::optional<EpochCadence> create(
      std::uint64_t epochLengthBlocks, std::uint64_t targetBlockSeconds);

  std::uint64_t epochLengthBlocks() const;
  std::uint64_t targetBlockSeconds() const;
  std::uint64_t nominalEpochSeconds() const;

  // Epochs are zero-based. Genesis height 0 has no consensus epoch.
  std::optional<std::uint64_t> epochForHeight(std::uint64_t height) const;
  bool isFinalBlockOfEpoch(std::uint64_t height) const;
  std::optional<std::uint64_t> firstHeight(std::uint64_t epoch) const;
  std::optional<std::uint64_t> lastHeight(std::uint64_t epoch) const;

  // Lower bound implied by the v1 BFT-time floor, not a predicted block time.
  // Heights beyond signed-i64 time or a positive genesis timestamp fail closed.
  std::optional<std::int64_t> earliestHeaderTime(
      std::int64_t genesisTime, std::uint64_t height) const;

private:
  EpochCadence(std::uint64_t epochLengthBlocks,
               std::uint64_t targetBlockSeconds);

  std::uint64_t m_epochLengthBlocks;
  std::uint64_t m_targetBlockSeconds;
};

} // namespace nodo::consensus

#endif
