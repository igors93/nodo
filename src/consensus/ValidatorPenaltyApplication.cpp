#include "consensus/ValidatorPenaltyApplication.hpp"

#include "crypto/hash.h"

#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::consensus {

namespace {

bool isSafeScalar(const std::string& value, std::size_t maxSize = 240) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::string hashString(const std::string& value) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));
    return std::string(output);
}

std::vector<std::string> splitTopLevel(const std::string& value, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    int braceDepth = 0;

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current = value[index];
        if (current == '{') {
            ++braceDepth;
        } else if (current == '}') {
            --braceDepth;
        }

        if (braceDepth < 0) {
            throw std::invalid_argument("Malformed serialized penalty decision.");
        }

        if (current == separator && braceDepth == 0) {
            if (index == start) {
                throw std::invalid_argument("Empty serialized penalty field.");
            }
            parts.push_back(value.substr(start, index - start));
            start = index + 1;
        }
    }

    if (braceDepth != 0 || start >= value.size()) {
        throw std::invalid_argument("Malformed serialized penalty decision.");
    }

    parts.push_back(value.substr(start));
    return parts;
}

std::map<std::string, std::string> parseObjectFields(
    const std::string& serialized,
    const std::string& typeName
) {
    const std::string prefix = typeName + "{";
    if (serialized.rfind(prefix, 0) != 0 || serialized.back() != '}') {
        throw std::invalid_argument("Serialized value is not a " + typeName + ".");
    }

    const std::string body = serialized.substr(prefix.size(), serialized.size() - prefix.size() - 1);
    if (body.empty()) {
        throw std::invalid_argument("Serialized " + typeName + " is empty.");
    }

    std::map<std::string, std::string> fields;
    for (const std::string& part : splitTopLevel(body, ';')) {
        const std::size_t separator = part.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 >= part.size()) {
            throw std::invalid_argument("Malformed serialized " + typeName + " field.");
        }

        const std::string key = part.substr(0, separator);
        const std::string value = part.substr(separator + 1);
        if (!fields.emplace(key, value).second) {
            throw std::invalid_argument("Duplicated serialized " + typeName + " field.");
        }
    }

    return fields;
}

std::string requireField(
    const std::map<std::string, std::string>& fields,
    const std::string& key,
    const std::string& typeName
) {
    const auto found = fields.find(key);
    if (found == fields.end()) {
        throw std::invalid_argument("Missing serialized " + typeName + " field: " + key);
    }
    return found->second;
}

void requireExactFields(
    const std::map<std::string, std::string>& fields,
    const std::set<std::string>& expected,
    const std::string& typeName
) {
    for (const auto& key : expected) {
        if (fields.find(key) == fields.end()) {
            throw std::invalid_argument("Missing serialized " + typeName + " field: " + key);
        }
    }

    for (const auto& [key, value] : fields) {
        (void)value;
        if (expected.find(key) == expected.end()) {
            throw std::invalid_argument("Unknown serialized " + typeName + " field: " + key);
        }
    }
}

std::uint64_t parseU64Strict(const std::string& value, const std::string& fieldName) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }
    for (const char current : value) {
        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }
    std::size_t parsedSize = 0;
    const auto parsed = static_cast<std::uint64_t>(std::stoull(value, &parsedSize));
    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }
    return parsed;
}

std::int64_t parseI64Strict(const std::string& value, const std::string& fieldName) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current = value[index];
        if (current == '-' && index == 0 && value.size() > 1) {
            continue;
        }
        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }
    std::size_t parsedSize = 0;
    const auto parsed = static_cast<std::int64_t>(std::stoll(value, &parsedSize));
    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }
    return parsed;
}

std::string computePenaltyId(
    const std::string& evidenceId,
    const std::string& validatorAddress,
    SlashingEvidenceType evidenceType,
    SlashingEvidenceSeverity evidenceSeverity,
    ValidatorPenaltyAction action,
    std::int64_t slashAmountRawUnits,
    std::uint64_t jailEpochs
) {
    std::ostringstream payload;
    payload << "NODO_VALIDATOR_PENALTY_DECISION_V1|"
            << "evidenceId=" << evidenceId
            << ";validatorAddress=" << validatorAddress
            << ";evidenceType=" << slashingEvidenceTypeToString(evidenceType)
            << ";evidenceSeverity=" << slashingEvidenceSeverityToString(evidenceSeverity)
            << ";action=" << validatorPenaltyActionToString(action)
            << ";slashAmountRawUnits=" << slashAmountRawUnits
            << ";jailEpochs=" << jailEpochs;
    return hashString(payload.str());
}

