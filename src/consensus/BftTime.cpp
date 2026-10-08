#include "consensus/BftTime.hpp"
#include "consensus/EpochCadence.hpp"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

namespace nodo::consensus {

std::optional<std::int64_t> BftTime::expectedChildTime(
    const core::Block &parent, std::uint64_t targetBlockSeconds,
    const QuorumCertificate *parentCertificate,
    const core::ValidatorSetHistory *validatorSetHistory,
    const crypto::CryptoPolicy &policy,
    const crypto::SignatureProvider &provider) {
  if (!parent.isValid(false) || parent.timestamp() <= 0 ||
      parent.previousHash() == "SNAPSHOT" ||
      parent.index() == std::numeric_limits<std::uint64_t>::max() ||
      targetBlockSeconds == 0 ||
      targetBlockSeconds > EpochCadence::kMaxTargetBlockSeconds ||
      targetBlockSeconds > static_cast<std::uint64_t>(
                               std::numeric_limits<std::int64_t>::max() -
                               parent.timestamp())) {
    return std::nullopt;
  }
  const std::int64_t minimum =
      parent.timestamp() + static_cast<std::int64_t>(targetBlockSeconds);

  if (parent.isGenesisBlock()) {
    return parentCertificate == nullptr ? std::optional<std::int64_t>(minimum)
                                        : std::nullopt;
  }
  if (parentCertificate == nullptr || validatorSetHistory == nullptr ||
      !validatorSetHistory->hasSet(parent.index()) ||
      parentCertificate->blockIndex() != parent.index() ||
      parentCertificate->blockHash() != parent.hash() ||
      parentCertificate->previousHash() != parent.previousHash()) {
    return std::nullopt;
  }
  const core::ValidatorRegistry &parentValidatorSet =
      validatorSetHistory->setAt(parent.index());
  if (!parentCertificate->verify(parentValidatorSet, policy, provider)) {
    return std::nullopt;
  }

  std::vector<std::pair<std::int64_t, std::uint64_t>> voteTimes;
  voteTimes.reserve(parentCertificate->votes().size());
  std::uint64_t signedWeight = 0;
  for (const ValidatorVoteRecord &vote : parentCertificate->votes()) {
    if (vote.createdAt() < parent.timestamp()) {
      return std::nullopt;
    }
    const std::uint64_t weight =
        parentValidatorSet.consensusWeightFor(vote.validatorAddress());
    if (weight == 0 ||
        signedWeight > std::numeric_limits<std::uint64_t>::max() - weight) {
      return std::nullopt;
    }
    signedWeight += weight;
    voteTimes.emplace_back(vote.createdAt(), weight);
  }
  if (signedWeight != parentCertificate->signedVotingWeight() ||
      signedWeight == 0) {
    return std::nullopt;
  }

  std::sort(voteTimes.begin(), voteTimes.end());
  // The lower median resolves even-weight ties without depending on vote
  // insertion order. This is ceil(S/2), calculated without overflow.
  const std::uint64_t threshold =
      signedWeight / 2 + signedWeight % 2;
  std::uint64_t cumulative = 0;
  for (const auto &[time, weight] : voteTimes) {
    cumulative += weight; // bounded above by verified signedWeight
    if (cumulative >= threshold) {
      return std::max(minimum, time);
    }
  }
  return std::nullopt;
}

} // namespace nodo::consensus
