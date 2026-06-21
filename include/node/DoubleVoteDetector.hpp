#ifndef NODO_NODE_DOUBLE_VOTE_DETECTOR_HPP
#define NODO_NODE_DOUBLE_VOTE_DETECTOR_HPP

#include "consensus/EvidencePool.hpp"
#include "consensus/VotePool.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "crypto/CryptoPolicy.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

struct DoubleVoteDetectionResult {
    std::size_t evidenceSubmitted = 0;
    std::size_t duplicatesSkipped = 0;
    std::vector<std::string> newEvidenceIds;
};

/*
 * DoubleVoteDetector scans the VotePool for conflicting votes (same validator,
 * same height+round, different block hashes) and submits them to the EvidencePool.
 *
 * This wires the consensus safety check into the slashing pipeline.
 * Call once per tick after vote collection.
 *
 * Security principle: evidence is idempotent — duplicate evidence is ignored
 * by EvidencePool. This detector is safe to call repeatedly.
 */
class DoubleVoteDetector {
public:
    /*
     * Scan the VotePool for double-votes and submit any found to the EvidencePool.
     * For each conflicting vote, pairs it with the original vote it conflicts with
     * by matching validator+height+round. Returns a result describing what was
     * submitted and what was skipped as duplicates.
     */
    static DoubleVoteDetectionResult detect(
        const consensus::VotePool&  votePool,
        consensus::EvidencePool&    evidencePool,
        const crypto::CryptoPolicy& policy,
        std::int64_t                now
    );

private:
    /*
     * Returns true if two votes constitute a double-vote pair: same validator,
     * same height, same round, but different block hash. This is the slashable
     * equivocation condition.
     */
    static bool isDoubleVotePair(
        const consensus::ValidatorVoteRecord& a,
        const consensus::ValidatorVoteRecord& b
    );
};

} // namespace nodo::node

#endif
