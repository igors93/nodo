#include "node/ParallelBlockSync.hpp"

#include "core/Block.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace nodo::node {

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

std::string syncPhaseToString(SyncPhase phase) {
    switch (phase) {
        case SyncPhase::IDLE:               return "IDLE";
        case SyncPhase::DOWNLOADING_HEADERS: return "DOWNLOADING_HEADERS";
        case SyncPhase::VERIFYING_HEADERS:  return "VERIFYING_HEADERS";
        case SyncPhase::DOWNLOADING_BODIES: return "DOWNLOADING_BODIES";
        case SyncPhase::APPLYING_BLOCKS:    return "APPLYING_BLOCKS";
        case SyncPhase::SYNCED:             return "SYNCED";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// SyncProgress
// ---------------------------------------------------------------------------

std::uint64_t SyncProgress::blocksRemaining() const {
    if (targetHeight <= localHeight) {
        return 0;
    }
    return targetHeight - localHeight - blocksApplied;
}

bool SyncProgress::isComplete() const {
    return phase == SyncPhase::SYNCED;
}

std::string SyncProgress::serialize() const {
    std::ostringstream oss;
    oss << "SyncProgress{"
        << "phase=" << syncPhaseToString(phase)
        << ";localHeight=" << localHeight
        << ";targetHeight=" << targetHeight
        << ";headersDownloaded=" << headersDownloaded
        << ";bodiesDownloaded=" << bodiesDownloaded
        << ";blocksApplied=" << blocksApplied
        << ";startedAt=" << startedAt
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// SyncCheckpoint
// ---------------------------------------------------------------------------

bool SyncCheckpoint::isValid() const {
    return height > 0 && !blockHash.empty();
}

std::string SyncCheckpoint::serialize() const {
    std::ostringstream oss;
    oss << "SyncCheckpoint{"
        << "height=" << height
        << ";blockHash=" << blockHash
        << ";timestamp=" << timestamp
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// ParallelBlockSync
// ---------------------------------------------------------------------------

ParallelBlockSync::ParallelBlockSync(std::vector<SyncCheckpoint> knownCheckpoints)
    : m_checkpoints(std::move(knownCheckpoints))
    , m_progress{SyncPhase::IDLE, 0, 0, 0, 0, 0, 0}
{}

SyncProgress ParallelBlockSync::beginSync(
    std::uint64_t localHeight,
    std::uint64_t targetHeight,
    std::int64_t  now
) {
    m_progress.phase             = SyncPhase::DOWNLOADING_HEADERS;
    m_progress.localHeight       = localHeight;
    m_progress.targetHeight      = targetHeight;
    m_progress.headersDownloaded = 0;
    m_progress.bodiesDownloaded  = 0;
    m_progress.blocksApplied     = 0;
    m_progress.startedAt         = now;

    m_pendingHeaders.clear();
    m_pendingBodies.clear();
    m_verifiedHeaders.clear();

    return m_progress;
}

bool ParallelBlockSync::onHeadersReceived(
    const std::vector<std::string>& serializedHeaders,
    std::uint64_t fromHeight,
    const std::string& /*peerId*/
) {
    if (serializedHeaders.empty()) {
        return false;
    }

    // Headers can arrive in DOWNLOADING_HEADERS, VERIFYING_HEADERS, or
    // DOWNLOADING_BODIES phases (headers and bodies may be fetched in parallel).
    if (m_progress.phase == SyncPhase::IDLE ||
        m_progress.phase == SyncPhase::APPLYING_BLOCKS ||
        m_progress.phase == SyncPhase::SYNCED) {
        return false;
    }

    // Deserialize headers and validate hash linkage + checkpoint matches
    std::uint64_t height = fromHeight;
    for (const auto& serializedHeader : serializedHeaders) {
        const auto blockOpt = nodo::core::Block::deserialize(serializedHeader);
        if (!blockOpt.has_value()) {
            return false;
        }

        const auto& block = blockOpt.value();

        // Validate that the block index matches the expected height
        if (block.index() != height) {
            return false;
        }

        // Checkpoint validation: if this height has a known checkpoint,
        // the block hash must match.
        if (matchesCheckpoint(height, block.hash()) == false) {
            // Check if there IS a checkpoint at this height that conflicts
            for (const auto& cp : m_checkpoints) {
                if (cp.height == height && cp.blockHash != block.hash()) {
                    return false;
                }
            }
        }

        const bool isNew = m_pendingHeaders.find(height) == m_pendingHeaders.end();
        m_pendingHeaders[height] = serializedHeader;
        m_verifiedHeaders[height] = block.hash();
        if (isNew) {
            ++m_progress.headersDownloaded;
        }
        ++height;
    }

    updatePhase();
    return true;
}

bool ParallelBlockSync::onBodyReceived(
    const std::string& serializedBlock,
    std::uint64_t height,
    const std::string& /*peerId*/
) {
    // Only accept bodies for heights with verified headers
    if (m_verifiedHeaders.find(height) == m_verifiedHeaders.end()) {
        return false;
    }

    const auto blockOpt = nodo::core::Block::deserialize(serializedBlock);
    if (!blockOpt.has_value()) {
        return false;
    }

    // Verify the body hash matches the previously committed header hash
    if (blockOpt.value().hash() != m_verifiedHeaders.at(height)) {
        return false;
    }

    const bool isNew = m_pendingBodies.find(height) == m_pendingBodies.end();
    m_pendingBodies[height] = serializedBlock;
    if (isNew) {
        ++m_progress.bodiesDownloaded;
    }

    updatePhase();
    return true;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> ParallelBlockSync::pendingBodyRanges() const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;

    if (m_verifiedHeaders.empty()) {
        return ranges;
    }

    // Find verified headers that don't yet have a body
    std::vector<std::uint64_t> missing;
    for (const auto& [h, _hash] : m_verifiedHeaders) {
        if (m_pendingBodies.find(h) == m_pendingBodies.end()) {
            missing.push_back(h);
        }
    }

    if (missing.empty()) {
        return ranges;
    }

    // Build contiguous ranges, up to MAX_PARALLEL_REQUESTS batches
    std::size_t idx = 0;
    while (idx < missing.size() && ranges.size() < MAX_PARALLEL_REQUESTS) {
        const std::uint64_t rangeStart = missing[idx];
        std::uint64_t rangeEnd = rangeStart;
        std::size_t count = 1;

        while (idx + count < missing.size() &&
               count < BODIES_PER_REQUEST &&
               missing[idx + count] == rangeEnd + 1) {
            ++rangeEnd;
            ++count;
        }

        ranges.emplace_back(rangeStart, rangeEnd);
        idx += count;
    }

    return ranges;
}

std::vector<std::string> ParallelBlockSync::drainReadyBlocks() {
    std::vector<std::string> ready;

    // Collect heights where both header and body are available, in ascending order.
    // Determine where to start: the lowest height in verifiedHeaders that is at or
    // above localHeight, offset by how many we have already drained this session.
    std::uint64_t nextExpected = m_progress.localHeight + m_progress.blocksApplied;

    // If verifiedHeaders doesn't contain nextExpected but contains nextExpected+1 or
    // more, this means localHeight was 0 and the first block is at 0, not 1.
    // Scan down: if nextExpected is not present but nextExpected-1 is, use that.
    // Simpler: find the minimum key >= localHeight in verifiedHeaders that is contiguous.
    if (!m_verifiedHeaders.empty()) {
        const std::uint64_t lowestVerified = m_verifiedHeaders.begin()->first;
        // If we haven't applied anything yet and the lowest block is below nextExpected,
        // start from there to handle the case where localHeight=0 means "start from 0".
        if (m_progress.blocksApplied == 0 && lowestVerified < nextExpected) {
            nextExpected = lowestVerified;
        }
    }

    while (true) {
        const bool headerPresent = m_verifiedHeaders.find(nextExpected) != m_verifiedHeaders.end();
        const bool bodyPresent   = m_pendingBodies.find(nextExpected) != m_pendingBodies.end();

        if (!headerPresent || !bodyPresent) {
            break;
        }

        ready.push_back(m_pendingBodies.at(nextExpected));

        m_verifiedHeaders.erase(nextExpected);
        m_pendingHeaders.erase(nextExpected);
        m_pendingBodies.erase(nextExpected);

        ++m_progress.blocksApplied;
        ++nextExpected;
    }

    updatePhase();
    return ready;
}

SyncProgress ParallelBlockSync::progress() const {
    return m_progress;
}

bool ParallelBlockSync::matchesCheckpoint(
    std::uint64_t height,
    const std::string& blockHash
) const {
    for (const auto& cp : m_checkpoints) {
        if (cp.height == height) {
            return cp.blockHash == blockHash;
        }
    }
    // No checkpoint at this height — considered a match (no constraint)
    return true;
}

void ParallelBlockSync::reset() {
    m_progress = {SyncPhase::IDLE, 0, 0, 0, 0, 0, 0};
    m_pendingHeaders.clear();
    m_pendingBodies.clear();
    m_verifiedHeaders.clear();
}

void ParallelBlockSync::updatePhase() {
    if (m_progress.phase == SyncPhase::IDLE) {
        return;
    }

    const std::uint64_t totalNeeded =
        (m_progress.targetHeight >= m_progress.localHeight)
        ? (m_progress.targetHeight - m_progress.localHeight)
        : 0;

    if (totalNeeded == 0) {
        m_progress.phase = SyncPhase::SYNCED;
        return;
    }

    if (m_progress.blocksApplied >= totalNeeded) {
        m_progress.phase = SyncPhase::SYNCED;
        return;
    }

    if (m_progress.bodiesDownloaded > 0 ||
        !m_pendingBodies.empty()) {
        m_progress.phase = SyncPhase::DOWNLOADING_BODIES;
        return;
    }

    if (m_progress.headersDownloaded > 0 ||
        !m_verifiedHeaders.empty()) {
        m_progress.phase = SyncPhase::DOWNLOADING_BODIES;
        return;
    }

    m_progress.phase = SyncPhase::DOWNLOADING_HEADERS;
}

} // namespace nodo::node