std::string computePenaltyId(
    const SlashingEvidenceRecord& evidence,
    ValidatorPenaltyAction action,
    std::int64_t slashAmountRawUnits,
    std::uint64_t jailEpochs
) {
    return computePenaltyId(
        evidence.evidenceId(),
        evidence.validatorAddress(),
        evidence.type(),
        evidence.severity(),
        action,
        slashAmountRawUnits,
        jailEpochs
    );
}

} // namespace

std::string validatorPenaltyActionToString(ValidatorPenaltyAction action) {
    switch (action) {
        case ValidatorPenaltyAction::NONE: return "NONE";
        case ValidatorPenaltyAction::WARNING: return "WARNING";
        case ValidatorPenaltyAction::JAIL: return "JAIL";
        case ValidatorPenaltyAction::SLASH: return "SLASH";
        case ValidatorPenaltyAction::TOMBSTONE: return "TOMBSTONE";
        default: return "NONE";
    }
}

ValidatorPenaltyAction validatorPenaltyActionFromString(const std::string& value) {
    if (value == "NONE") return ValidatorPenaltyAction::NONE;
    if (value == "WARNING") return ValidatorPenaltyAction::WARNING;
    if (value == "JAIL") return ValidatorPenaltyAction::JAIL;
    if (value == "SLASH") return ValidatorPenaltyAction::SLASH;
    if (value == "TOMBSTONE") return ValidatorPenaltyAction::TOMBSTONE;
    return ValidatorPenaltyAction::NONE;
}

ValidatorPenaltyPolicy::ValidatorPenaltyPolicy()
    : m_doubleVoteSlashRawUnits(0),
      m_equivocationSlashRawUnits(0),
      m_defaultJailEpochs(0),
      m_tombstoneEquivocation(false) {}

ValidatorPenaltyPolicy::ValidatorPenaltyPolicy(
    std::int64_t doubleVoteSlashRawUnits,
    std::int64_t equivocationSlashRawUnits,
    std::uint64_t defaultJailEpochs,
    bool tombstoneEquivocation
) : m_doubleVoteSlashRawUnits(doubleVoteSlashRawUnits),
    m_equivocationSlashRawUnits(equivocationSlashRawUnits),
    m_defaultJailEpochs(defaultJailEpochs),
    m_tombstoneEquivocation(tombstoneEquivocation) {}

ValidatorPenaltyPolicy ValidatorPenaltyPolicy::conservativeTestnetPolicy() {
    return ValidatorPenaltyPolicy(
        100000000,
        500000000,
        64,
        true
    );
}

std::int64_t ValidatorPenaltyPolicy::doubleVoteSlashRawUnits() const {
    return m_doubleVoteSlashRawUnits;
}

std::int64_t ValidatorPenaltyPolicy::equivocationSlashRawUnits() const {
    return m_equivocationSlashRawUnits;
}

std::uint64_t ValidatorPenaltyPolicy::defaultJailEpochs() const {
    return m_defaultJailEpochs;
}

bool ValidatorPenaltyPolicy::tombstoneEquivocation() const {
    return m_tombstoneEquivocation;
}

bool ValidatorPenaltyPolicy::isValid() const {
    return m_doubleVoteSlashRawUnits >= 0 &&
           m_equivocationSlashRawUnits >= m_doubleVoteSlashRawUnits &&
           m_defaultJailEpochs > 0;
}

ValidatorPenaltyAction ValidatorPenaltyPolicy::actionForEvidence(
    const SlashingEvidenceRecord& evidence
) const {
    if (!isValid() || !evidence.isValid()) {
        return ValidatorPenaltyAction::NONE;
    }

    if (evidence.severity() == SlashingEvidenceSeverity::WARNING) {
        return ValidatorPenaltyAction::WARNING;
    }

    if (evidence.severity() != SlashingEvidenceSeverity::SLASHABLE) {
        return ValidatorPenaltyAction::NONE;
    }

    if (evidence.type() == SlashingEvidenceType::DOUBLE_VOTE) {
        return ValidatorPenaltyAction::SLASH;
    }

    if (evidence.type() == SlashingEvidenceType::EQUIVOCATION) {
        return m_tombstoneEquivocation
            ? ValidatorPenaltyAction::TOMBSTONE
            : ValidatorPenaltyAction::SLASH;
    }

    if (evidence.type() == SlashingEvidenceType::INVALID_SIGNATURE) {
        return ValidatorPenaltyAction::JAIL;
    }

    return ValidatorPenaltyAction::NONE;
}

