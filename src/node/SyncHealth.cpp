#include "node/SyncHealth.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

std::string syncHealthStatusToString(SyncHealthStatus status) {
  switch (status) {
  case SyncHealthStatus::HEALTHY:
    return "HEALTHY";
  case SyncHealthStatus::BATCH_FAILURE:
    return "BATCH_FAILURE";
  case SyncHealthStatus::REQUEST_FAILURE:
    return "REQUEST_FAILURE";
  case SyncHealthStatus::SERVE_FAILURE:
    return "SERVE_FAILURE";
  case SyncHealthStatus::DISABLED:
    return "DISABLED";
  default:
    return "UNKNOWN";
  }
}

SyncHealth::SyncHealth()
    : m_status(SyncHealthStatus::DISABLED), m_totalSynced(0),
      m_totalFailures(0), m_lastFailureAt(0), m_lastSuccessAt(0),
      m_lastFailureReason() {}

SyncHealth::SyncHealth(SyncHealthStatus status, std::uint64_t totalSynced,
                       std::uint64_t totalFailures, std::int64_t lastFailureAt,
                       std::string lastFailureReason)
    : m_status(status), m_totalSynced(totalSynced),
      m_totalFailures(totalFailures), m_lastFailureAt(lastFailureAt),
      m_lastSuccessAt(0), m_lastFailureReason(std::move(lastFailureReason)) {}

SyncHealthStatus SyncHealth::status() const { return m_status; }
std::uint64_t SyncHealth::totalSynced() const { return m_totalSynced; }
std::uint64_t SyncHealth::totalFailures() const { return m_totalFailures; }
std::int64_t SyncHealth::lastFailureAt() const { return m_lastFailureAt; }
const std::string &SyncHealth::lastFailureReason() const {
  return m_lastFailureReason;
}

bool SyncHealth::isHealthy() const {
  return m_status == SyncHealthStatus::HEALTHY ||
         m_status == SyncHealthStatus::DISABLED;
}

bool SyncHealth::hasRecentFailure(std::int64_t now,
                                  std::int64_t windowSeconds) const {
  if (m_lastFailureAt <= 0 || now < m_lastFailureAt) {
    return false;
  }
  return (now - m_lastFailureAt) <= windowSeconds;
}

bool SyncHealth::isStagnant(std::int64_t now,
                            std::int64_t maxIdleSeconds) const {
  if (m_lastSuccessAt <= 0 || now < m_lastSuccessAt) {
    return false;
  }
  return (now - m_lastSuccessAt) > maxIdleSeconds;
}

std::string SyncHealth::serialize() const {
  std::ostringstream oss;
  oss << "SyncHealth{"
      << "status=" << syncHealthStatusToString(m_status)
      << ";totalSynced=" << m_totalSynced
      << ";totalFailures=" << m_totalFailures
      << ";lastFailureAt=" << m_lastFailureAt
      << ";lastFailureReason=" << m_lastFailureReason << "}";
  return oss.str();
}

void SyncHealth::recordSuccess(std::int64_t now) {
  m_status = SyncHealthStatus::HEALTHY;
  ++m_totalSynced;
  m_lastSuccessAt = now;
}

void SyncHealth::recordBatchFailure(const std::string &reason,
                                    std::int64_t now) {
  recordFailure(SyncHealthStatus::BATCH_FAILURE, reason, now);
}

void SyncHealth::recordRequestFailure(const std::string &reason,
                                      std::int64_t now) {
  recordFailure(SyncHealthStatus::REQUEST_FAILURE, reason, now);
}

void SyncHealth::recordServeFailure(const std::string &reason,
                                    std::int64_t now) {
  recordFailure(SyncHealthStatus::SERVE_FAILURE, reason, now);
}

void SyncHealth::recordFailure(SyncHealthStatus kind, const std::string &reason,
                               std::int64_t now) {
  m_status = kind;
  ++m_totalFailures;
  m_lastFailureAt = now;
  m_lastFailureReason = reason.substr(0, 256);
}

} // namespace nodo::node
