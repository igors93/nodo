#ifndef NODO_CONSENSUS_EVIDENCE_POOL_HPP
#define NODO_CONSENSUS_EVIDENCE_POOL_HPP

#include "consensus/SlashingEvidence.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace nodo::consensus {

class EvidencePool {
public:
    EvidencePool();

    SlashingEvidenceValidationResult submitRecord(
        const SlashingEvidenceRecord& record
    );

    SlashingEvidenceValidationResult submitDoubleVoteEvidence(
        const DoubleVoteEvidence& evidence
    );

    bool contains(const std::string& evidenceId) const;

    const SlashingEvidenceRecord* evidenceById(
        const std::string& evidenceId
    ) const;

    std::vector<SlashingEvidenceRecord> allEvidence() const;

    std::vector<SlashingEvidenceRecord> evidenceForValidator(
        const std::string& validatorAddress
    ) const;

    std::size_t size() const;
    std::size_t countForValidator(const std::string& validatorAddress) const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::map<std::string, SlashingEvidenceRecord> m_evidenceById;
};

} // namespace nodo::consensus

#endif
