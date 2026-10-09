#ifndef NODO_CONSENSUS_V1_LIVENESS_ACCOUNTABILITY_HPP
#define NODO_CONSENSUS_V1_LIVENESS_ACCOUNTABILITY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nodo::consensus {

// Checked v1 reference. The caller MUST first authenticate the parent QC,
// historical validator set, ADR 0007 cadence parameters and finalized state.
// This does not switch nodo/0.x.
class V1LivenessAccountability {
public:
  using Digest = std::array<unsigned char, 32>;

  struct Validator {
    Digest id;
    std::uint64_t weight;
    bool operator==(const Validator &) const = default;
  };

  struct ParentCommit {
    std::uint64_t height;
    Digest setRoot;
    std::vector<Digest> signers; // Strictly increasing IDs from a verified QC.
  };

  struct NextSet {
    Digest root;
    std::vector<Validator> validators;
  };

  struct Assessment {
    Digest validatorId;
    std::uint64_t signedHeights;
    std::uint64_t observedHeights;
    bool inactivityCandidate; // Reversible eligibility only, never slash.
  };

  struct Transition {
    bool boundary;
    std::vector<Assessment> assessments; // Populated at an epoch boundary.
  };

  static constexpr std::size_t kMaxValidators = 9619;

  static std::optional<V1LivenessAccountability>
  create(std::uint64_t epochLengthBlocks, const Digest &genesisSetRoot,
         const std::vector<Validator> &genesisValidators);

  // Height h consumes the verified QC for h-1. The QC at an epoch boundary
  // is deliberately excluded from the next epoch's participation window.
  // On boundary h, nextSet is the frozen set selected for h+1, if changed.
  std::optional<Transition>
  advance(std::uint64_t height, const std::optional<ParentCommit> &parent,
          const std::optional<NextSet> &nextSet = std::nullopt);

  // Exact domain-13 state value, committed after each finalized height.
  std::vector<unsigned char> stateValueBytes() const;
  std::uint64_t nextHeight() const { return m_nextHeight; }

private:
  V1LivenessAccountability(std::uint64_t epochLengthBlocks,
                           const Digest &root,
                           const std::vector<Validator> &validators,
                           std::uint64_t totalWeight);
  static bool validSet(const std::vector<Validator> &validators,
                       std::uint64_t &totalWeight);
  static std::optional<std::vector<std::size_t>>
  verifiedSignerIndexes(const std::vector<Digest> &signers,
                        const std::vector<Validator> &validators,
                        std::uint64_t totalWeight);

  std::uint64_t m_epochLengthBlocks;
  std::uint64_t m_nextHeight = 1;
  std::uint64_t m_epoch = 0;
  std::uint64_t m_observed = 0;
  std::uint64_t m_totalWeight;
  Digest m_setRoot;
  std::optional<Digest> m_previousBoundaryRoot;
  std::vector<Validator> m_previousBoundaryValidators;
  std::uint64_t m_previousBoundaryWeight = 0;
  std::vector<Validator> m_validators;
  std::vector<std::uint64_t> m_signed;
};

} // namespace nodo::consensus

#endif
