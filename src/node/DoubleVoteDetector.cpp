#include "node/DoubleVoteDetector.hpp"

#include "consensus/SlashingEvidence.hpp"

namespace nodo::node {

DoubleVoteDetectionResult DoubleVoteDetector::detect(
    const consensus::VotePool&  votePool,
    consensus::EvidencePool&    evidencePool,
    const crypto::CryptoPolicy& /*policy*/,
    std::int64_t                now
) {
    DoubleVoteDetectionResult result;

    const auto& conflicts = votePool.conflictingVotes();

    for (const consensus::ValidatorVoteRecord& conflicting : conflicts) {
        // Look up the original vote that this one conflicts with.
        // The original is stored in the VotePool keyed by validator+height+round.
        const consensus::ValidatorVoteRecord* original =
            votePool.firstVoteForValidator(
                conflicting.validatorAddress(),
                conflicting.blockIndex(),
                conflicting.round()
            );

        if (!original) {
            // Original vote no longer accessible — skip silently.
            continue;
        }

        // Sanity check: must be a true double-vote pair before submitting.
        if (!isDoubleVotePair(*original, conflicting)) {
            continue;
        }

        consensus::DoubleVoteEvidence evidence(*original, conflicting, now);

        // Guard against re-submitting evidence we already have.
        if (evidencePool.contains(evidence.evidenceId())) {
            result.duplicatesSkipped++;
            continue;
        }

        const consensus::SlashingEvidenceValidationResult submitResult =
            evidencePool.submitDoubleVoteEvidence(evidence);

        if (submitResult.accepted()) {
            result.evidenceSubmitted++;
            result.newEvidenceIds.push_back(submitResult.record().evidenceId());
        } else if (submitResult.duplicate()) {
            result.duplicatesSkipped++;
        }
        // Rejected evidence is silently dropped — it means the evidence was
        // structurally invalid, which shouldn't happen if VotePool is correct.
    }

    return result;
}

bool DoubleVoteDetector::isDoubleVotePair(
    const consensus::ValidatorVoteRecord& a,
    const consensus::ValidatorVoteRecord& b
) {
    // Double-vote: same validator, same height+round, but different block hash.
    return a.validatorAddress() == b.validatorAddress()
        && a.blockIndex()       == b.blockIndex()
        && a.round()            == b.round()
        && a.blockHash()        != b.blockHash();
}

} // namespace nodo::node
