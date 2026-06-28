#include "node/SlashingEvidenceMessages.hpp"

#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

bool isSafeScalar(const std::string& value, std::size_t maxSize = 200) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.' ||
            character == ':' ||
            character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> splitTopLevel(
    const std::string& value,
    char separator
) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    std::size_t depth = 0;
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char character = value[index];
        if (character == '{') {
            ++depth;
        } else if (character == '}') {
            if (depth == 0) {
                throw std::invalid_argument("Unbalanced message payload.");
            }
            --depth;
        } else if (character == separator && depth == 0) {
            parts.push_back(value.substr(start, index - start));
            start = index + 1;
        }
    }
    if (depth != 0) {
        throw std::invalid_argument("Unbalanced message payload.");
    }
    parts.push_back(value.substr(start));
    return parts;
}

std::map<std::string, std::string> parseObjectFields(
    const std::string& serialized,
    const std::string& typeName
) {
    const std::string prefix = typeName + "{";
    if (serialized.size() <= prefix.size() ||
        serialized.rfind(prefix, 0) != 0 ||
        serialized.back() != '}') {
        throw std::invalid_argument("Invalid " + typeName + " encoding.");
    }

    const std::string body = serialized.substr(
        prefix.size(), serialized.size() - prefix.size() - 1
    );
    std::map<std::string, std::string> fields;
    for (const std::string& entry : splitTopLevel(body, ';')) {
        const std::size_t separator = entry.find('=');
        if (separator == std::string::npos || separator == 0) {
            throw std::invalid_argument("Invalid " + typeName + " field.");
        }
        const std::string key = entry.substr(0, separator);
        if (!fields.emplace(key, entry.substr(separator + 1)).second) {
            throw std::invalid_argument("Duplicate " + typeName + " field.");
        }
    }
    return fields;
}

void requireExactFields(
    const std::map<std::string, std::string>& fields,
    const std::set<std::string>& expected,
    const std::string& typeName
) {
    if (fields.size() != expected.size()) {
        throw std::invalid_argument("Unexpected " + typeName + " field count.");
    }
    for (const std::string& field : expected) {
        if (fields.find(field) == fields.end()) {
            throw std::invalid_argument("Missing " + typeName + " field: " + field);
        }
    }
}

std::int64_t parseI64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty integer field: " + fieldName);
    }
    std::size_t consumed = 0;
    std::int64_t parsed = 0;
    try {
        parsed = std::stoll(value, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid integer field: " + fieldName);
    }
    if (consumed != value.size()) {
        throw std::invalid_argument("Invalid integer field: " + fieldName);
    }
    return parsed;
}

} // namespace

SlashingEvidenceAnnouncement::SlashingEvidenceAnnouncement()
    : m_networkId(),
      m_chainId(),
      m_announcerNodeId(),
      m_evidence(),
      m_announcedAt(0) {}

SlashingEvidenceAnnouncement::SlashingEvidenceAnnouncement(
    std::string networkId,
    std::string chainId,
    std::string announcerNodeId,
    consensus::DoubleVoteEvidence evidence,
    std::int64_t announcedAt
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_announcerNodeId(std::move(announcerNodeId)),
    m_evidence(std::move(evidence)),
    m_announcedAt(announcedAt) {}

const std::string& SlashingEvidenceAnnouncement::networkId() const {
    return m_networkId;
}

const std::string& SlashingEvidenceAnnouncement::chainId() const {
    return m_chainId;
}

const std::string& SlashingEvidenceAnnouncement::announcerNodeId() const {
    return m_announcerNodeId;
}

const consensus::DoubleVoteEvidence& SlashingEvidenceAnnouncement::evidence() const {
    return m_evidence;
}

consensus::SlashingEvidenceRecord SlashingEvidenceAnnouncement::record() const {
    return m_evidence.toRecord();
}

std::int64_t SlashingEvidenceAnnouncement::announcedAt() const {
    return m_announcedAt;
}

bool SlashingEvidenceAnnouncement::isValid() const {
    return isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_announcerNodeId) &&
           consensus::SlashingEvidenceVerifier::validateDoubleVoteStructure(
               m_evidence
           ).accepted() &&
           m_announcedAt > 0;
}

std::string SlashingEvidenceAnnouncement::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceAnnouncement{"
           << "networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";announcerNodeId=" << m_announcerNodeId
           << ";announcedAt=" << m_announcedAt
           << ";evidence=" << m_evidence.serialize()
           << "}";
    return output.str();
}

SlashingEvidenceAnnouncement SlashingEvidenceAnnouncement::deserialize(
    const std::string& serialized
) {
    const auto fields = parseObjectFields(
        serialized, "SlashingEvidenceAnnouncement"
    );
    requireExactFields(
        fields,
        {"networkId", "chainId", "announcerNodeId", "announcedAt", "evidence"},
        "SlashingEvidenceAnnouncement"
    );

    SlashingEvidenceAnnouncement announcement(
        fields.at("networkId"),
        fields.at("chainId"),
        fields.at("announcerNodeId"),
        consensus::DoubleVoteEvidence::deserialize(fields.at("evidence")),
        parseI64Strict(fields.at("announcedAt"), "announcedAt")
    );
    if (!announcement.isValid() || announcement.serialize() != serialized) {
        throw std::invalid_argument(
            "Serialized slashing evidence announcement is invalid or non-canonical."
        );
    }
    return announcement;
}

SlashingEvidenceRequest::SlashingEvidenceRequest()
    : m_requesterNodeId(),
      m_evidenceId(),
      m_requestedAt(0) {}

SlashingEvidenceRequest::SlashingEvidenceRequest(
    std::string requesterNodeId,
    std::string evidenceId,
    std::int64_t requestedAt
) : m_requesterNodeId(std::move(requesterNodeId)),
    m_evidenceId(std::move(evidenceId)),
    m_requestedAt(requestedAt) {}

const std::string& SlashingEvidenceRequest::requesterNodeId() const {
    return m_requesterNodeId;
}

const std::string& SlashingEvidenceRequest::evidenceId() const {
    return m_evidenceId;
}

std::int64_t SlashingEvidenceRequest::requestedAt() const {
    return m_requestedAt;
}

bool SlashingEvidenceRequest::isValid() const {
    return isSafeScalar(m_requesterNodeId) &&
           isSafeScalar(m_evidenceId) &&
           m_requestedAt > 0;
}

std::string SlashingEvidenceRequest::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceRequest{"
           << "requesterNodeId=" << m_requesterNodeId
           << ";evidenceId=" << m_evidenceId
           << ";requestedAt=" << m_requestedAt
           << "}";
    return output.str();
}

} // namespace nodo::node