std::int64_t ValidatorPenaltyPolicy::slashAmountForEvidence(
    const SlashingEvidenceRecord& evidence
) const {
    const ValidatorPenaltyAction action = actionForEvidence(evidence);
    if (action != ValidatorPenaltyAction::SLASH &&
        action != ValidatorPenaltyAction::TOMBSTONE) {
        return 0;
    }

    if (evidence.type() == SlashingEvidenceType::EQUIVOCATION) {
        return m_equivocationSlashRawUnits;
    }

    return m_doubleVoteSlashRawUnits;
}

std::uint64_t ValidatorPenaltyPolicy::jailEpochsForEvidence(
    const SlashingEvidenceRecord& evidence
) const {
    const ValidatorPenaltyAction action = actionForEvidence(evidence);
    if (action == ValidatorPenaltyAction::JAIL ||
        action == ValidatorPenaltyAction::SLASH) {
        return m_defaultJailEpochs;
    }

    if (action == ValidatorPenaltyAction::TOMBSTONE) {
        return 0;
    }

    return 0;
}

std::string ValidatorPenaltyPolicy::serialize() const {
    std::ostringstream output;
    output << "ValidatorPenaltyPolicy{"
           << "doubleVoteSlashRawUnits=" << m_doubleVoteSlashRawUnits
           << ";equivocationSlashRawUnits=" << m_equivocationSlashRawUnits
           << ";defaultJailEpochs=" << m_defaultJailEpochs
           << ";tombstoneEquivocation=" << (m_tombstoneEquivocation ? "true" : "false")
           << "}";
    return output.str();
}

ValidatorPenaltyDecision::ValidatorPenaltyDecision()
    : m_penaltyId(""),
      m_evidenceId(""),
      m_validatorAddress(""),
      m_evidenceType(SlashingEvidenceType::UNKNOWN),
      m_evidenceSeverity(SlashingEvidenceSeverity::UNKNOWN),
      m_action(ValidatorPenaltyAction::NONE),
      m_slashAmountRawUnits(0),
      m_jailEpochs(0),
      m_createdAt(0) {}

ValidatorPenaltyDecision::ValidatorPenaltyDecision(
    std::string penaltyId,
    std::string evidenceId,
    std::string validatorAddress,
    SlashingEvidenceType evidenceType,
    SlashingEvidenceSeverity evidenceSeverity,
    ValidatorPenaltyAction action,
    std::int64_t slashAmountRawUnits,
    std::uint64_t jailEpochs,
    std::int64_t createdAt
) : m_penaltyId(std::move(penaltyId)),
    m_evidenceId(std::move(evidenceId)),
    m_validatorAddress(std::move(validatorAddress)),
    m_evidenceType(evidenceType),
    m_evidenceSeverity(evidenceSeverity),
    m_action(action),
    m_slashAmountRawUnits(slashAmountRawUnits),
    m_jailEpochs(jailEpochs),
    m_createdAt(createdAt) {}

const std::string& ValidatorPenaltyDecision::penaltyId() const { return m_penaltyId; }
const std::string& ValidatorPenaltyDecision::evidenceId() const { return m_evidenceId; }
const std::string& ValidatorPenaltyDecision::validatorAddress() const { return m_validatorAddress; }
SlashingEvidenceType ValidatorPenaltyDecision::evidenceType() const { return m_evidenceType; }
SlashingEvidenceSeverity ValidatorPenaltyDecision::evidenceSeverity() const { return m_evidenceSeverity; }
ValidatorPenaltyAction ValidatorPenaltyDecision::action() const { return m_action; }
std::int64_t ValidatorPenaltyDecision::slashAmountRawUnits() const { return m_slashAmountRawUnits; }
std::uint64_t ValidatorPenaltyDecision::jailEpochs() const { return m_jailEpochs; }
std::int64_t ValidatorPenaltyDecision::createdAt() const { return m_createdAt; }

