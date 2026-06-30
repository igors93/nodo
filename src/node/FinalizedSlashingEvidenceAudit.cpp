#include "node/FinalizedSlashingEvidenceAudit.hpp"

#include "core/ProtocolLimits.hpp"
#include "node/CanonicalSlashingTransition.hpp"

#include <algorithm>
#include <exception>
#include <set>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

FinalizedSlashingEvidenceAuditResult auditEvidenceRecord(
    const consensus::SlashingEvidenceRecord& record,
    std::int64_t blockTimestamp,
    const consensus::ValidatorPenaltyLedger& penaltyLedger,
    const core::ValidatorRegistry& validators,
    const StakingRegistry& staking
) {
    if (!record.isValid()) {
        return FinalizedSlashingEvidenceAuditResult::failed(
            "Finalized block contains invalid slashing evidence record."
        );
    }

    const consensus::ValidatorPenaltyDecision* decision =
        penaltyLedger.decisionByEvidenceId(record.evidenceId());
    if (decision == nullptr) {
        return FinalizedSlashingEvidenceAuditResult::failed(
            "Finalized slashing evidence has no deterministic penalty decision: " +
            record.evidenceId()
        );
    }

    if (!decision->isValid() ||
        decision->evidenceId() != record.evidenceId() ||
        decision->validatorAddress() != record.validatorAddress() ||
        decision->evidenceType() != record.type() ||
        decision->evidenceSeverity() != record.severity() ||
        decision->createdAt() != blockTimestamp) {
        return FinalizedSlashingEvidenceAuditResult::failed(
            "Finalized slashing penalty decision is not canonically bound to evidence: " +
            record.evidenceId()
        );
    }

    const core::ValidatorRegistryEntry* entry =
        validators.entryForAddress(decision->validatorAddress());
    if (entry == nullptr) {
        return FinalizedSlashingEvidenceAuditResult::failed(
            "Penalized validator is missing from the finalized validator registry."
        );
    }

    const economics::StakeAccount* stakeAccount =
        staking.accountFor(decision->validatorAddress());
    if (stakeAccount == nullptr) {
        return FinalizedSlashingEvidenceAuditResult::failed(
            "Penalized validator is missing from the finalized staking registry."
        );
    }

    const std::int64_t totalSlashRaw =
        penaltyLedger.totalSlashAmountForValidator(decision->validatorAddress());
    const std::int64_t expectedSlashedRaw = std::min(
        std::max<std::int64_t>(totalSlashRaw, 0),
        stakeAccount->bondedAmount().rawUnits()
    );
    if (stakeAccount->slashedAmount().rawUnits() != expectedSlashedRaw) {
        return FinalizedSlashingEvidenceAuditResult::failed(
            "Finalized staking registry does not mirror the bounded slash total."
        );
    }

    if (decision->tombstonesValidator()) {
        if (!entry->exited() || !stakeAccount->tombstoned()) {
            return FinalizedSlashingEvidenceAuditResult::failed(
                "Finalized tombstone penalty is not reflected in registry and staking state."
            );
        }
        return FinalizedSlashingEvidenceAuditResult::passed(1);
    }

    if (decision->jailsValidator()) {
        if (!entry->jailed() || !stakeAccount->jailed()) {
            return FinalizedSlashingEvidenceAuditResult::failed(
                "Finalized jail/slash penalty is not reflected in registry and staking state."
            );
        }
    }

    return FinalizedSlashingEvidenceAuditResult::passed(1);
}

} // namespace

FinalizedSlashingEvidenceAuditResult
FinalizedSlashingEvidenceAuditResult::passed(std::size_t evidenceCount) {
    FinalizedSlashingEvidenceAuditResult result;
    result.m_passed = true;
    result.m_reason = "Finalized slashing evidence effects are canonical.";
    result.m_evidenceCount = evidenceCount;
    return result;
}

FinalizedSlashingEvidenceAuditResult
FinalizedSlashingEvidenceAuditResult::failed(std::string reason) {
    FinalizedSlashingEvidenceAuditResult result;
    result.m_passed = false;
    result.m_reason = std::move(reason);
    result.m_evidenceCount = 0;
    return result;
}

bool FinalizedSlashingEvidenceAuditResult::passed() const {
    return m_passed;
}

const std::string& FinalizedSlashingEvidenceAuditResult::reason() const {
    return m_reason;
}

std::size_t FinalizedSlashingEvidenceAuditResult::evidenceCount() const {
    return m_evidenceCount;
}

FinalizedSlashingEvidenceAuditResult
FinalizedSlashingEvidenceAudit::auditBlockEffects(
    const core::Block& block,
    const consensus::ValidatorPenaltyLedger& penaltyLedger,
    const core::ValidatorRegistry& validators,
    const StakingRegistry& staking
) {
    if (block.index() == 0 || block.timestamp() <= 0) {
        return FinalizedSlashingEvidenceAuditResult::failed(
            "Finalized slashing audit requires a non-genesis block with a positive timestamp."
        );
    }

    std::vector<consensus::SlashingEvidenceRecord> records;
    std::set<std::string> evidenceIds;
    try {
        for (const consensus::DoubleVoteEvidence& evidence :
             CanonicalSlashingTransition::doubleVoteEvidenceFromBlock(block)) {
            const consensus::SlashingEvidenceRecord record = evidence.toRecord();
            if (!evidenceIds.insert(record.evidenceId()).second) {
                return FinalizedSlashingEvidenceAuditResult::failed(
                    "Finalized block contains duplicate slashing evidence."
                );
            }
            records.push_back(record);
        }
        for (const consensus::ProposerEquivocationEvidence& evidence :
             CanonicalSlashingTransition::proposerEquivocationEvidenceFromBlock(block)) {
            const consensus::SlashingEvidenceRecord record = evidence.toRecord();
            if (!evidenceIds.insert(record.evidenceId()).second) {
                return FinalizedSlashingEvidenceAuditResult::failed(
                    "Finalized block contains duplicate slashing evidence."
                );
            }
            records.push_back(record);
        }
    } catch (const std::exception& error) {
        return FinalizedSlashingEvidenceAuditResult::failed(
            std::string("Finalized block contains non-canonical slashing evidence: ") +
            error.what()
        );
    }

    if (records.size() > core::ProtocolLimits::MAX_SLASHING_EVIDENCE_PER_BLOCK) {
        return FinalizedSlashingEvidenceAuditResult::failed(
            "Finalized block exceeds the slashing evidence per-block limit."
        );
    }

    for (const consensus::SlashingEvidenceRecord& record : records) {
        const FinalizedSlashingEvidenceAuditResult recordAudit =
            auditEvidenceRecord(
                record,
                block.timestamp(),
                penaltyLedger,
                validators,
                staking
            );
        if (!recordAudit.passed()) {
            return recordAudit;
        }
    }

    return FinalizedSlashingEvidenceAuditResult::passed(records.size());
}

} // namespace nodo::node
