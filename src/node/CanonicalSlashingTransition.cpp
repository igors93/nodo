#include "node/CanonicalSlashingTransition.hpp"

#include "core/LedgerRecordDomainValidator.hpp"
#include "core/ProtocolLimits.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace nodo::node {

core::LedgerRecord CanonicalSlashingTransition::buildEvidenceRecord(
    const consensus::DoubleVoteEvidence& evidence,
    std::int64_t blockTimestamp
) {
    const consensus::SlashingEvidenceValidationResult structural =
        consensus::SlashingEvidenceVerifier::validateDoubleVoteStructure(
            evidence
        );
    if (!structural.accepted() || blockTimestamp <= 0) {
        throw std::invalid_argument(
            "Cannot build a ledger record from invalid slashing evidence."
        );
    }
    return core::LedgerRecord::fromSlashingEvidencePayload(
        evidence.evidenceId(), evidence.serialize(), blockTimestamp
    );
}

std::vector<consensus::DoubleVoteEvidence>
CanonicalSlashingTransition::evidenceFromBlock(const core::Block& block) {
    std::vector<consensus::DoubleVoteEvidence> evidence;
    for (const core::LedgerRecord& record : block.records()) {
        if (record.type() != core::LedgerRecordType::SLASHING_EVIDENCE) {
            continue;
        }
        const core::LedgerRecordDomainValidator::Result domain =
            core::LedgerRecordDomainValidator::validate(record);
        if (!domain.valid) {
            throw std::invalid_argument(domain.reason);
        }
        evidence.push_back(
            consensus::DoubleVoteEvidence::deserialize(record.payload())
        );
    }
    return evidence;
}

void CanonicalSlashingTransition::applyBlockEvidence(
    const core::Block& block,
    const core::ValidatorSetHistory& validatorSetHistory,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    consensus::ValidatorPenaltyLedger& penaltyLedger,
    core::ValidatorRegistry& validators
) {
    std::vector<core::LedgerRecord> records;
    for (const core::LedgerRecord& record : block.records()) {
        if (record.type() == core::LedgerRecordType::SLASHING_EVIDENCE) {
            records.push_back(record);
        }
    }
    applyEvidenceRecords(
        records,
        block.index(),
        block.timestamp(),
        validatorSetHistory,
        policy,
        provider,
        penaltyLedger,
        validators
    );
}

void CanonicalSlashingTransition::applyEvidenceRecords(
    const std::vector<core::LedgerRecord>& evidenceRecords,
    std::uint64_t blockHeight,
    std::int64_t blockTimestamp,
    const core::ValidatorSetHistory& validatorSetHistory,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    consensus::ValidatorPenaltyLedger& penaltyLedger,
    core::ValidatorRegistry& validators
) {
    if (evidenceRecords.size() >
        core::ProtocolLimits::MAX_SLASHING_EVIDENCE_PER_BLOCK) {
        throw std::invalid_argument(
            "Block exceeds the canonical slashing evidence limit."
        );
    }

    std::set<std::string> evidenceIds;
    for (const core::LedgerRecord& ledgerRecord : evidenceRecords) {
        if (ledgerRecord.type() !=
                core::LedgerRecordType::SLASHING_EVIDENCE ||
            ledgerRecord.timestamp() != blockTimestamp) {
            throw std::invalid_argument(
                "Canonical slashing input contains an invalid ledger record."
            );
        }
        const consensus::DoubleVoteEvidence evidence =
            consensus::DoubleVoteEvidence::deserialize(
                ledgerRecord.payload()
            );
        const consensus::SlashingEvidenceRecord record = evidence.toRecord();
        const std::uint64_t offenseHeight = evidence.firstVote().blockIndex();

        if (ledgerRecord.sourceId() != record.evidenceId() ||
            !evidenceIds.insert(record.evidenceId()).second ||
            penaltyLedger.containsEvidence(record.evidenceId())) {
            throw std::invalid_argument(
                "Slashing evidence is duplicated or already finalized."
            );
        }
        if (offenseHeight == 0 || offenseHeight >= blockHeight ||
            evidence.detectedAt() > blockTimestamp) {
            throw std::invalid_argument(
                "Slashing evidence must refer to a prior block height."
            );
        }
        const consensus::SlashingEvidenceValidationResult verified =
            consensus::SlashingEvidenceVerifier::
                verifyDoubleVoteEvidenceForHistory(
                    evidence,
                    blockHeight - 1,
                    validatorSetHistory,
                    policy,
                    provider
                );
        if (!verified.accepted()) {
            throw std::invalid_argument(
                "Slashing evidence verification failed: " +
                verified.reason()
            );
        }

        const consensus::ValidatorPenaltyApplicationResult applied =
            penaltyLedger.applyEvidence(
                record,
                consensus::ValidatorPenaltyPolicy::conservativeTestnetPolicy(),
                blockTimestamp
            );
        if (!applied.applied() || !applied.decision().has_value()) {
            throw std::logic_error(
                "Canonical validator penalty could not be applied."
            );
        }

        const consensus::ValidatorPenaltyDecision& decision =
            applied.decision().value();
        const core::ValidatorRegistryEntry* currentEntry =
            validators.entryForAddress(decision.validatorAddress());
        if (currentEntry == nullptr) {
            throw std::logic_error(
                "Penalized validator is missing from the current registry."
            );
        }

        if (decision.slashable() && currentEntry->stakeAmount() > 0) {
            const std::uint64_t requestedSlash =
                decision.slashAmountRawUnits() <= 0
                    ? 0
                    : static_cast<std::uint64_t>(
                        decision.slashAmountRawUnits()
                    );
            const std::uint64_t newStake =
                currentEntry->stakeAmount() -
                std::min(currentEntry->stakeAmount(), requestedSlash);
            const core::ValidatorRegistryUpdateResult stakeResult =
                validators.updateStake(
                    decision.validatorAddress(), newStake, blockTimestamp
                );
            if (!stakeResult.success()) {
                throw std::logic_error(
                    "Canonical validator stake slash failed."
                );
            }
            currentEntry = validators.entryForAddress(
                decision.validatorAddress()
            );
        }

        if (decision.tombstonesValidator()) {
            if (!currentEntry->exited()) {
                const core::ValidatorRegistryUpdateResult result =
                    validators.deactivateValidator(
                        decision.validatorAddress(), blockTimestamp
                    );
                if (!result.success()) {
                    throw std::logic_error(
                        "Canonical validator tombstone failed."
                    );
                }
            }
        } else if (decision.jailsValidator() && currentEntry->active()) {
            const std::uint64_t currentEpoch =
                ((blockHeight - 1) / NODO_VALIDATOR_EPOCH_BLOCKS) + 1;
            const core::ValidatorRegistryUpdateResult result =
                validators.jailValidator(
                    decision.validatorAddress(),
                    currentEpoch + decision.jailEpochs(),
                    blockTimestamp
                );
            if (!result.success()) {
                throw std::logic_error(
                    "Canonical validator jail failed."
                );
            }
        }
    }
}

} // namespace nodo::node
