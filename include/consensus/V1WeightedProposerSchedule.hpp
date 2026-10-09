#ifndef NODO_CONSENSUS_V1_WEIGHTED_PROPOSER_SCHEDULE_HPP
#define NODO_CONSENSUS_V1_WEIGHTED_PROPOSER_SCHEDULE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nodo::consensus {

// Pure checked reference for v1 proposer selection. Callers must authenticate
// validator-set roots and finalized state; this does not switch nodo/0.x.
class V1WeightedProposerSchedule {
public:
  using Digest = std::array<unsigned char, 32>;
  using Priority = __int128;

  struct Validator {
    Digest id;
    std::uint64_t weight;
    bool operator==(const Validator &) const = default;
  };

  struct NextSet {
    Digest root;
    std::vector<Validator> validators;
  };

  // Derived from the 1 MiB canonical validator-set object cap.
  static constexpr std::size_t kMaxValidators = 9619;

  static std::optional<V1WeightedProposerSchedule>
  create(std::uint64_t epochLengthBlocks, const Digest &genesisSetRoot,
         const std::vector<Validator> &genesisValidators);

  // Round zero is primary. The first n rounds enumerate every active ID.
  std::optional<Digest> proposer(std::uint64_t height,
                                 std::uint64_t round) const;

  // Apply exactly one finalized height. A changed set is legal only at an
  // epoch boundary; the signed block proposer must match this schedule.
  bool finalize(std::uint64_t height, std::uint64_t round,
                const Digest &signedProposer,
                const std::optional<NextSet> &nextSet = std::nullopt);

  // Exact tag-12 state value: set root, finalized height, total weight,
  // and the sorted (ID, i128 priority) list.
  std::vector<unsigned char> stateValueBytes() const;
  std::uint64_t nextHeight() const { return m_nextHeight; }
  std::uint64_t totalWeight() const { return m_totalWeight; }

private:
  struct Ranking {
    std::vector<Priority> scores;
    std::vector<std::size_t> order;
  };

  V1WeightedProposerSchedule(std::uint64_t epochLengthBlocks,
                             const Digest &root,
                             const std::vector<Validator> &validators,
                             std::uint64_t totalWeight);

  static bool validSet(const std::vector<Validator> &validators,
                       std::uint64_t &totalWeight);
  std::optional<Ranking> ranking() const;
  static bool rebase(const std::vector<Validator> &oldValidators,
                     const std::vector<Priority> &oldPriorities,
                     const std::vector<Validator> &newValidators,
                     std::uint64_t newWeight,
                     std::vector<Priority> &result);
  static bool normalize(std::vector<Priority> &priorities,
                        std::uint64_t totalWeight);

  std::uint64_t m_epochLengthBlocks;
  std::uint64_t m_nextHeight = 1;
  Digest m_setRoot;
  std::uint64_t m_totalWeight;
  std::vector<Validator> m_validators;
  std::vector<Priority> m_priorities;
};

} // namespace nodo::consensus

#endif