bool ValidatorPenaltyDecision::isValid() const {
    if (!isSafeScalar(m_penaltyId) ||
        !isSafeScalar(m_evidenceId) ||
        !isSafeScalar(m_validatorAddress)) {
        return false;
    }

    if (m_evidenceType == SlashingEvidenceType::UNKNOWN ||
        m_evidenceSeverity == SlashingEvidenceSeverity::UNKNOWN) {
        return false;
    }

    if (m_action == ValidatorPenaltyAction::NONE) {
        return false;
    }

    if (m_slashAmountRawUnits < 0 || m_createdAt <= 0) {
        return false;
    }

    if ((m_action == ValidatorPenaltyAction::WARNING ||
         m_action == ValidatorPenaltyAction::JAIL) &&
        m_slashAmountRawUnits != 0) {
        return false;
    }

    if (m_action == ValidatorPenaltyAction::WARNING && m_jailEpochs != 0) {
        return false;
    }

    if (m_action == ValidatorPenaltyAction::JAIL && m_jailEpochs == 0) {
        return false;
    }

    if (m_action == ValidatorPenaltyAction::SLASH &&
        (m_slashAmountRawUnits <= 0 || m_jailEpochs == 0)) {
        return false;
    }

    if (m_action == ValidatorPenaltyAction::TOMBSTONE && m_slashAmountRawUnits <= 0) {
        return false;
    }

    return m_penaltyId == computePenaltyId(
        m_evidenceId,
        m_validatorAddress,
        m_evidenceType,
        m_evidenceSeverity,
        m_action,
        m_slashAmountRawUnits,
        m_jailEpochs
    );
}

bool ValidatorPenaltyDecision::slashable() const {
    return m_action == ValidatorPenaltyAction::SLASH ||
           m_action == ValidatorPenaltyAction::TOMBSTONE;
}

bool ValidatorPenaltyDecision::jailsValidator() const {
    return m_action == ValidatorPenaltyAction::JAIL ||
           m_action == ValidatorPenaltyAction::SLASH;
}

bool ValidatorPenaltyDecision::tombstonesValidator() const {
    return m_action == ValidatorPenaltyAction::TOMBSTONE;
}

