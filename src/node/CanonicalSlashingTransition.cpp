#include "node/CanonicalSlashingTransition.hpp"

#include "consensus/ProposerSchedule.hpp"
#include "core/LedgerRecordDomainValidator.hpp"
#include "core/ProtocolLimits.hpp"
#include "node/SignedBlockProposalMessage.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace nodo::node {

namespace {

using EvidenceVariant = std::variant<
    consensus::DoubleVoteEvidence,
    consensus::ProposerEquivocationEvidence
>;

bool hasPrefix(const std::string& value, const char* prefix) {
    const std::string expected(prefix);
    return value.size() >= expected.size() &&
           value.compare(0, expected.size(), expected) == 0;
}

EvidenceVariant deserializeCanonicalEvidence(const core::LedgerRecord& record) {
    if (record.type() != core::LedgerRecordType::SLASHING_EVIDENCE) {
        throw std::invalid_argument("Ledger record is not slashing evidence.");
    }

    const std::string& payload = record.payload();
    if (hasPrefix(payload, "DoubleVoteEvidence{")) {
        consensus::DoubleVoteEvidence evidence =
            consensus::DoubleVoteEvidence::deserialize(payload);
        if (record.sourceId() != evidence.evidenceId()) {
            throw std::invalid_argument(
                "Slashing ledger record source id does not match double-vote evidence."
            );
        }
        return evidence;
    }

    if (hasPrefix(payload, "ProposerEquivocationEvidence{")) {
        consensus::ProposerEquivocationEvidence evidence =
            consensus::ProposerEquivocationEvidence::deserialize(payload);
        if (record.sourceId() != evidence.evidenceId()) {
            throw std::invalid_argument(
                "Slashing ledger record source id does not match proposer-equivocation evidence."
            );
        }
        return evidence;
    }

    throw std::invalid_argument("Unknown slashing evidence payload type.");
}

std::uint64_t offenseHeightOf(const EvidenceVariant& evidence) {
    return std::visit([](const auto& value) -> std::uint64_t {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, consensus::DoubleVoteEvidence>) {
            return value.firstVote().blockIndex();
        } else {
            return value.blockIndex();
        }
    }, evidence);
}

const consensus::SlashingEvidenceRecord recordOf(const EvidenceVariant& evidence) {
    return std::visit([](const auto& value) {
        return value.toRecord();
    }, evidence);
}

std::int64_t detectedAtOf(const EvidenceVariant& evidence) {
    return std::visit([](const auto& value) {
        return value.detectedAt();
    }, evidence);
}

consensus::SlashingEvidenceValidationResult verifyProposerEquivocationForHistory(
    const consensus::ProposerEquivocationEvidence& evidence,
    std::uint64_t maximumOffenseHeight,
    const core::ValidatorSetHistory& validatorSetHistory,
    const std::string& chainId,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    const consensus::SlashingEvidenceValidationResult structural =
        consensus::SlashingEvidenceVerifier::validateProposerEquivocationStructure(
            evidence
        );
    if (!structural.accepted()) {
        return structural;
    }

    if (evidence.blockIndex() == 0 || evidence.blockIndex() > maximumOffenseHeight) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence refers to an unavailable consensus height.",
            structural.record()
        );
    }
    if (chainId.empty() || !validatorSetHistory.hasSet(evidence.blockIndex())) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Historical validator set is unavailable for proposer-equivocation evidence.",
            structural.record()
        );
    }

    SignedBlockProposalMessage firstProposal;
    SignedBlockProposalMessage secondProposal;
    try {
        firstProposal = SignedBlockProposalMessage::deserialize(evidence.firstProposal());
        secondProposal = SignedBlockProposalMessage::deserialize(evidence.secondProposal());
    } catch (const std::exception&) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence contains malformed signed proposals.",
            structural.record()
        );
    }

    if (firstProposal.proposerAddress() != evidence.proposerAddress() ||
        secondProposal.proposerAddress() != evidence.proposerAddress() ||
        firstProposal.blockIndex() != evidence.blockIndex() ||
        secondProposal.blockIndex() != evidence.blockIndex() ||
        firstProposal.round() != evidence.round() ||
        secondProposal.round() != evidence.round() ||
        firstProposal.blockHash() != evidence.firstBlockHash() ||
        secondProposal.blockHash() != evidence.secondBlockHash() ||
        firstProposal.blockHash() == secondProposal.blockHash()) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence metadata does not match signed proposals.",
            structural.record()
        );
    }

    const core::ValidatorRegistry& historicalValidators =
        validatorSetHistory.setAt(evidence.blockIndex());
    const std::string expectedProposer = consensus::ProposerSchedule::selectProposer(
        historicalValidators,
        chainId,
        evidence.blockIndex(),
        evidence.round()
    );
    if (expectedProposer.empty() || expectedProposer != evidence.proposerAddress()) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation signer was not the scheduled proposer.",
            structural.record()
        );
    }

    if (!firstProposal.verify(expectedProposer, historicalValidators, policy, provider) ||
        !secondProposal.verify(expectedProposer, historicalValidators, policy, provider)) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence contains an invalid proposal signature.",
            structural.record()
        );
    }

    return consensus::SlashingEvidenceValidationResult(
        consensus::SlashingEvidenceValidationStatus::ACCEPTED,
        "Proposer-equivocation evidence is structurally valid and both proposal signatures verify.",
        structural.record()
    );
}

