#include "node/SlashingEvidenceMessages.hpp"

#include "core/ProtocolLimits.hpp"

#include <algorithm>
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

std::vector<std::string> parseEvidenceIds(const std::string& value) {
    if (value.size() < 2 || value.front() != '[' || value.back() != ']') {
        throw std::invalid_argument("Invalid slashing evidence inventory list.");
    }
    const std::string body = value.substr(1, value.size() - 2);
    if (body.empty()) {
        return {};
    }
    return splitTopLevel(body, ',');
}

std::string serializeEvidenceIds(
    const std::vector<std::string>& evidenceIds
) {
    std::ostringstream output;
    output << "[";
    for (std::size_t index = 0; index < evidenceIds.size(); ++index) {
        if (index > 0) output << ",";
        output << evidenceIds[index];
    }
    output << "]";
    return output.str();
}

} // namespace

SlashingEvidenceAnnouncement::SlashingEvidenceAnnouncement()
    : m_networkId(),
      m_chainId(),
      m_announcerNodeId(),
      m_evidenceType(consensus::SlashingEvidenceType::UNKNOWN),
      m_evidence(),
      m_proposerEquivocationEvidence(),
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
    m_evidenceType(consensus::SlashingEvidenceType::DOUBLE_VOTE),
    m_evidence(std::move(evidence)),
    m_proposerEquivocationEvidence(),
    m_announcedAt(announcedAt) {}

SlashingEvidenceAnnouncement::SlashingEvidenceAnnouncement(
    std::string networkId,
    std::string chainId,
    std::string announcerNodeId,
    consensus::ProposerEquivocationEvidence evidence,
    std::int64_t announcedAt
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_announcerNodeId(std::move(announcerNodeId)),
    m_evidenceType(consensus::SlashingEvidenceType::EQUIVOCATION),
    m_evidence(),
    m_proposerEquivocationEvidence(std::move(evidence)),
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

consensus::SlashingEvidenceType SlashingEvidenceAnnouncement::evidenceType() const {
    return m_evidenceType;
}

const consensus::DoubleVoteEvidence& SlashingEvidenceAnnouncement::evidence() const {
    return m_evidence;
}

const consensus::ProposerEquivocationEvidence&
SlashingEvidenceAnnouncement::proposerEquivocationEvidence() const {
    return m_proposerEquivocationEvidence;
}

consensus::SlashingEvidenceRecord SlashingEvidenceAnnouncement::record() const {
    if (m_evidenceType == consensus::SlashingEvidenceType::DOUBLE_VOTE) {
        return m_evidence.toRecord();
    }
    if (m_evidenceType == consensus::SlashingEvidenceType::EQUIVOCATION) {
        return m_proposerEquivocationEvidence.toRecord();
    }
    return consensus::SlashingEvidenceRecord();
}

std::int64_t SlashingEvidenceAnnouncement::announcedAt() const {
    return m_announcedAt;
}

bool SlashingEvidenceAnnouncement::isValid() const {
    return isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_announcerNodeId) &&
           ((m_evidenceType == consensus::SlashingEvidenceType::DOUBLE_VOTE &&
             consensus::SlashingEvidenceVerifier::validateDoubleVoteStructure(
                 m_evidence
             ).accepted()) ||
            (m_evidenceType == consensus::SlashingEvidenceType::EQUIVOCATION &&
             consensus::SlashingEvidenceVerifier::validateProposerEquivocationStructure(
                 m_proposerEquivocationEvidence
             ).accepted())) &&
           m_announcedAt > 0;
}

