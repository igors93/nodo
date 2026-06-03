#include "node/EvidenceCaptureHealth.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

std::string evidenceCaptureStatusToString(EvidenceCaptureStatus status) {
    switch (status) {
        case EvidenceCaptureStatus::HEALTHY:           return "HEALTHY";
        case EvidenceCaptureStatus::STORE_UNAVAILABLE: return "STORE_UNAVAILABLE";
        case EvidenceCaptureStatus::PERSIST_FAILURE:   return "PERSIST_FAILURE";
        case EvidenceCaptureStatus::DISABLED:          return "DISABLED";
        default:                                       return "UNKNOWN";
    }
}

EvidenceCaptureHealth::EvidenceCaptureHealth()
    : m_status(EvidenceCaptureStatus::DISABLED),
      m_totalCaptured(0),
      m_totalPersistFailures(0),
      m_lastFailureAt(0),
      m_lastFailureReason("") {}

EvidenceCaptureHealth::EvidenceCaptureHealth(
    EvidenceCaptureStatus status,
    std::uint64_t totalCaptured,
    std::uint64_t totalPersistFailures,
    std::int64_t lastFailureAt,
    std::string lastFailureReason
)
    : m_status(status),
      m_totalCaptured(totalCaptured),
      m_totalPersistFailures(totalPersistFailures),
      m_lastFailureAt(lastFailureAt),
      m_lastFailureReason(std::move(lastFailureReason)) {}

EvidenceCaptureStatus EvidenceCaptureHealth::status() const {
    return m_status;
}

std::uint64_t EvidenceCaptureHealth::totalCaptured() const {
    return m_totalCaptured;
}

std::uint64_t EvidenceCaptureHealth::totalPersistFailures() const {
    return m_totalPersistFailures;
}

std::int64_t EvidenceCaptureHealth::lastFailureAt() const {
    return m_lastFailureAt;
}

const std::string& EvidenceCaptureHealth::lastFailureReason() const {
    return m_lastFailureReason;
}

bool EvidenceCaptureHealth::isHealthy() const {
    return m_status == EvidenceCaptureStatus::HEALTHY ||
           m_status == EvidenceCaptureStatus::DISABLED;
}

bool EvidenceCaptureHealth::hasRecentFailure(
    std::int64_t now,
    std::int64_t windowSeconds
) const {
    if (m_lastFailureAt <= 0) {
        return false;
    }
    return (now - m_lastFailureAt) < windowSeconds;
}

std::string EvidenceCaptureHealth::serialize() const {
    std::ostringstream oss;
    oss << "EvidenceCaptureHealth{"
        << "status=" << evidenceCaptureStatusToString(m_status)
        << ";totalCaptured=" << m_totalCaptured
        << ";totalPersistFailures=" << m_totalPersistFailures
        << ";lastFailureAt=" << m_lastFailureAt
        << ";lastFailureReason=" << m_lastFailureReason
        << "}";
    return oss.str();
}

void EvidenceCaptureHealth::recordSuccess() {
    m_status = EvidenceCaptureStatus::HEALTHY;
    ++m_totalCaptured;
}

void EvidenceCaptureHealth::recordFailure(
    const std::string& reason,
    std::int64_t now
) {
    m_status = EvidenceCaptureStatus::PERSIST_FAILURE;
    ++m_totalPersistFailures;
    m_lastFailureAt = now;
    m_lastFailureReason = reason.substr(0, 256);
}

void EvidenceCaptureHealth::markUnavailable() {
    m_status = EvidenceCaptureStatus::STORE_UNAVAILABLE;
}

} // namespace nodo::node
