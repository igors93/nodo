#ifndef NODO_NODE_PERSISTENT_SLASHING_EVIDENCE_POOL_HPP
#define NODO_NODE_PERSISTENT_SLASHING_EVIDENCE_POOL_HPP

#include "consensus/EvidencePool.hpp"
#include "consensus/ValidatorPenaltyApplication.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nodo::storage {
class SlashingEvidenceStore;
}

namespace nodo::node {

enum class PersistentSlashingEvidencePoolLoadStatus {
    LOADED,
    INVALID_ARGUMENT,
    INVALID_EVIDENCE,
    IO_ERROR
};

class PersistentSlashingEvidencePoolLoadResult {
public:
    static PersistentSlashingEvidencePoolLoadResult loaded(
        std::size_t loadedEvidenceCount,
        std::size_t removedFinalizedEvidenceCount
    );

    static PersistentSlashingEvidencePoolLoadResult rejected(
        PersistentSlashingEvidencePoolLoadStatus status,
        std::string reason
    );

    PersistentSlashingEvidencePoolLoadStatus status() const;
    const std::string& reason() const;
    std::size_t loadedEvidenceCount() const;
    std::size_t removedFinalizedEvidenceCount() const;
    bool success() const;

private:
    PersistentSlashingEvidencePoolLoadStatus m_status =
        PersistentSlashingEvidencePoolLoadStatus::INVALID_ARGUMENT;
    std::string m_reason;
    std::size_t m_loadedEvidenceCount = 0;
    std::size_t m_removedFinalizedEvidenceCount = 0;
};

class PersistentSlashingEvidencePool {
public:
    static PersistentSlashingEvidencePoolLoadResult restore(
        consensus::EvidencePool& evidencePool,
        storage::SlashingEvidenceStore& store,
        std::uint64_t currentConsensusHeight,
        std::int64_t now,
        const core::ValidatorSetHistory& validatorSetHistory,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        const consensus::ValidatorPenaltyLedger& penaltyLedger
    );
};

} // namespace nodo::node

#endif
