#ifndef NODO_CONSENSUS_EVIDENCE_POOL_PERSISTENCE_HPP
#define NODO_CONSENSUS_EVIDENCE_POOL_PERSISTENCE_HPP

#include "consensus/SlashingEvidence.hpp"

#include <string>

namespace nodo::consensus {

class EvidencePoolPersistence {
public:
    virtual ~EvidencePoolPersistence() = default;

    virtual void persist(
        const DoubleVoteEvidence& evidence
    ) = 0;

    virtual void persist(
        const ProposerEquivocationEvidence& evidence
    ) = 0;

    virtual bool erase(
        const std::string& evidenceId
    ) = 0;
};

} // namespace nodo::consensus

#endif
