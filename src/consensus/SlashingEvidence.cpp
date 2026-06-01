#include "consensus/SlashingEvidence.hpp"

#include "crypto/hash.h"

#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::consensus {

namespace {

bool isSafeScalar(const std::string& value, std::size_t maxSize = 512) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        if (character == '\n' ||
            character == '\r' ||
            character == '\t' ||
            character == '{' ||
            character == '}' ||
            character == '[' ||
            character == ']') {
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
    int bracketDepth = 0;

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current = value[index];

        if (current == '{') {
            ++braceDepth;
        } else if (current == '}') {
            --braceDepth;
        } else if (current == '[') {
            ++bracketDepth;
        } else if (current == ']') {
            --bracketDepth;
        }

        if (braceDepth < 0 || bracketDepth < 0) {
            throw std::invalid_argument("Malformed nested evidence serialization.");
        }

        if (current == separator && braceDepth == 0 && bracketDepth == 0) {
            if (index == start) {
                throw std::invalid_argument("Empty evidence serialization field.");
            }

            parts.push_back(value.substr(start, index - start));
            start = index + 1;
        }
    }

    if (braceDepth != 0 || bracketDepth != 0) {
        throw std::invalid_argument("Unbalanced evidence serialization.");
    }

    if (start >= value.size()) {
        throw std::invalid_argument("Evidence serialization has trailing separator.");
    }

    parts.push_back(value.substr(start));
    return parts;
}

std::map<std::string, std::string> parseObjectFields(
    const std::string& serialized,
    const std::string& typeName
) {
    const std::string prefix = typeName + "{";

    if (serialized.rfind(prefix, 0) != 0 ||
        serialized.size() <= prefix.size() ||
        serialized.back() != '}') {
        throw std::invalid_argument("Serialized data is not a " + typeName + ".");
    }

    const std::string body =
        serialized.substr(prefix.size(), serialized.size() - prefix.size() - 1);

    if (body.empty()) {
        throw std::invalid_argument("Serialized " + typeName + " is empty.");
    }

    std::map<std::string, std::string> fields;
    for (const std::string& part : splitTopLevel(body, ';')) {
        const std::size_t separator = part.find('=');
        if (separator == std::string::npos ||
            separator == 0 ||
            separator + 1 >= part.size()) {
            throw std::invalid_argument("Malformed serialized " + typeName + " field.");
        }

        const std::string key = part.substr(0, separator);
        const std::string value = part.substr(separator + 1);

        if (!fields.emplace(key, value).second) {
            throw std::invalid_argument("Duplicate serialized " + typeName + " field: " + key);
        }
    }

    return fields;
}

void requireExactFields(
    const std::map<std::string, std::string>& fields,
    const std::set<std::string>& expected,
    const std::string& typeName
) {
    for (const std::string& key : expected) {
        if (fields.find(key) == fields.end()) {
            throw std::invalid_argument("Missing serialized " + typeName + " field: " + key);
        }
    }

    for (const auto& entry : fields) {
        if (expected.find(entry.first) == expected.end()) {
            throw std::invalid_argument("Unknown serialized " + typeName + " field: " + entry.first);
        }
    }
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
    const std::int64_t parsed = static_cast<std::int64_t>(
        std::stoll(value, &parsedSize)
    );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

bool sameValidatorHeightRound(const ValidatorVoteRecord& left, const ValidatorVoteRecord& right) {
    return !left.validatorAddress().empty() &&
           left.validatorAddress() == right.validatorAddress() &&
           left.blockIndex() == right.blockIndex() &&
           left.round() == right.round();
}

bool sameVoteTarget(const ValidatorVoteRecord& left, const ValidatorVoteRecord& right) {
    return left.blockHash() == right.blockHash() &&
           left.previousHash() == right.previousHash() &&
           left.decision() == right.decision() &&
           left.reasonHash() == right.reasonHash();
}

} // namespace