std::string SlashingEvidenceAnnouncement::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceAnnouncement{"
           << "networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";announcerNodeId=" << m_announcerNodeId
           << ";announcedAt=" << m_announcedAt
           << ";evidenceType=" << consensus::slashingEvidenceTypeToString(m_evidenceType)
           << ";evidence="
           << (m_evidenceType == consensus::SlashingEvidenceType::EQUIVOCATION
                   ? m_proposerEquivocationEvidence.serialize()
                   : m_evidence.serialize())
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
        {"networkId", "chainId", "announcerNodeId", "announcedAt", "evidenceType", "evidence"},
        "SlashingEvidenceAnnouncement"
    );

    const consensus::SlashingEvidenceType evidenceType =
        consensus::slashingEvidenceTypeFromString(fields.at("evidenceType"));
    SlashingEvidenceAnnouncement announcement;
    if (evidenceType == consensus::SlashingEvidenceType::DOUBLE_VOTE) {
        announcement = SlashingEvidenceAnnouncement(
            fields.at("networkId"),
            fields.at("chainId"),
            fields.at("announcerNodeId"),
            consensus::DoubleVoteEvidence::deserialize(fields.at("evidence")),
            parseI64Strict(fields.at("announcedAt"), "announcedAt")
        );
    } else if (evidenceType == consensus::SlashingEvidenceType::EQUIVOCATION) {
        announcement = SlashingEvidenceAnnouncement(
            fields.at("networkId"),
            fields.at("chainId"),
            fields.at("announcerNodeId"),
            consensus::ProposerEquivocationEvidence::deserialize(fields.at("evidence")),
            parseI64Strict(fields.at("announcedAt"), "announcedAt")
        );
    } else {
        throw std::invalid_argument("Unsupported slashing evidence announcement type.");
    }
    if (!announcement.isValid() || announcement.serialize() != serialized) {
        throw std::invalid_argument(
            "Serialized slashing evidence announcement is invalid or non-canonical."
        );
    }
    return announcement;
}

SlashingEvidenceInventory::SlashingEvidenceInventory()
    : m_networkId(),
      m_chainId(),
      m_announcerNodeId(),
      m_evidenceIds(),
      m_generatedAt(0) {}

SlashingEvidenceInventory::SlashingEvidenceInventory(
    std::string networkId,
    std::string chainId,
    std::string announcerNodeId,
    std::vector<std::string> evidenceIds,
    std::int64_t generatedAt
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_announcerNodeId(std::move(announcerNodeId)),
    m_evidenceIds(std::move(evidenceIds)),
    m_generatedAt(generatedAt) {
    std::sort(m_evidenceIds.begin(), m_evidenceIds.end());
    m_evidenceIds.erase(
        std::unique(m_evidenceIds.begin(), m_evidenceIds.end()),
        m_evidenceIds.end()
    );
}

const std::string& SlashingEvidenceInventory::networkId() const {
    return m_networkId;
}

const std::string& SlashingEvidenceInventory::chainId() const {
    return m_chainId;
}

const std::string& SlashingEvidenceInventory::announcerNodeId() const {
    return m_announcerNodeId;
}

const std::vector<std::string>& SlashingEvidenceInventory::evidenceIds() const {
    return m_evidenceIds;
}

std::int64_t SlashingEvidenceInventory::generatedAt() const {
    return m_generatedAt;
}

bool SlashingEvidenceInventory::isValid() const {
    if (!isSafeScalar(m_networkId) ||
        !isSafeScalar(m_chainId) ||
        !isSafeScalar(m_announcerNodeId) ||
        m_evidenceIds.empty() ||
        m_evidenceIds.size() >
            core::ProtocolLimits::MAX_SLASHING_EVIDENCE_INVENTORY_IDS ||
        m_generatedAt <= 0) {
        return false;
    }
    for (std::size_t index = 0; index < m_evidenceIds.size(); ++index) {
        if (!isSafeScalar(m_evidenceIds[index], 160) ||
            (index > 0 && m_evidenceIds[index - 1] >= m_evidenceIds[index])) {
            return false;
        }
    }
    return true;
}

std::string SlashingEvidenceInventory::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceInventory{"
           << "networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";announcerNodeId=" << m_announcerNodeId
           << ";generatedAt=" << m_generatedAt
           << ";evidenceIds=" << serializeEvidenceIds(m_evidenceIds)
           << "}";
    return output.str();
}

SlashingEvidenceInventory SlashingEvidenceInventory::deserialize(
    const std::string& serialized
) {
    const auto fields = parseObjectFields(
        serialized, "SlashingEvidenceInventory"
    );
    requireExactFields(
        fields,
        {"networkId", "chainId", "announcerNodeId", "generatedAt", "evidenceIds"},
        "SlashingEvidenceInventory"
    );
    SlashingEvidenceInventory inventory(
        fields.at("networkId"),
        fields.at("chainId"),
        fields.at("announcerNodeId"),
        parseEvidenceIds(fields.at("evidenceIds")),
        parseI64Strict(fields.at("generatedAt"), "generatedAt")
    );
    if (!inventory.isValid() || inventory.serialize() != serialized) {
        throw std::invalid_argument(
            "Serialized slashing evidence inventory is invalid or non-canonical."
        );
    }
    return inventory;
}

SlashingEvidenceRequest::SlashingEvidenceRequest()
    : m_networkId(),
      m_chainId(),
      m_requesterNodeId(),
      m_evidenceId(),
      m_requestedAt(0) {}

SlashingEvidenceRequest::SlashingEvidenceRequest(
    std::string networkId,
    std::string chainId,
    std::string requesterNodeId,
    std::string evidenceId,
    std::int64_t requestedAt
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_requesterNodeId(std::move(requesterNodeId)),
    m_evidenceId(std::move(evidenceId)),
    m_requestedAt(requestedAt) {}

const std::string& SlashingEvidenceRequest::networkId() const {
    return m_networkId;
}

const std::string& SlashingEvidenceRequest::chainId() const {
    return m_chainId;
}

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
    return isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_requesterNodeId) &&
           isSafeScalar(m_evidenceId, 160) &&
           m_requestedAt > 0;
}

