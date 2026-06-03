#ifndef NODO_NODE_EVIDENCE_CAPTURE_HEALTH_HPP
#define NODO_NODE_EVIDENCE_CAPTURE_HEALTH_HPP

#include <cstdint>
#include <string>

namespace nodo::node {

enum class EvidenceCaptureStatus {
    HEALTHY,
    STORE_UNAVAILABLE,
    PERSIST_FAILURE,
    DISABLED
};

std::string evidenceCaptureStatusToString(EvidenceCaptureStatus status);

/*
 * EvidenceCaptureHealth tracks whether protocol evidence is being captured
 * and persisted successfully. A node whose evidence store has failures is
 * still safe to operate, but the operator must be informed so the store can
 * be repaired before evidence is silently lost.
 *
 * Security principle:
 * Invalid peer messages must still be rejected even when evidence persistence
 * fails. Capture failures are surfaced through diagnostics without crashing
 * the mesh or silently ignoring the underlying problem.
 */
class EvidenceCaptureHealth {
public:
    EvidenceCaptureHealth();

    EvidenceCaptureHealth(
        EvidenceCaptureStatus status,
        std::uint64_t totalCaptured,
        std::uint64_t totalPersistFailures,
        std::int64_t lastFailureAt,
        std::string lastFailureReason
    );

    EvidenceCaptureStatus status() const;
    std::uint64_t totalCaptured() const;
    std::uint64_t totalPersistFailures() const;
    std::int64_t lastFailureAt() const;
    const std::string& lastFailureReason() const;

    bool isHealthy() const;
    bool hasRecentFailure(std::int64_t now, std::int64_t windowSeconds) const;

    std::string serialize() const;

    // Record a successful evidence persistence.
    void recordSuccess();

    // Record a persistence failure.
    void recordFailure(const std::string& reason, std::int64_t now);

    // Mark the evidence store as unavailable (store pointer is null).
    void markUnavailable();

private:
    EvidenceCaptureStatus m_status;
    std::uint64_t m_totalCaptured;
    std::uint64_t m_totalPersistFailures;
    std::int64_t m_lastFailureAt;
    std::string m_lastFailureReason;
};

} // namespace nodo::node

#endif
