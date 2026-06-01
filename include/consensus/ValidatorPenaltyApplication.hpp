#ifndef NODO_CONSENSUS_VALIDATOR_PENALTY_APPLICATION_HPP
#define NODO_CONSENSUS_VALIDATOR_PENALTY_APPLICATION_HPP

#include "consensus/SlashingEvidence.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nodo::consensus {

/*
 * Validator penalty application is the conservative boundary that turns already
 * accepted slashing evidence into deterministic penalty decisions.
 *
 * This layer is intentionally separate from evidence verification. Evidence says
 * "this validator misbehaved". Penalty application says "given that evidence and
 * this policy, this is the deterministic protocol consequence".
 */
enum class ValidatorPenaltyAction {
    NONE,
    WARNING,
    JAIL,
    SLASH,
    TOMBSTONE
};

std::string validatorPenaltyActionToString(ValidatorPenaltyAction action);
ValidatorPenaltyAction validatorPenaltyActionFromString(const std::string& value);

class ValidatorPenaltyPolicy {
public:
    ValidatorPenaltyPolicy();

    ValidatorPenaltyPolicy(
        std::int64_t doubleVoteSlashRawUnits,
        std::int64_t equivocationSlashRawUnits,
        std::uint64_t defaultJailEpochs,
        bool tombstoneEquivocation
    );

    static ValidatorPenaltyPolicy conservativeTestnetPolicy();

    std::int64_t doubleVoteSlashRawUnits() const;
    std::int64_t equivocationSlashRawUnits() const;
    std::uint64_t defaultJailEpochs() const;
    bool tombstoneEquivocation() const;

    bool isValid() const;

    ValidatorPenaltyAction actionForEvidence(
        const SlashingEvidenceRecord& evidence
    ) const;

    std::int64_t slashAmountForEvidence(
        const SlashingEvidenceRecord& evidence
    ) const;

    std::uint64_t jailEpochsForEvidence(
        const SlashingEvidenceRecord& evidence
    ) const;

    std::string serialize() const;

private:
    std::int64_t m_doubleVoteSlashRawUnits;
    std::int64_t m_equivocationSlashRawUnits;
    std::uint64_t m_defaultJailEpochs;
    bool m_tombstoneEquivocation;
};

class ValidatorPenaltyDecision {
public:
    ValidatorPenaltyDecision();

    ValidatorPenaltyDecision(
        std::string penaltyId,
        std::string evidenceId,
        std::string validatorAddress,
        SlashingEvidenceType evidenceType,
        SlashingEvidenceSeverity evidenceSeverity,
        ValidatorPenaltyAction action,
        std::int64_t slashAmountRawUnits,
        std::uint64_t jailEpochs,
        std::int64_t createdAt
    );

    const std::string& penaltyId() const;
    const std::string& evidenceId() const;
    const std::string& validatorAddress() const;
    SlashingEvidenceType evidenceType() const;
    SlashingEvidenceSeverity evidenceSeverity() const;
    ValidatorPenaltyAction action() const;
    std::int64_t slashAmountRawUnits() const;
    std::uint64_t jailEpochs() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    bool slashable() const;
    bool jailsValidator() const;
    bool tombstonesValidator() const;
    std::string serialize() const;

    static ValidatorPenaltyDecision create(
        const SlashingEvidenceRecord& evidence,
        const ValidatorPenaltyPolicy& policy,
        std::int64_t createdAt
    );

    static ValidatorPenaltyDecision deserialize(
        const std::string& serialized
    );

private:
    std::string m_penaltyId;
    std::string m_evidenceId;
    std::string m_validatorAddress;
    SlashingEvidenceType m_evidenceType;
    SlashingEvidenceSeverity m_evidenceSeverity;
    ValidatorPenaltyAction m_action;
    std::int64_t m_slashAmountRawUnits;
    std::uint64_t m_jailEpochs;
    std::int64_t m_createdAt;
};

enum class ValidatorPenaltyApplicationStatus {
    APPLIED,
    DUPLICATE,
    REJECTED
};

std::string validatorPenaltyApplicationStatusToString(
    ValidatorPenaltyApplicationStatus status
);

class ValidatorPenaltyApplicationResult {
public:
    ValidatorPenaltyApplicationResult();

    ValidatorPenaltyApplicationResult(
        ValidatorPenaltyApplicationStatus status,
        std::string reason,
        std::optional<ValidatorPenaltyDecision> decision
    );

    ValidatorPenaltyApplicationStatus status() const;
    const std::string& reason() const;
    const std::optional<ValidatorPenaltyDecision>& decision() const;

    bool applied() const;
    bool duplicate() const;
    bool rejected() const;
    std::string serialize() const;

private:
    ValidatorPenaltyApplicationStatus m_status;
    std::string m_reason;
    std::optional<ValidatorPenaltyDecision> m_decision;
};

class ValidatorPenaltyLedger {
public:
    ValidatorPenaltyLedger();

    ValidatorPenaltyApplicationResult applyEvidence(
        const SlashingEvidenceRecord& evidence,
        const ValidatorPenaltyPolicy& policy,
        std::int64_t now
    );

    ValidatorPenaltyApplicationResult applyDecision(
        const ValidatorPenaltyDecision& decision
    );

    bool containsEvidence(const std::string& evidenceId) const;
    bool containsPenalty(const std::string& penaltyId) const;

    const ValidatorPenaltyDecision* decisionByEvidenceId(
        const std::string& evidenceId
    ) const;

    const ValidatorPenaltyDecision* decisionByPenaltyId(
        const std::string& penaltyId
    ) const;

    std::vector<ValidatorPenaltyDecision> allDecisions() const;

    std::vector<ValidatorPenaltyDecision> decisionsForValidator(
        const std::string& validatorAddress
    ) const;

    std::int64_t totalSlashAmountForValidator(
        const std::string& validatorAddress
    ) const;

    bool validatorIsJailed(const std::string& validatorAddress) const;
    bool validatorIsTombstoned(const std::string& validatorAddress) const;

    std::size_t size() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::map<std::string, ValidatorPenaltyDecision> m_decisionsByPenaltyId;
    std::map<std::string, std::string> m_penaltyIdByEvidenceId;
    std::map<std::string, std::vector<std::string>> m_penaltyIdsByValidator;
};

} // namespace nodo::consensus

#endif