std::string SlashingEvidenceRequest::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceRequest{"
           << "networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";requesterNodeId=" << m_requesterNodeId
           << ";evidenceId=" << m_evidenceId
           << ";requestedAt=" << m_requestedAt
           << "}";
    return output.str();
}

SlashingEvidenceRequest SlashingEvidenceRequest::deserialize(
    const std::string& serialized
) {
    const auto fields = parseObjectFields(
        serialized, "SlashingEvidenceRequest"
    );
    requireExactFields(
        fields,
        {"networkId", "chainId", "requesterNodeId", "evidenceId", "requestedAt"},
        "SlashingEvidenceRequest"
    );
    SlashingEvidenceRequest request(
        fields.at("networkId"),
        fields.at("chainId"),
        fields.at("requesterNodeId"),
        fields.at("evidenceId"),
        parseI64Strict(fields.at("requestedAt"), "requestedAt")
    );
    if (!request.isValid() || request.serialize() != serialized) {
        throw std::invalid_argument(
            "Serialized slashing evidence request is invalid or non-canonical."
        );
    }
    return request;
}

SlashingEvidenceResponse::SlashingEvidenceResponse()
    : m_networkId(),
      m_chainId(),
      m_responderNodeId(),
      m_evidenceType(consensus::SlashingEvidenceType::UNKNOWN),
      m_evidence(),
      m_proposerEquivocationEvidence(),
      m_respondedAt(0) {}

