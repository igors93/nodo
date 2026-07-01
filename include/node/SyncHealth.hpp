#ifndef NODO_NODE_SYNC_HEALTH_HPP
#define NODO_NODE_SYNC_HEALTH_HPP

#include <cstdint>
#include <string>

namespace nodo::node {

enum class SyncHealthStatus {
    HEALTHY,
    BATCH_FAILURE,
    REQUEST_FAILURE,
    SERVE_FAILURE,
    DISABLED
};

std::string syncHealthStatusToString(SyncHealthStatus status);

/*
 * SyncHealth tracks the operational health of the persistent block sync
 * pipeline. It distinguishes three failure domains:
 *
 *   BATCH_FAILURE   — a received BLOCK_SYNC_RESPONSE was rejected (bad batch,
 *                     failed validation, or QC verification error).
 *   REQUEST_FAILURE — a BLOCK_SYNC_REQUEST could not be sent or decoded.
 *   SERVE_FAILURE   — an inbound BLOCK_SYNC_REQUEST could not be served
 *                     (build or send error on the responder side).
 *
 * Failures are surfaced through diagnostics (operators and tests can observe
 * syncHealth()) without terminating the node or silently hiding problems.
 * A sync failure does not prevent consensus from running.
 */
class SyncHealth {
public:
    SyncHealth();

    SyncHealth(
        SyncHealthStatus status,
        std::uint64_t totalSynced,
        std::uint64_t totalFailures,
        std::int64_t lastFailureAt,
        std::string lastFailureReason
    );

    SyncHealthStatus status() const;
    std::uint64_t totalSynced() const;
    std::uint64_t totalFailures() const;
    std::int64_t lastFailureAt() const;
    const std::string& lastFailureReason() const;

    bool isHealthy() const;
    bool hasRecentFailure(std::int64_t now, std::int64_t windowSeconds) const;
    bool isStagnant(std::int64_t now, std::int64_t maxIdleSeconds) const;

    std::string serialize() const;

    // Record a successfully imported BLOCK_SYNC_RESPONSE batch.
    void recordSuccess(std::int64_t now);

    // Record a rejected or malformed BLOCK_SYNC_RESPONSE batch.
    void recordBatchFailure(const std::string& reason, std::int64_t now);

    // Record failure to send or process a BLOCK_SYNC_REQUEST.
    void recordRequestFailure(const std::string& reason, std::int64_t now);

    // Record failure to build or send a BLOCK_SYNC_RESPONSE (server side).
    void recordServeFailure(const std::string& reason, std::int64_t now);

private:
    void recordFailure(SyncHealthStatus kind, const std::string& reason, std::int64_t now);

    SyncHealthStatus m_status;
    std::uint64_t m_totalSynced;
    std::uint64_t m_totalFailures;
    std::int64_t m_lastFailureAt;
    std::int64_t m_lastSuccessAt;
    std::string m_lastFailureReason;
};

} // namespace nodo::node

#endif
