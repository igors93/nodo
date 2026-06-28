#ifndef NODO_NODE_VERIFIED_SLASHING_EVIDENCE_ADMISSION_HPP
#define NODO_NODE_VERIFIED_SLASHING_EVIDENCE_ADMISSION_HPP

#include "consensus/EvidencePool.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>

namespace nodo::node {

class VerifiedSlashingEvidenceAdmission {
public:
    static consensus::SlashingEvidenceValidationResult admit(
        const consensus::DoubleVoteEvidence& evidence,
        std::uint64_t currentConsensusHeight,
        std::int64_t now,
        const core::ValidatorSetHistory& validatorSetHistory,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        consensus::EvidencePool& evidencePool
    );
};

} // namespace nodo::node

#endif