std::string slashingEvidenceTypeToString(SlashingEvidenceType type) {
    switch (type) {
        case SlashingEvidenceType::DOUBLE_VOTE:
            return "DOUBLE_VOTE";
        case SlashingEvidenceType::INVALID_SIGNATURE:
            return "INVALID_SIGNATURE";
        case SlashingEvidenceType::EQUIVOCATION:
            return "EQUIVOCATION";
        case SlashingEvidenceType::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

SlashingEvidenceType slashingEvidenceTypeFromString(const std::string& value) {
    if (value == "DOUBLE_VOTE") {
        return SlashingEvidenceType::DOUBLE_VOTE;
    }

    if (value == "INVALID_SIGNATURE") {
        return SlashingEvidenceType::INVALID_SIGNATURE;
    }

    if (value == "EQUIVOCATION") {
        return SlashingEvidenceType::EQUIVOCATION;
    }

    return SlashingEvidenceType::UNKNOWN;
}

std::string slashingEvidenceSeverityToString(SlashingEvidenceSeverity severity) {
    switch (severity) {
        case SlashingEvidenceSeverity::WARNING:
            return "WARNING";
        case SlashingEvidenceSeverity::SLASHABLE:
            return "SLASHABLE";
        case SlashingEvidenceSeverity::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

SlashingEvidenceSeverity slashingEvidenceSeverityFromString(const std::string& value) {
    if (value == "WARNING") {
        return SlashingEvidenceSeverity::WARNING;
    }

    if (value == "SLASHABLE") {
        return SlashingEvidenceSeverity::SLASHABLE;
    }

    return SlashingEvidenceSeverity::UNKNOWN;
}

std::string slashingEvidenceValidationStatusToString(
    SlashingEvidenceValidationStatus status
) {
    switch (status) {
        case SlashingEvidenceValidationStatus::ACCEPTED:
            return "ACCEPTED";
        case SlashingEvidenceValidationStatus::DUPLICATE:
            return "DUPLICATE";
        case SlashingEvidenceValidationStatus::REJECTED:
        default:
            return "REJECTED";
    }
}

SlashingEvidenceRecord::SlashingEvidenceRecord()
    : m_evidenceId(),
      m_type(SlashingEvidenceType::UNKNOWN),
      m_validatorAddress(),
      m_payloadHash(),
      m_severity(SlashingEvidenceSeverity::UNKNOWN),
      m_createdAt(0) {}

SlashingEvidenceRecord::SlashingEvidenceRecord(
    std::string evidenceId,
    SlashingEvidenceType type,
    std::string validatorAddress,
    std::string payloadHash,
    SlashingEvidenceSeverity severity,
    std::int64_t createdAt
) : m_evidenceId(std::move(evidenceId)),
    m_type(type),
    m_validatorAddress(std::move(validatorAddress)),
    m_payloadHash(std::move(payloadHash)),
    m_severity(severity),
    m_createdAt(createdAt) {}

const std::string& SlashingEvidenceRecord::evidenceId() const {
    return m_evidenceId;
}

SlashingEvidenceType SlashingEvidenceRecord::type() const {
    return m_type;
}

const std::string& SlashingEvidenceRecord::validatorAddress() const {
    return m_validatorAddress;
}

const std::string& SlashingEvidenceRecord::payloadHash() const {
    return m_payloadHash;
}

SlashingEvidenceSeverity SlashingEvidenceRecord::severity() const {
    return m_severity;
}

std::int64_t SlashingEvidenceRecord::createdAt() const {
    return m_createdAt;
}

bool SlashingEvidenceRecord::isValid() const {
    return isSafeScalar(m_evidenceId, 160) &&
           m_type != SlashingEvidenceType::UNKNOWN &&
           isSafeScalar(m_validatorAddress, 200) &&
           isSafeScalar(m_payloadHash, 160) &&
           m_severity != SlashingEvidenceSeverity::UNKNOWN &&
           m_createdAt > 0;
}

std::string SlashingEvidenceRecord::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceRecord{"
           << "evidenceId=" << m_evidenceId
           << ";type=" << slashingEvidenceTypeToString(m_type)
           << ";validatorAddress=" << m_validatorAddress
           << ";payloadHash=" << m_payloadHash
           << ";severity=" << slashingEvidenceSeverityToString(m_severity)
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

SlashingEvidenceRecord SlashingEvidenceRecord::deserialize(
    const std::string& serialized
) {
    const auto fields = parseObjectFields(serialized, "SlashingEvidenceRecord");

    requireExactFields(
        fields,
        {
            "evidenceId",
            "type",
            "validatorAddress",
            "payloadHash",
            "severity",
            "createdAt"
        },
        "SlashingEvidenceRecord"
    );

    SlashingEvidenceRecord record(
        fields.at("evidenceId"),
        slashingEvidenceTypeFromString(fields.at("type")),
        fields.at("validatorAddress"),
        fields.at("payloadHash"),
        slashingEvidenceSeverityFromString(fields.at("severity")),
        parseI64Strict(fields.at("createdAt"), "createdAt")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Serialized slashing evidence record is invalid.");
    }

    return record;
}

DoubleVoteEvidence::DoubleVoteEvidence()
    : m_firstVote(),
      m_secondVote(),
      m_detectedAt(0) {}

DoubleVoteEvidence::DoubleVoteEvidence(
    ValidatorVoteRecord firstVote,
    ValidatorVoteRecord secondVote,
    std::int64_t detectedAt
) : m_firstVote(std::move(firstVote)),
    m_secondVote(std::move(secondVote)),
    m_detectedAt(detectedAt) {}

const ValidatorVoteRecord& DoubleVoteEvidence::firstVote() const {
    return m_firstVote;
}

const ValidatorVoteRecord& DoubleVoteEvidence::secondVote() const {
    return m_secondVote;
}

std::int64_t DoubleVoteEvidence::detectedAt() const {
    return m_detectedAt;
}

bool DoubleVoteEvidence::isConflictPair() const {
    return m_detectedAt > 0 &&
           sameValidatorHeightRound(m_firstVote, m_secondVote) &&
           !sameVoteTarget(m_firstVote, m_secondVote);
}

std::string DoubleVoteEvidence::validatorAddress() const {
    return isConflictPair() ? m_firstVote.validatorAddress() : "";
}

std::string DoubleVoteEvidence::payload() const {
    std::ostringstream output;
    output << "DoubleVoteEvidencePayload{"
           << "detectedAt=" << m_detectedAt
           << ";firstVote=" << m_firstVote.serialize()
           << ";secondVote=" << m_secondVote.serialize()
           << "}";
    return output.str();
}

std::string DoubleVoteEvidence::payloadHash() const {
    return hashString("NODO_SLASHING_DOUBLE_VOTE_PAYLOAD_V1|" + payload());
}

std::string DoubleVoteEvidence::evidenceId() const {
    if (!isConflictPair()) {
        return "";
    }

    return hashString("NODO_SLASHING_DOUBLE_VOTE_EVIDENCE_ID_V1|" + payloadHash());
}

SlashingEvidenceRecord DoubleVoteEvidence::toRecord() const {
    return SlashingEvidenceRecord(
        evidenceId(),
        SlashingEvidenceType::DOUBLE_VOTE,
        validatorAddress(),
        payloadHash(),
        SlashingEvidenceSeverity::SLASHABLE,
        m_detectedAt
    );
}

std::string DoubleVoteEvidence::serialize() const {
    std::ostringstream output;
    output << "DoubleVoteEvidence{"
           << "evidenceId=" << evidenceId()
           << ";validatorAddress=" << validatorAddress()
           << ";detectedAt=" << m_detectedAt
           << ";payloadHash=" << payloadHash()
           << ";firstVote=" << m_firstVote.serialize()
           << ";secondVote=" << m_secondVote.serialize()
           << "}";
    return output.str();
}

SlashingEvidenceValidationResult::SlashingEvidenceValidationResult()
    : m_status(SlashingEvidenceValidationStatus::REJECTED),
      m_reason("Uninitialized slashing evidence validation result."),
      m_record() {}

SlashingEvidenceValidationResult::SlashingEvidenceValidationResult(
    SlashingEvidenceValidationStatus status,
    std::string reason,
    SlashingEvidenceRecord record
) : m_status(status),
    m_reason(std::move(reason)),
    m_record(std::move(record)) {}

SlashingEvidenceValidationStatus SlashingEvidenceValidationResult::status() const {
    return m_status;
}

const std::string& SlashingEvidenceValidationResult::reason() const {
    return m_reason;
}

const SlashingEvidenceRecord& SlashingEvidenceValidationResult::record() const {
    return m_record;
}

bool SlashingEvidenceValidationResult::accepted() const {
    return m_status == SlashingEvidenceValidationStatus::ACCEPTED;
}

bool SlashingEvidenceValidationResult::duplicate() const {
    return m_status == SlashingEvidenceValidationStatus::DUPLICATE;
}

bool SlashingEvidenceValidationResult::rejected() const {
    return m_status == SlashingEvidenceValidationStatus::REJECTED;
}

std::string SlashingEvidenceValidationResult::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceValidationResult{"
           << "status=" << slashingEvidenceValidationStatusToString(m_status)
           << ";reason=" << m_reason
           << ";record=" << m_record.serialize()
           << "}";
    return output.str();
}

SlashingEvidenceValidationResult SlashingEvidenceVerifier::validateDoubleVoteStructure(
    const DoubleVoteEvidence& evidence
) {
    if (!evidence.isConflictPair()) {
        return SlashingEvidenceValidationResult(
            SlashingEvidenceValidationStatus::REJECTED,
            "Votes do not form a same-validator same-height same-round conflict.",
            SlashingEvidenceRecord()
        );
    }

    const SlashingEvidenceRecord record = evidence.toRecord();
    if (!record.isValid()) {
        return SlashingEvidenceValidationResult(
            SlashingEvidenceValidationStatus::REJECTED,
            "Double-vote evidence record is invalid.",
            record
        );
    }

    return SlashingEvidenceValidationResult(
        SlashingEvidenceValidationStatus::ACCEPTED,
        "Double-vote evidence structure is valid.",
        record
    );
}

SlashingEvidenceValidationResult SlashingEvidenceVerifier::verifyDoubleVoteEvidence(
    const DoubleVoteEvidence& evidence,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    const SlashingEvidenceValidationResult structural =
        validateDoubleVoteStructure(evidence);

    if (!structural.accepted()) {
        return structural;
    }

    if (!bothVotesVerify(evidence, policy, provider)) {
        return SlashingEvidenceValidationResult(
            SlashingEvidenceValidationStatus::REJECTED,
            "Double-vote evidence contains at least one vote with an invalid signature.",
            structural.record()
        );
    }

    return SlashingEvidenceValidationResult(
        SlashingEvidenceValidationStatus::ACCEPTED,
        "Double-vote evidence is structurally valid and both signatures verify.",
        structural.record()
    );
}

bool SlashingEvidenceVerifier::bothVotesVerify(
    const DoubleVoteEvidence& evidence,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    return evidence.firstVote().verify(policy, provider) &&
           evidence.secondVote().verify(policy, provider);
}

} // namespace nodo::consensus
