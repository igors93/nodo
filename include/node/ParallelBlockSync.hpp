#ifndef NODO_NODE_PARALLEL_BLOCK_SYNC_HPP
#define NODO_NODE_PARALLEL_BLOCK_SYNC_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace nodo::node {

enum class SyncPhase {
    IDLE,
    DOWNLOADING_HEADERS,
    VERIFYING_HEADERS,
    DOWNLOADING_BODIES,
    APPLYING_BLOCKS,
    SYNCED
};

std::string syncPhaseToString(SyncPhase phase);

struct SyncProgress {
    SyncPhase phase;
    std::uint64_t localHeight;
    std::uint64_t targetHeight;
    std::uint64_t headersDownloaded;
    std::uint64_t bodiesDownloaded;
    std::uint64_t blocksApplied;
    std::int64_t startedAt;

    std::uint64_t blocksRemaining() const;
    bool isComplete() const;
    std::string serialize() const;
};

struct SyncCheckpoint {
    std::uint64_t height;
    std::string blockHash;
    std::int64_t timestamp;

    bool isValid() const;
    std::string serialize() const;
};

/*
 * ParallelBlockSync implements a two-phase IBD (initial block download) protocol.
 *
 * Phase 1 — Headers-first:
 *   Request block headers in batches from multiple peers. Headers are much
 *   lighter than full blocks, so the chain of hashes can be verified quickly
 *   without downloading transaction bodies.
 *
 * Phase 2 — Parallel body download:
 *   Request block bodies (transactions) in parallel from different peers,
 *   using the verified header chain as the authority.
 *
 * Checkpoint anchoring:
 *   Known-good checkpoints (hardcoded or from config) let a new node skip
 *   verification of very old blocks, trusting only the checkpoint hash.
 */
class ParallelBlockSync {
public:
    static constexpr std::size_t HEADERS_PER_REQUEST   = 500;
    static constexpr std::size_t BODIES_PER_REQUEST    = 50;
    static constexpr std::size_t MAX_PARALLEL_REQUESTS = 4;

    explicit ParallelBlockSync(std::vector<SyncCheckpoint> knownCheckpoints = {});

    // Begin a sync session. Returns initial SyncProgress.
    SyncProgress beginSync(
        std::uint64_t localHeight,
        std::uint64_t targetHeight,
        std::int64_t  now
    );

    // Called when a header batch arrives from a peer.
    // Validates the header chain (hash linkage + checkpoint match).
    // Returns true if headers were accepted.
    bool onHeadersReceived(
        const std::vector<std::string>& serializedHeaders,
        std::uint64_t fromHeight,
        const std::string& peerId
    );

    // Called when a block body arrives from a peer.
    // Returns true if the body matches a pending header.
    bool onBodyReceived(
        const std::string& serializedBlock,
        std::uint64_t height,
        const std::string& peerId
    );

    // Returns the next batches of heights to request bodies for
    // (up to MAX_PARALLEL_REQUESTS batches).
    std::vector<std::pair<std::uint64_t, std::uint64_t>> pendingBodyRanges() const;

    // Returns blocks ready to apply (headers verified + bodies received), in order.
    std::vector<std::string> drainReadyBlocks();

    // Current progress snapshot.
    SyncProgress progress() const;

    // Check if a given height matches a known checkpoint.
    bool matchesCheckpoint(std::uint64_t height, const std::string& blockHash) const;

    void reset();

private:
    std::vector<SyncCheckpoint> m_checkpoints;
    SyncProgress m_progress;
    std::map<std::uint64_t, std::string> m_pendingHeaders;   // height → serialized header
    std::map<std::uint64_t, std::string> m_pendingBodies;    // height → serialized block
    std::map<std::uint64_t, std::string> m_verifiedHeaders;  // height → block hash
    std::string m_lastAppliedHeaderHash;

    void updatePhase();
};

} // namespace nodo::node

#endif