consensus::SlashingEvidenceValidationResult verifyEvidenceForHistory(
    const EvidenceVariant& evidence,
    std::uint64_t maximumOffenseHeight,
    const core::ValidatorSetHistory& validatorSetHistory,
    const std::string& chainId,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    return std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, consensus::DoubleVoteEvidence>) {
            return consensus::SlashingEvidenceVerifier::verifyDoubleVoteEvidenceForHistory(
                value,
                maximumOffenseHeight,
                validatorSetHistory,
                policy,
                provider
            );
        } else {
            return verifyProposerEquivocationForHistory(
                value,
                maximumOffenseHeight,
                validatorSetHistory,
                chainId,
                policy,
                provider
            );
        }
    }, evidence);
}

void applyPenaltyEffects(
    const consensus::ValidatorPenaltyDecision& decision,
    std::uint64_t blockHeight,
    std::int64_t blockTimestamp,
    const consensus::ValidatorPenaltyLedger& penaltyLedger,
    core::ValidatorRegistry& validators,
    StakingRegistry& staking
) {
    const core::ValidatorRegistryEntry* entry =
        validators.entryForAddress(decision.validatorAddress());
    if (entry == nullptr) {
        throw std::logic_error(
            "Penalized validator is missing from the current registry."
        );
    }

    const economics::StakeAccount* stakeAccount =
        staking.accountFor(decision.validatorAddress());
    if (stakeAccount == nullptr) {
        throw std::logic_error(
            "Penalized validator is missing from the staking registry."
        );
    }

    const std::int64_t totalSlashRaw =
        penaltyLedger.totalSlashAmountForValidator(decision.validatorAddress());
    const std::int64_t boundedSlashRaw = std::min(
        std::max<std::int64_t>(totalSlashRaw, 0),
        stakeAccount->bondedAmount().rawUnits()
    );
    const bool tombstoned =
        penaltyLedger.validatorIsTombstoned(decision.validatorAddress());
    const bool jailed = tombstoned ||
        penaltyLedger.validatorIsJailed(decision.validatorAddress());

    staking.applyPenaltyState(
        decision.validatorAddress(),
        utils::Amount::fromRawUnits(boundedSlashRaw),
        jailed,
        tombstoned,
        blockHeight
    );

    const std::int64_t activeStakeRaw =
        staking.activeStakeFor(decision.validatorAddress()).rawUnits();
    const std::uint64_t registryStake = activeStakeRaw <= 0
        ? 0
        : static_cast<std::uint64_t>(activeStakeRaw);
    const core::ValidatorRegistryUpdateResult stakeResult =
        validators.updateStake(
            decision.validatorAddress(),
            registryStake,
            blockTimestamp
        );
    if (!stakeResult.success()) {
        throw std::logic_error(
            "Canonical validator stake slash could not update the registry."
        );
    }

    entry = validators.entryForAddress(decision.validatorAddress());
    if (entry == nullptr) {
        throw std::logic_error(
            "Penalized validator disappeared after stake update."
        );
    }

    if (tombstoned) {
        if (!entry->exited()) {
            const core::ValidatorRegistryUpdateResult result =
                validators.deactivateValidator(
                    decision.validatorAddress(),
                    blockTimestamp
                );
            if (!result.success()) {
                throw std::logic_error(
                    "Canonical validator tombstone failed."
                );
            }
        }
        return;
    }

    if (jailed && entry->active()) {
        const std::uint64_t currentEpoch =
            ((blockHeight - 1) / NODO_VALIDATOR_EPOCH_BLOCKS) + 1;
        const core::ValidatorRegistryUpdateResult result =
            validators.jailValidator(
                decision.validatorAddress(),
                currentEpoch + decision.jailEpochs(),
                blockTimestamp
            );
        if (!result.success()) {
            throw std::logic_error("Canonical validator jail failed.");
        }
    }
}

} // namespace

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
            "Cannot build a ledger record from invalid double-vote evidence."
        );
    }
    return core::LedgerRecord::fromSlashingEvidencePayload(
        evidence.evidenceId(), evidence.serialize(), blockTimestamp
    );
}

