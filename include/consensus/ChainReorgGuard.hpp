#ifndef NODO_CONSENSUS_CHAIN_REORG_GUARD_HPP
#define NODO_CONSENSUS_CHAIN_REORG_GUARD_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::consensus {

struct ChainReorgGuardConfig {
  std::uint64_t maxReorgDepth; // reject reorgs deeper than this (default: 6)
  std::uint64_t alertThresholdDepth; // emit alert for reorgs deeper than this
                                     // (default: 3)

  static ChainReorgGuardConfig defaults();
  bool isValid() const;
  std::string serialize() const;
};

enum class ReorgCheckOutcome {
  ALLOWED,            // depth <= alertThreshold
  ALLOWED_WITH_ALERT, // alertThreshold < depth <= maxReorgDepth
  REJECTED            // depth > maxReorgDepth
};

std::string reorgCheckOutcomeToString(ReorgCheckOutcome outcome);

class ReorgEvent {
public:
  ReorgEvent();

  ReorgEvent(std::int64_t timestamp,
             std::uint64_t fromHeight, // height of the common ancestor
             std::uint64_t localTipHeight, std::uint64_t candidateTipHeight,
             std::uint64_t depth, ReorgCheckOutcome outcome,
             std::string reason);

  std::int64_t timestamp() const;
  std::uint64_t fromHeight() const;
  std::uint64_t localTipHeight() const;
  std::uint64_t candidateTipHeight() const;
  std::uint64_t depth() const;
  ReorgCheckOutcome outcome() const;
  const std::string &reason() const;

  bool isValid() const;
  std::string serialize() const;

private:
  std::int64_t m_timestamp;
  std::uint64_t m_fromHeight;
  std::uint64_t m_localTipHeight;
  std::uint64_t m_candidateTipHeight;
  std::uint64_t m_depth;
  ReorgCheckOutcome m_outcome;
  std::string m_reason;
};

class ChainReorgCheckResult {
public:
  static ChainReorgCheckResult allowed(std::uint64_t depth, std::string reason);
  static ChainReorgCheckResult allowedWithAlert(std::uint64_t depth,
                                                std::string reason);
  static ChainReorgCheckResult rejected(std::uint64_t depth,
                                        std::string reason);

  ReorgCheckOutcome outcome() const;
  std::uint64_t depth() const;
  const std::string &reason() const;
  bool isAllowed() const;
  bool isRejected() const;
  bool requiresAlert() const;
  std::string serialize() const;

private:
  ChainReorgCheckResult();

  ReorgCheckOutcome m_outcome;
  std::uint64_t m_depth;
  std::string m_reason;
};

/*
 * ChainReorgGuard evaluates whether a proposed chain reorganization is safe.
 *
 * Security principle:
 * Deep reorgs are a sign of a long-range attack or severe network partition.
 * Nodes must reject reorgs beyond the configured depth limit even if the
 * candidate chain is valid per fork choice rules.
 */
class ChainReorgGuard {
public:
  explicit ChainReorgGuard(
      ChainReorgGuardConfig config = ChainReorgGuardConfig::defaults());

  // Check if a reorg from localTipHeight back to commonAncestorHeight is safe.
  // depth = localTipHeight - commonAncestorHeight
  ChainReorgCheckResult checkReorg(std::uint64_t localTipHeight,
                                   std::uint64_t commonAncestorHeight,
                                   std::int64_t now) const;

  // Record a reorg event (for audit/logging)
  void recordReorgEvent(const ReorgEvent &event);

  const std::vector<ReorgEvent> &reorgHistory() const;
  std::size_t totalRejectedReorgs() const;
  std::size_t totalAlertedReorgs() const;

  const ChainReorgGuardConfig &config() const;

private:
  ChainReorgGuardConfig m_config;
  std::vector<ReorgEvent> m_reorgHistory;
  std::size_t m_totalRejected{0};
  std::size_t m_totalAlerted{0};
};

} // namespace nodo::consensus

#endif
