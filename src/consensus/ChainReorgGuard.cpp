#include "consensus/ChainReorgGuard.hpp"

#include <sstream>
#include <utility>

namespace nodo::consensus {

// ChainReorgGuardConfig

ChainReorgGuardConfig ChainReorgGuardConfig::defaults() { return {6, 3}; }

bool ChainReorgGuardConfig::isValid() const {
  return maxReorgDepth > 0 && alertThresholdDepth < maxReorgDepth;
}

std::string ChainReorgGuardConfig::serialize() const {
  std::ostringstream oss;
  oss << "ChainReorgGuardConfig{"
      << "maxReorgDepth=" << maxReorgDepth
      << ";alertThresholdDepth=" << alertThresholdDepth << "}";
  return oss.str();
}

// ReorgCheckOutcome

std::string reorgCheckOutcomeToString(ReorgCheckOutcome outcome) {
  switch (outcome) {
  case ReorgCheckOutcome::ALLOWED:
    return "ALLOWED";
  case ReorgCheckOutcome::ALLOWED_WITH_ALERT:
    return "ALLOWED_WITH_ALERT";
  case ReorgCheckOutcome::REJECTED:
    return "REJECTED";
  default:
    return "REJECTED";
  }
}

// ReorgEvent

ReorgEvent::ReorgEvent()
    : m_timestamp(0), m_fromHeight(0), m_localTipHeight(0),
      m_candidateTipHeight(0), m_depth(0),
      m_outcome(ReorgCheckOutcome::REJECTED), m_reason("") {}

ReorgEvent::ReorgEvent(std::int64_t timestamp, std::uint64_t fromHeight,
                       std::uint64_t localTipHeight,
                       std::uint64_t candidateTipHeight, std::uint64_t depth,
                       ReorgCheckOutcome outcome, std::string reason)
    : m_timestamp(timestamp), m_fromHeight(fromHeight),
      m_localTipHeight(localTipHeight),
      m_candidateTipHeight(candidateTipHeight), m_depth(depth),
      m_outcome(outcome), m_reason(std::move(reason)) {}

std::int64_t ReorgEvent::timestamp() const { return m_timestamp; }

std::uint64_t ReorgEvent::fromHeight() const { return m_fromHeight; }

std::uint64_t ReorgEvent::localTipHeight() const { return m_localTipHeight; }

std::uint64_t ReorgEvent::candidateTipHeight() const {
  return m_candidateTipHeight;
}

std::uint64_t ReorgEvent::depth() const { return m_depth; }

ReorgCheckOutcome ReorgEvent::outcome() const { return m_outcome; }

const std::string &ReorgEvent::reason() const { return m_reason; }

bool ReorgEvent::isValid() const {
  return m_timestamp > 0 && m_localTipHeight >= m_fromHeight &&
         m_depth == m_localTipHeight - m_fromHeight;
}

std::string ReorgEvent::serialize() const {
  std::ostringstream oss;
  oss << "ReorgEvent{"
      << "timestamp=" << m_timestamp << ";fromHeight=" << m_fromHeight
      << ";localTipHeight=" << m_localTipHeight
      << ";candidateTipHeight=" << m_candidateTipHeight << ";depth=" << m_depth
      << ";outcome=" << reorgCheckOutcomeToString(m_outcome)
      << ";reason=" << m_reason << "}";
  return oss.str();
}

// ChainReorgCheckResult

ChainReorgCheckResult::ChainReorgCheckResult()
    : m_outcome(ReorgCheckOutcome::REJECTED), m_depth(0), m_reason("") {}

ChainReorgCheckResult ChainReorgCheckResult::allowed(std::uint64_t depth,
                                                     std::string reason) {
  ChainReorgCheckResult result;
  result.m_outcome = ReorgCheckOutcome::ALLOWED;
  result.m_depth = depth;
  result.m_reason = std::move(reason);
  return result;
}

ChainReorgCheckResult
ChainReorgCheckResult::allowedWithAlert(std::uint64_t depth,
                                        std::string reason) {
  ChainReorgCheckResult result;
  result.m_outcome = ReorgCheckOutcome::ALLOWED_WITH_ALERT;
  result.m_depth = depth;
  result.m_reason = std::move(reason);
  return result;
}

ChainReorgCheckResult ChainReorgCheckResult::rejected(std::uint64_t depth,
                                                      std::string reason) {
  ChainReorgCheckResult result;
  result.m_outcome = ReorgCheckOutcome::REJECTED;
  result.m_depth = depth;
  result.m_reason = std::move(reason);
  return result;
}

ReorgCheckOutcome ChainReorgCheckResult::outcome() const { return m_outcome; }

std::uint64_t ChainReorgCheckResult::depth() const { return m_depth; }

const std::string &ChainReorgCheckResult::reason() const { return m_reason; }

bool ChainReorgCheckResult::isAllowed() const {
  return m_outcome == ReorgCheckOutcome::ALLOWED ||
         m_outcome == ReorgCheckOutcome::ALLOWED_WITH_ALERT;
}

bool ChainReorgCheckResult::isRejected() const {
  return m_outcome == ReorgCheckOutcome::REJECTED;
}

bool ChainReorgCheckResult::requiresAlert() const {
  return m_outcome == ReorgCheckOutcome::ALLOWED_WITH_ALERT;
}

std::string ChainReorgCheckResult::serialize() const {
  std::ostringstream oss;
  oss << "ChainReorgCheckResult{"
      << "outcome=" << reorgCheckOutcomeToString(m_outcome)
      << ";depth=" << m_depth << ";reason=" << m_reason << "}";
  return oss.str();
}

// ChainReorgGuard

ChainReorgGuard::ChainReorgGuard(ChainReorgGuardConfig config)
    : m_config(config), m_reorgHistory(), m_totalRejected(0),
      m_totalAlerted(0) {}

ChainReorgCheckResult
ChainReorgGuard::checkReorg(std::uint64_t localTipHeight,
                            std::uint64_t commonAncestorHeight,
                            [[maybe_unused]] std::int64_t now) const {
  // Overflow guard: if ancestor is deeper than tip, treat as depth 0
  const std::uint64_t depth = (localTipHeight >= commonAncestorHeight)
                                  ? (localTipHeight - commonAncestorHeight)
                                  : 0;

  if (depth > m_config.maxReorgDepth) {
    return ChainReorgCheckResult::rejected(
        depth, "Reorg depth " + std::to_string(depth) +
                   " exceeds maximum allowed depth " +
                   std::to_string(m_config.maxReorgDepth) + ".");
  }

  if (depth > m_config.alertThresholdDepth) {
    return ChainReorgCheckResult::allowedWithAlert(
        depth, "Reorg depth " + std::to_string(depth) +
                   " exceeds alert threshold " +
                   std::to_string(m_config.alertThresholdDepth) + ".");
  }

  return ChainReorgCheckResult::allowed(depth, "Reorg depth " +
                                                   std::to_string(depth) +
                                                   " is within safe bounds.");
}

void ChainReorgGuard::recordReorgEvent(const ReorgEvent &event) {
  m_reorgHistory.push_back(event);

  if (event.outcome() == ReorgCheckOutcome::REJECTED) {
    ++m_totalRejected;
  } else if (event.outcome() == ReorgCheckOutcome::ALLOWED_WITH_ALERT) {
    ++m_totalAlerted;
  }
}

const std::vector<ReorgEvent> &ChainReorgGuard::reorgHistory() const {
  return m_reorgHistory;
}

std::size_t ChainReorgGuard::totalRejectedReorgs() const {
  return m_totalRejected;
}

std::size_t ChainReorgGuard::totalAlertedReorgs() const {
  return m_totalAlerted;
}

const ChainReorgGuardConfig &ChainReorgGuard::config() const {
  return m_config;
}

} // namespace nodo::consensus
