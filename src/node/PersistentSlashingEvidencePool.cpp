#include "node/PersistentSlashingEvidencePool.hpp"

#include "core/ProtocolLimits.hpp"
#include "storage/SlashingEvidenceStore.hpp"

#include <exception>
#include <filesystem>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

namespace nodo::node {

PersistentSlashingEvidencePoolLoadResult
PersistentSlashingEvidencePoolLoadResult::loaded(
    std::size_t loadedEvidenceCount,
    std::size_t removedFinalizedEvidenceCount
) {
    PersistentSlashingEvidencePoolLoadResult result;
    result.m_status = PersistentSlashingEvidencePoolLoadStatus::LOADED;
    result.m_reason = "Pending slashing evidence pool restored.";
    result.m_loadedEvidenceCount = loadedEvidenceCount;
    result.m_removedFinalizedEvidenceCount = removedFinalizedEvidenceCount;
    return result;
}

PersistentSlashingEvidencePoolLoadResult
PersistentSlashingEvidencePoolLoadResult::rejected(
    PersistentSlashingEvidencePoolLoadStatus status,
    std::string reason
) {
    PersistentSlashingEvidencePoolLoadResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

PersistentSlashingEvidencePoolLoadStatus
PersistentSlashingEvidencePoolLoadResult::status() const {
    return m_status;
}

const std::string& PersistentSlashingEvidencePoolLoadResult::reason() const {
    return m_reason;
}

std::size_t
PersistentSlashingEvidencePoolLoadResult::loadedEvidenceCount() const {
    return m_loadedEvidenceCount;
}

std::size_t
PersistentSlashingEvidencePoolLoadResult::removedFinalizedEvidenceCount() const {
    return m_removedFinalizedEvidenceCount;
}

bool PersistentSlashingEvidencePoolLoadResult::success() const {
    return m_status == PersistentSlashingEvidencePoolLoadStatus::LOADED;
}

PersistentSlashingEvidencePoolLoadResult
PersistentSlashingEvidencePool::restore(
    consensus::EvidencePool& evidencePool,
    storage::SlashingEvidenceStore& store,
    std::uint64_t currentConsensusHeight,
    std::int64_t now,
    const core::ValidatorSetHistory& validatorSetHistory,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    const consensus::ValidatorPenaltyLedger& penaltyLedger
) {
    if (currentConsensusHeight == 0 || now <= 0 ||
        evidencePool.size() != 0 || evidencePool.hasPersistence()) {
        return PersistentSlashingEvidencePoolLoadResult::rejected(
            PersistentSlashingEvidencePoolLoadStatus::INVALID_ARGUMENT,
            "Persistent evidence restore requires an empty, unbound pool."
        );
    }

    std::vector<consensus::DoubleVoteEvidence> storedEvidence;
    std::error_code directoryError;
    std::filesystem::create_directories(
        store.evidenceDirectory(), directoryError
    );
    if (directoryError) {
        return PersistentSlashingEvidencePoolLoadResult::rejected(
            PersistentSlashingEvidencePoolLoadStatus::IO_ERROR,
            "Pending slashing evidence directory could not be initialized."
        );
    }
    try {
        storedEvidence = store.loadAll();
    } catch (const std::exception&) {
        return PersistentSlashingEvidencePoolLoadResult::rejected(
            PersistentSlashingEvidencePoolLoadStatus::IO_ERROR,
            "Pending slashing evidence files could not be loaded canonically."
        );
    }

    evidencePool.setPersistence(&store);

    std::size_t loadedCount = 0;
    std::size_t removedFinalizedCount = 0;
    for (const consensus::DoubleVoteEvidence& evidence : storedEvidence) {
        if (penaltyLedger.containsEvidence(evidence.evidenceId())) {
            if (!store.erase(evidence.evidenceId())) {
                return PersistentSlashingEvidencePoolLoadResult::rejected(
                    PersistentSlashingEvidencePoolLoadStatus::IO_ERROR,
                    "Finalized slashing evidence could not be removed from disk."
                );
            }
            ++removedFinalizedCount;
            continue;
        }

        const std::int64_t maximumVoteTimestamp =
            now > std::numeric_limits<std::int64_t>::max() -
                    core::ProtocolLimits::MAX_SLASHING_EVIDENCE_CLOCK_SKEW_SECONDS
                ? std::numeric_limits<std::int64_t>::max()
                : now +
                    core::ProtocolLimits::MAX_SLASHING_EVIDENCE_CLOCK_SKEW_SECONDS;
        if (evidence.detectedAt() > maximumVoteTimestamp ||
            evidence.firstVote().createdAt() > maximumVoteTimestamp ||
            evidence.secondVote().createdAt() > maximumVoteTimestamp) {
            return PersistentSlashingEvidencePoolLoadResult::rejected(
                PersistentSlashingEvidencePoolLoadStatus::INVALID_EVIDENCE,
                "Stored slashing evidence contains a future timestamp."
            );
        }

        const consensus::SlashingEvidenceValidationResult verified =
            consensus::SlashingEvidenceVerifier::
                verifyDoubleVoteEvidenceForHistory(
                    evidence,
                    currentConsensusHeight,
                    validatorSetHistory,
                    policy,
                    provider
                );
        if (!verified.accepted()) {
            return PersistentSlashingEvidencePoolLoadResult::rejected(
                PersistentSlashingEvidencePoolLoadStatus::INVALID_EVIDENCE,
                "Stored slashing evidence failed verification: " +
                    verified.reason()
            );
        }

        const consensus::SlashingEvidenceValidationResult inserted =
            evidencePool.submitDoubleVoteEvidence(evidence);
        if (!inserted.accepted()) {
            return PersistentSlashingEvidencePoolLoadResult::rejected(
                PersistentSlashingEvidencePoolLoadStatus::INVALID_EVIDENCE,
                "Stored slashing evidence could not be restored: " +
                    inserted.reason()
            );
        }
        ++loadedCount;
    }

    return PersistentSlashingEvidencePoolLoadResult::loaded(
        loadedCount, removedFinalizedCount
    );
}

} // namespace nodo::node