core::LedgerRecord CanonicalSlashingTransition::buildEvidenceRecord(
    const consensus::ProposerEquivocationEvidence& evidence,
    std::int64_t blockTimestamp
) {
    const consensus::SlashingEvidenceValidationResult structural =
        consensus::SlashingEvidenceVerifier::validateProposerEquivocationStructure(
            evidence
        );
    if (!structural.accepted() || blockTimestamp <= 0) {
        throw std::invalid_argument(
            "Cannot build a ledger record from invalid proposer-equivocation evidence."
        );
    }
    return core::LedgerRecord::fromSlashingEvidencePayload(
        evidence.evidenceId(), evidence.serialize(), blockTimestamp
    );
}

std::vector<consensus::DoubleVoteEvidence>
CanonicalSlashingTransition::doubleVoteEvidenceFromBlock(const core::Block& block) {
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
        const EvidenceVariant parsed = deserializeCanonicalEvidence(record);
        if (const auto* doubleVote = std::get_if<consensus::DoubleVoteEvidence>(&parsed)) {
            evidence.push_back(*doubleVote);
        }
    }
    return evidence;
}

std::vector<consensus::ProposerEquivocationEvidence>
CanonicalSlashingTransition::proposerEquivocationEvidenceFromBlock(const core::Block& block) {
    std::vector<consensus::ProposerEquivocationEvidence> evidence;
    for (const core::LedgerRecord& record : block.records()) {
        if (record.type() != core::LedgerRecordType::SLASHING_EVIDENCE) {
            continue;
        }
        const core::LedgerRecordDomainValidator::Result domain =
            core::LedgerRecordDomainValidator::validate(record);
        if (!domain.valid) {
            throw std::invalid_argument(domain.reason);
        }
        const EvidenceVariant parsed = deserializeCanonicalEvidence(record);
        if (const auto* equivocation = std::get_if<consensus::ProposerEquivocationEvidence>(&parsed)) {
            evidence.push_back(*equivocation);
        }
    }
    return evidence;
}

void CanonicalSlashingTransition::applyBlockEvidence(
    const core::Block& block,
    const core::ValidatorSetHistory& validatorSetHistory,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    consensus::ValidatorPenaltyLedger& penaltyLedger,
    core::ValidatorRegistry& validators,
    StakingRegistry& staking,
    const std::string& chainId
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
        validators,
        staking,
        chainId
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
    core::ValidatorRegistry& validators,
    StakingRegistry& staking,
    const std::string& chainId
) {
    if (blockHeight == 0 || blockTimestamp <= 0 || chainId.empty()) {
        throw std::invalid_argument(
            "Canonical slashing transition requires block height, timestamp and chain id."
        );
    }
    if (evidenceRecords.size() >
        core::ProtocolLimits::MAX_SLASHING_EVIDENCE_PER_BLOCK) {
        throw std::invalid_argument(
            "Block exceeds the canonical slashing evidence limit."
        );
    }

    std::set<std::string> evidenceIds;
    for (const core::LedgerRecord& ledgerRecord : evidenceRecords) {
        if (ledgerRecord.type() != core::LedgerRecordType::SLASHING_EVIDENCE ||
            ledgerRecord.timestamp() != blockTimestamp) {
            throw std::invalid_argument(
                "Canonical slashing input contains an invalid ledger record."
            );
        }
        const core::LedgerRecordDomainValidator::Result domain =
            core::LedgerRecordDomainValidator::validate(ledgerRecord);
        if (!domain.valid) {
            throw std::invalid_argument(domain.reason);
        }

        const EvidenceVariant evidence = deserializeCanonicalEvidence(ledgerRecord);
        const consensus::SlashingEvidenceRecord record = recordOf(evidence);
        const std::uint64_t offenseHeight = offenseHeightOf(evidence);

        if (ledgerRecord.sourceId() != record.evidenceId() ||
            !evidenceIds.insert(record.evidenceId()).second ||
            penaltyLedger.containsEvidence(record.evidenceId())) {
            throw std::invalid_argument(
                "Slashing evidence is duplicated or already finalized."
            );
        }
        if (offenseHeight == 0 || offenseHeight >= blockHeight ||
            detectedAtOf(evidence) > blockTimestamp) {
            throw std::invalid_argument(
                "Slashing evidence must refer to a prior block height."
            );
        }

        const consensus::SlashingEvidenceValidationResult verified =
            verifyEvidenceForHistory(
                evidence,
                blockHeight - 1,
                validatorSetHistory,
                chainId,
                policy,
                provider
            );
        if (!verified.accepted()) {
            throw std::invalid_argument(
                "Slashing evidence verification failed: " + verified.reason()
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

        applyPenaltyEffects(
            applied.decision().value(),
            blockHeight,
            blockTimestamp,
            penaltyLedger,
            validators,
            staking
        );
    }
}

} // namespace nodo::node
