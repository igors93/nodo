#ifndef NODO_CONSENSUS_EVIDENCE_POOL_HPP
#define NODO_CONSENSUS_EVIDENCE_POOL_HPP

#include "consensus/SlashingEvidence.hpp"
#include "consensus/EvidencePoolPersistence.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace nodo::consensus {

class EvidencePool {
public:
    EvidencePool();

    SlashingEvidenceValidationResult submitDoubleVoteEvidence(
        const DoubleVoteEvidence& evidence
    );

    void setPersistence(EvidencePoolPersistence* persistence);
    bool hasPersistence() const;

    bool contains(const std::string& evidenceId) const;

    const SlashingEvidenceRecord* evidenceById(
        const std::string& evidenceId
    ) const;

    const DoubleVoteEvidence* doubleVoteEvidenceById(
        const std::string& evidenceId
    ) const;

    std::vector<SlashingEvidenceRecord> allEvidence() const;

    std::vector<DoubleVoteEvidence> allDoubleVoteEvidence() const;

    std::vector<DoubleVoteEvidence> doubleVoteEvidenceBeforeHeight(
        std::uint64_t blockHeight
    ) const;

    bool removeEvidence(const std::string& evidenceId);

    std::vector<SlashingEvidenceRecord> evidenceForValidator(
        const std::string& validatorAddress
    ) const;

    std::size_t size() const;
    std::size_t countForValidator(const std::string& validatorAddress) const;

    // Minimum time (seconds) evidence must be retained to cover the unbonding
    // window before a validator can be slashed.  Callers must pass a `now`
    // value so the pool can enforce this floor internally.
    static constexpr std::int64_t kMinRetentionSeconds = 7 * 24 * 3600; // 7 days

    void pruneOlderThan(std::int64_t cutoffTimestamp, std::int64_t now);

    bool isValid() const;
    std::string serialize() const;

private:
    std::map<std::string, SlashingEvidenceRecord> m_evidenceById;
    std::map<std::string, DoubleVoteEvidence> m_doubleVoteEvidenceById;
    EvidencePoolPersistence* m_persistence;
};

} // namespace nodo::consensus

#endif