SlashingEvidenceResponse::SlashingEvidenceResponse(
    std::string networkId,
    std::string chainId,
    std::string responderNodeId,
    consensus::DoubleVoteEvidence evidence,
    std::int64_t respondedAt
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_responderNodeId(std::move(responderNodeId)),
    m_evidenceType(consensus::SlashingEvidenceType::DOUBLE_VOTE),
    m_evidence(std::move(evidence)),
    m_proposerEquivocationEvidence(),
    m_respondedAt(respondedAt) {}

SlashingEvidenceResponse::SlashingEvidenceResponse(
    std::string networkId,
    std::string chainId,
    std::string responderNodeId,
    consensus::ProposerEquivocationEvidence evidence,
    std::int64_t respondedAt
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_responderNodeId(std::move(responderNodeId)),
    m_evidenceType(consensus::SlashingEvidenceType::EQUIVOCATION),
    m_evidence(),
    m_proposerEquivocationEvidence(std::move(evidence)),
    m_respondedAt(respondedAt) {}

const std::string& SlashingEvidenceResponse::networkId() const {
    return m_networkId;
}

const std::string& SlashingEvidenceResponse::chainId() const {
    return m_chainId;
}

const std::string& SlashingEvidenceResponse::responderNodeId() const {
    return m_responderNodeId;
}

consensus::SlashingEvidenceType SlashingEvidenceResponse::evidenceType() const {
    return m_evidenceType;
}

const consensus::DoubleVoteEvidence& SlashingEvidenceResponse::evidence() const {
    return m_evidence;
}

const consensus::ProposerEquivocationEvidence&
SlashingEvidenceResponse::proposerEquivocationEvidence() const {
    return m_proposerEquivocationEvidence;
}

std::int64_t SlashingEvidenceResponse::respondedAt() const {
    return m_respondedAt;
}

bool SlashingEvidenceResponse::isValid() const {
    return isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_responderNodeId) &&
           ((m_evidenceType == consensus::SlashingEvidenceType::DOUBLE_VOTE &&
             consensus::SlashingEvidenceVerifier::validateDoubleVoteStructure(
                 m_evidence
             ).accepted()) ||
            (m_evidenceType == consensus::SlashingEvidenceType::EQUIVOCATION &&
             consensus::SlashingEvidenceVerifier::validateProposerEquivocationStructure(
                 m_proposerEquivocationEvidence
             ).accepted())) &&
           m_respondedAt > 0;
}

std::string SlashingEvidenceResponse::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceResponse{"
           << "networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";responderNodeId=" << m_responderNodeId
           << ";respondedAt=" << m_respondedAt
           << ";evidenceType=" << consensus::slashingEvidenceTypeToString(m_evidenceType)
           << ";evidence="
           << (m_evidenceType == consensus::SlashingEvidenceType::EQUIVOCATION
                   ? m_proposerEquivocationEvidence.serialize()
                   : m_evidence.serialize())
           << "}";
    return output.str();
}

SlashingEvidenceResponse SlashingEvidenceResponse::deserialize(
    const std::string& serialized
) {
    const auto fields = parseObjectFields(
        serialized, "SlashingEvidenceResponse"
    );
    requireExactFields(
        fields,
        {"networkId", "chainId", "responderNodeId", "respondedAt", "evidenceType", "evidence"},
        "SlashingEvidenceResponse"
    );
    const consensus::SlashingEvidenceType evidenceType =
        consensus::slashingEvidenceTypeFromString(fields.at("evidenceType"));
    SlashingEvidenceResponse response;
    if (evidenceType == consensus::SlashingEvidenceType::DOUBLE_VOTE) {
        response = SlashingEvidenceResponse(
            fields.at("networkId"),
            fields.at("chainId"),
            fields.at("responderNodeId"),
            consensus::DoubleVoteEvidence::deserialize(fields.at("evidence")),
            parseI64Strict(fields.at("respondedAt"), "respondedAt")
        );
    } else if (evidenceType == consensus::SlashingEvidenceType::EQUIVOCATION) {
        response = SlashingEvidenceResponse(
            fields.at("networkId"),
            fields.at("chainId"),
            fields.at("responderNodeId"),
            consensus::ProposerEquivocationEvidence::deserialize(fields.at("evidence")),
            parseI64Strict(fields.at("respondedAt"), "respondedAt")
        );
    } else {
        throw std::invalid_argument("Unsupported slashing evidence response type.");
    }
    if (!response.isValid() || response.serialize() != serialized) {
        throw std::invalid_argument(
            "Serialized slashing evidence response is invalid or non-canonical."
        );
    }
    return response;
}

} // namespace nodo::node