std::string ValidatorPenaltyDecision::serialize() const {
    std::ostringstream output;
    output << "ValidatorPenaltyDecision{"
           << "penaltyId=" << m_penaltyId
           << ";evidenceId=" << m_evidenceId
           << ";validatorAddress=" << m_validatorAddress
           << ";evidenceType=" << slashingEvidenceTypeToString(m_evidenceType)
           << ";evidenceSeverity=" << slashingEvidenceSeverityToString(m_evidenceSeverity)
           << ";action=" << validatorPenaltyActionToString(m_action)
           << ";slashAmountRawUnits=" << m_slashAmountRawUnits
           << ";jailEpochs=" << m_jailEpochs
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

ValidatorPenaltyDecision ValidatorPenaltyDecision::create(
    const SlashingEvidenceRecord& evidence,
    const ValidatorPenaltyPolicy& policy,
    std::int64_t createdAt
) {
    if (!evidence.isValid()) {
        throw std::invalid_argument("Cannot create penalty decision from invalid evidence.");
    }

    if (!policy.isValid()) {
        throw std::invalid_argument("Cannot create penalty decision from invalid policy.");
    }

    const ValidatorPenaltyAction action = policy.actionForEvidence(evidence);
    const std::int64_t slashAmount = policy.slashAmountForEvidence(evidence);
    const std::uint64_t jailEpochs = policy.jailEpochsForEvidence(evidence);
    const std::string penaltyId = computePenaltyId(evidence, action, slashAmount, jailEpochs);

    return ValidatorPenaltyDecision(
        penaltyId,
        evidence.evidenceId(),
        evidence.validatorAddress(),
        evidence.type(),
        evidence.severity(),
        action,
        slashAmount,
        jailEpochs,
        createdAt
    );
}

ValidatorPenaltyDecision ValidatorPenaltyDecision::deserialize(
    const std::string& serialized
) {
    const std::map<std::string, std::string> fields =
        parseObjectFields(serialized, "ValidatorPenaltyDecision");

    requireExactFields(
        fields,
        {
            "penaltyId",
            "evidenceId",
            "validatorAddress",
            "evidenceType",
            "evidenceSeverity",
            "action",
            "slashAmountRawUnits",
            "jailEpochs",
            "createdAt"
        },
        "ValidatorPenaltyDecision"
    );

    ValidatorPenaltyDecision decision(
        requireField(fields, "penaltyId", "ValidatorPenaltyDecision"),
        requireField(fields, "evidenceId", "ValidatorPenaltyDecision"),
        requireField(fields, "validatorAddress", "ValidatorPenaltyDecision"),
        slashingEvidenceTypeFromString(requireField(fields, "evidenceType", "ValidatorPenaltyDecision")),
        slashingEvidenceSeverityFromString(requireField(fields, "evidenceSeverity", "ValidatorPenaltyDecision")),
        validatorPenaltyActionFromString(requireField(fields, "action", "ValidatorPenaltyDecision")),
        parseI64Strict(requireField(fields, "slashAmountRawUnits", "ValidatorPenaltyDecision"), "slashAmountRawUnits"),
        parseU64Strict(requireField(fields, "jailEpochs", "ValidatorPenaltyDecision"), "jailEpochs"),
        parseI64Strict(requireField(fields, "createdAt", "ValidatorPenaltyDecision"), "createdAt")
    );

    if (!decision.isValid()) {
        throw std::invalid_argument("Serialized ValidatorPenaltyDecision is invalid.");
    }

    const std::string expectedPenaltyId = computePenaltyId(
        decision.evidenceId(),
        decision.validatorAddress(),
        decision.evidenceType(),
        decision.evidenceSeverity(),
        decision.action(),
        decision.slashAmountRawUnits(),
        decision.jailEpochs()
    );

    if (decision.penaltyId() != expectedPenaltyId) {
        throw std::invalid_argument(
            "Serialized ValidatorPenaltyDecision penalty id does not match its contents."
        );
    }

    return decision;
}

std::string validatorPenaltyApplicationStatusToString(
    ValidatorPenaltyApplicationStatus status
) {
    switch (status) {
        case ValidatorPenaltyApplicationStatus::APPLIED: return "APPLIED";
        case ValidatorPenaltyApplicationStatus::DUPLICATE: return "DUPLICATE";
        case ValidatorPenaltyApplicationStatus::REJECTED: return "REJECTED";
        default: return "REJECTED";
    }
}

ValidatorPenaltyApplicationResult::ValidatorPenaltyApplicationResult()
    : m_status(ValidatorPenaltyApplicationStatus::REJECTED),
      m_reason("Uninitialized validator penalty application result."),
      m_decision(std::nullopt) {}

ValidatorPenaltyApplicationResult::ValidatorPenaltyApplicationResult(
    ValidatorPenaltyApplicationStatus status,
    std::string reason,
    std::optional<ValidatorPenaltyDecision> decision
) : m_status(status),
    m_reason(std::move(reason)),
    m_decision(std::move(decision)) {}

ValidatorPenaltyApplicationStatus ValidatorPenaltyApplicationResult::status() const {
    return m_status;
}

const std::string& ValidatorPenaltyApplicationResult::reason() const {
    return m_reason;
}

const std::optional<ValidatorPenaltyDecision>& ValidatorPenaltyApplicationResult::decision() const {
    return m_decision;
}

bool ValidatorPenaltyApplicationResult::applied() const {
    return m_status == ValidatorPenaltyApplicationStatus::APPLIED;
}

bool ValidatorPenaltyApplicationResult::duplicate() const {
    return m_status == ValidatorPenaltyApplicationStatus::DUPLICATE;
}

bool ValidatorPenaltyApplicationResult::rejected() const {
    return m_status == ValidatorPenaltyApplicationStatus::REJECTED;
}

std::string ValidatorPenaltyApplicationResult::serialize() const {
    std::ostringstream output;
    output << "ValidatorPenaltyApplicationResult{"
           << "status=" << validatorPenaltyApplicationStatusToString(m_status)
           << ";reason=" << m_reason
           << ";decision=" << (m_decision.has_value() ? m_decision->serialize() : "none")
           << "}";
    return output.str();
}

ValidatorPenaltyLedger::ValidatorPenaltyLedger()
    : m_decisionsByPenaltyId(),
      m_penaltyIdByEvidenceId(),
      m_penaltyIdsByValidator() {}

ValidatorPenaltyApplicationResult ValidatorPenaltyLedger::applyEvidence(
    const SlashingEvidenceRecord& evidence,
    const ValidatorPenaltyPolicy& policy,
    std::int64_t now
) {
    if (!evidence.isValid()) {
        return ValidatorPenaltyApplicationResult(
            ValidatorPenaltyApplicationStatus::REJECTED,
            "Cannot apply penalty for invalid evidence.",
            std::nullopt
        );
    }

    if (!policy.isValid()) {
        return ValidatorPenaltyApplicationResult(
            ValidatorPenaltyApplicationStatus::REJECTED,
            "Cannot apply penalty with invalid policy.",
            std::nullopt
        );
    }

    if (now <= 0) {
        return ValidatorPenaltyApplicationResult(
            ValidatorPenaltyApplicationStatus::REJECTED,
            "Penalty application timestamp must be positive.",
            std::nullopt
        );
    }

    if (containsEvidence(evidence.evidenceId())) {
        return ValidatorPenaltyApplicationResult(
            ValidatorPenaltyApplicationStatus::DUPLICATE,
            "Evidence already has an applied penalty decision.",
            *decisionByEvidenceId(evidence.evidenceId())
        );
    }

    ValidatorPenaltyDecision decision;
    try {
        decision = ValidatorPenaltyDecision::create(evidence, policy, now);
    } catch (const std::exception& error) {
        return ValidatorPenaltyApplicationResult(
            ValidatorPenaltyApplicationStatus::REJECTED,
            error.what(),
            std::nullopt
        );
    }

    return applyDecision(decision);
}

ValidatorPenaltyApplicationResult ValidatorPenaltyLedger::applyDecision(
    const ValidatorPenaltyDecision& decision
) {
    if (!decision.isValid()) {
        return ValidatorPenaltyApplicationResult(
            ValidatorPenaltyApplicationStatus::REJECTED,
            "Validator penalty decision is invalid.",
            std::nullopt
        );
    }

    if (containsEvidence(decision.evidenceId())) {
        const ValidatorPenaltyDecision* existing =
            decisionByEvidenceId(decision.evidenceId());
        const std::optional<ValidatorPenaltyDecision> existingDecision =
            existing == nullptr
                ? std::nullopt
                : std::optional<ValidatorPenaltyDecision>(*existing);

        return ValidatorPenaltyApplicationResult(
            ValidatorPenaltyApplicationStatus::DUPLICATE,
            "Penalty decision is already applied.",
            existingDecision
        );
    }

    if (containsPenalty(decision.penaltyId())) {
        const ValidatorPenaltyDecision* existing =
            decisionByPenaltyId(decision.penaltyId());
        const std::optional<ValidatorPenaltyDecision> existingDecision =
            existing == nullptr
                ? std::nullopt
                : std::optional<ValidatorPenaltyDecision>(*existing);

        return ValidatorPenaltyApplicationResult(
            ValidatorPenaltyApplicationStatus::DUPLICATE,
            "Penalty decision is already applied.",
            existingDecision
        );
    }

    m_decisionsByPenaltyId.emplace(decision.penaltyId(), decision);
    m_penaltyIdByEvidenceId.emplace(decision.evidenceId(), decision.penaltyId());
    m_penaltyIdsByValidator[decision.validatorAddress()].push_back(decision.penaltyId());

    if (!isValid()) {
        m_penaltyIdsByValidator[decision.validatorAddress()].pop_back();
        if (m_penaltyIdsByValidator[decision.validatorAddress()].empty()) {
            m_penaltyIdsByValidator.erase(decision.validatorAddress());
        }
        m_penaltyIdByEvidenceId.erase(decision.evidenceId());
        m_decisionsByPenaltyId.erase(decision.penaltyId());

        return ValidatorPenaltyApplicationResult(
            ValidatorPenaltyApplicationStatus::REJECTED,
            "Penalty ledger failed post-apply audit.",
            std::nullopt
        );
    }

    return ValidatorPenaltyApplicationResult(
        ValidatorPenaltyApplicationStatus::APPLIED,
        "Penalty decision applied.",
        decision
    );
}

bool ValidatorPenaltyLedger::containsEvidence(const std::string& evidenceId) const {
    return m_penaltyIdByEvidenceId.find(evidenceId) != m_penaltyIdByEvidenceId.end();
}

bool ValidatorPenaltyLedger::containsPenalty(const std::string& penaltyId) const {
    return m_decisionsByPenaltyId.find(penaltyId) != m_decisionsByPenaltyId.end();
}

const ValidatorPenaltyDecision* ValidatorPenaltyLedger::decisionByEvidenceId(
    const std::string& evidenceId
) const {
    const auto penalty = m_penaltyIdByEvidenceId.find(evidenceId);
    if (penalty == m_penaltyIdByEvidenceId.end()) {
        return nullptr;
    }
    return decisionByPenaltyId(penalty->second);
}

const ValidatorPenaltyDecision* ValidatorPenaltyLedger::decisionByPenaltyId(
    const std::string& penaltyId
) const {
    const auto decision = m_decisionsByPenaltyId.find(penaltyId);
    return decision == m_decisionsByPenaltyId.end() ? nullptr : &decision->second;
}

std::vector<ValidatorPenaltyDecision> ValidatorPenaltyLedger::allDecisions() const {
    std::vector<ValidatorPenaltyDecision> decisions;
    decisions.reserve(m_decisionsByPenaltyId.size());
    for (const auto& [penaltyId, decision] : m_decisionsByPenaltyId) {
        (void)penaltyId;
        decisions.push_back(decision);
    }
    return decisions;
}

std::vector<ValidatorPenaltyDecision> ValidatorPenaltyLedger::decisionsForValidator(
    const std::string& validatorAddress
) const {
    std::vector<ValidatorPenaltyDecision> decisions;
    const auto found = m_penaltyIdsByValidator.find(validatorAddress);
    if (found == m_penaltyIdsByValidator.end()) {
        return decisions;
    }

    decisions.reserve(found->second.size());
    for (const auto& penaltyId : found->second) {
        const ValidatorPenaltyDecision* decision = decisionByPenaltyId(penaltyId);
        if (decision != nullptr) {
            decisions.push_back(*decision);
        }
    }
    return decisions;
}

std::int64_t ValidatorPenaltyLedger::totalSlashAmountForValidator(
    const std::string& validatorAddress
) const {
    std::int64_t total = 0;
    for (const auto& decision : decisionsForValidator(validatorAddress)) {
        const std::int64_t slash = decision.slashAmountRawUnits();
        if (slash > 0 && total > std::numeric_limits<std::int64_t>::max() - slash) {
            return std::numeric_limits<std::int64_t>::max();
        }
        total += slash;
    }
    return total;
}

bool ValidatorPenaltyLedger::validatorIsJailed(const std::string& validatorAddress) const {
    for (const auto& decision : decisionsForValidator(validatorAddress)) {
        if (decision.jailsValidator()) {
            return true;
        }
    }
    return false;
}

bool ValidatorPenaltyLedger::validatorIsTombstoned(const std::string& validatorAddress) const {
    for (const auto& decision : decisionsForValidator(validatorAddress)) {
        if (decision.tombstonesValidator()) {
            return true;
        }
    }
    return false;
}

std::size_t ValidatorPenaltyLedger::size() const {
    return m_decisionsByPenaltyId.size();
}

bool ValidatorPenaltyLedger::isValid() const {
    for (const auto& [penaltyId, decision] : m_decisionsByPenaltyId) {
        if (penaltyId != decision.penaltyId() || !decision.isValid()) {
            return false;
        }

        const auto evidence = m_penaltyIdByEvidenceId.find(decision.evidenceId());
        if (evidence == m_penaltyIdByEvidenceId.end() || evidence->second != penaltyId) {
            return false;
        }
    }

    for (const auto& [evidenceId, penaltyId] : m_penaltyIdByEvidenceId) {
        (void)evidenceId;
        if (m_decisionsByPenaltyId.find(penaltyId) == m_decisionsByPenaltyId.end()) {
            return false;
        }
    }

    for (const auto& [validatorAddress, penaltyIds] : m_penaltyIdsByValidator) {
        if (!isSafeScalar(validatorAddress) || penaltyIds.empty()) {
            return false;
        }

        for (const auto& penaltyId : penaltyIds) {
            const ValidatorPenaltyDecision* decision = decisionByPenaltyId(penaltyId);
            if (decision == nullptr || decision->validatorAddress() != validatorAddress) {
                return false;
            }
        }
    }

    return true;
}

std::string ValidatorPenaltyLedger::serialize() const {
    std::ostringstream output;
    output << "ValidatorPenaltyLedger{size=" << m_decisionsByPenaltyId.size()
           << ";decisions=[";
    bool first = true;
    for (const auto& [penaltyId, decision] : m_decisionsByPenaltyId) {
        (void)penaltyId;
        if (!first) {
            output << ",";
        }
        output << decision.serialize();
        first = false;
    }
    output << "]}";
    return output.str();
}

} // namespace nodo::consensus
