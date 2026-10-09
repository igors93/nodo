#ifndef NODO_CONSENSUS_V1_UPGRADE_SCHEDULE_HPP
#define NODO_CONSENSUS_V1_UPGRADE_SCHEDULE_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace nodo::consensus {

// Checked reference for v1's height-activated rule commitments. Callers apply
// finalized, authorized governance mutations in increasing height order before
// computing that height's header commitment. Governance authorization, typed
// state and future-version execution remain separate.
class V1UpgradeSchedule {
public:
  using Digest = std::array<unsigned char, 32>;

  struct Rules {
    std::uint16_t version;
    Digest hash;
    bool operator==(const Rules &) const = default;
  };

  struct HeaderRules {
    Rules current;
    Rules next;
    bool operator==(const HeaderRules &) const = default;
  };

  static constexpr std::uint64_t kFullNoticeEpochs = 4;

  static std::optional<V1UpgradeSchedule> create(
      std::uint64_t epochLengthBlocks, const Digest &initialRulesHash);

  // The three content hashes are commitments to a published, immutable rule
  // bundle, conformance suite and migration. A zero migration hash means none.
  static Digest ruleSetHash(std::uint16_t version,
                            const Digest &bundleHash,
                            const Digest &vectorsHash,
                            const Digest &migrationHash);

  std::optional<std::uint64_t>
  earliestActivationHeight(std::uint64_t executionHeight) const;
  std::optional<Rules> schedule(std::uint64_t executionHeight,
                                std::uint64_t activationHeight,
                                std::uint16_t nextVersion,
                                const Digest &bundleHash,
                                const Digest &vectorsHash,
                                const Digest &migrationHash);
  bool cancelPending(std::uint64_t executionHeight,
                     std::uint64_t activationHeight,
                     const Digest &ruleSetHash);
  Rules activeAt(std::uint64_t height) const;
  std::optional<HeaderRules> headerRules(std::uint64_t height) const;

private:
  struct Upgrade {
    std::uint64_t activationHeight;
    Rules rules;
    bool canceled;
  };

  V1UpgradeSchedule(std::uint64_t epochLengthBlocks,
                    const Digest &initialRulesHash);
  bool hasPendingAt(std::uint64_t height) const;

  std::uint64_t m_epochLengthBlocks;
  Digest m_initialRulesHash;
  std::uint64_t m_lastMutationHeight = 0;
  std::vector<Upgrade> m_upgrades;
};

} // namespace nodo::consensus

#endif
