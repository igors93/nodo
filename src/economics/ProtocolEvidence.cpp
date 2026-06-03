#include "economics/ProtocolEvidence.hpp"

#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

std::string protocolEvidenceTypeToString(ProtocolEvidenceType type) {
    switch (type) {
        case ProtocolEvidenceType::P2P_INVALID_MESSAGE:     return "P2P_INVALID_MESSAGE";
        case ProtocolEvidenceType::P2P_RATE_LIMIT_EXCEEDED: return "P2P_RATE_LIMIT_EXCEEDED";
        case ProtocolEvidenceType::P2P_PEER_QUARANTINED:    return "P2P_PEER_QUARANTINED";
        case ProtocolEvidenceType::DATA_AVAILABILITY_FAILURE: return "DATA_AVAILABILITY_FAILURE";
        case ProtocolEvidenceType::DOUBLE_SIGN:             return "DOUBLE_SIGN";
        case ProtocolEvidenceType::INVALID_BLOCK_VOTE:      return "INVALID_BLOCK_VOTE";
        default:                                             return "UNKNOWN";
    }
}

bool protocolEvidenceTypeFromString(const std::string& s, ProtocolEvidenceType& out) {
    if (s == "P2P_INVALID_MESSAGE")       { out = ProtocolEvidenceType::P2P_INVALID_MESSAGE;     return true; }
    if (s == "P2P_RATE_LIMIT_EXCEEDED")   { out = ProtocolEvidenceType::P2P_RATE_LIMIT_EXCEEDED; return true; }
    if (s == "P2P_PEER_QUARANTINED")      { out = ProtocolEvidenceType::P2P_PEER_QUARANTINED;    return true; }
    if (s == "DATA_AVAILABILITY_FAILURE") { out = ProtocolEvidenceType::DATA_AVAILABILITY_FAILURE; return true; }
    if (s == "DOUBLE_SIGN")               { out = ProtocolEvidenceType::DOUBLE_SIGN;             return true; }
    if (s == "INVALID_BLOCK_VOTE")        { out = ProtocolEvidenceType::INVALID_BLOCK_VOTE;      return true; }
    return false;
}

ProtocolEvidence::ProtocolEvidence()
    : m_evidenceType(ProtocolEvidenceType::P2P_INVALID_MESSAGE),
      m_blockHeight(0),
      m_epoch(0),
      m_createdAt(0),
      m_valid(false),
      m_rejectionReason("ProtocolEvidence: default-constructed.") {}

ProtocolEvidence::ProtocolEvidence(
    std::string evidenceId,
    ProtocolEvidenceType evidenceType,
    std::string subjectId,
    std::string sourceId,
    std::uint64_t blockHeight,
    std::uint64_t epoch,
    std::string ruleId,
    std::string payloadDigest,
    std::string reason,
    std::int64_t createdAt
)
    : m_evidenceId(std::move(evidenceId)),
      m_evidenceType(evidenceType),
      m_subjectId(std::move(subjectId)),
      m_sourceId(std::move(sourceId)),
      m_blockHeight(blockHeight),
      m_epoch(epoch),
      m_ruleId(std::move(ruleId)),
      m_payloadDigest(std::move(payloadDigest)),
      m_reason(std::move(reason)),
      m_createdAt(createdAt),
      m_valid(false),
      m_rejectionReason("")
{
    constexpr std::size_t kMaxIdLen     = 128;
    constexpr std::size_t kMaxDigestLen = 128;
    constexpr std::size_t kMaxReasonLen = 512;

    if (m_evidenceId.empty()) {
        m_rejectionReason = "ProtocolEvidence: evidenceId must not be empty.";
        return;
    }
    if (m_evidenceId.size() > kMaxIdLen) {
        m_rejectionReason = "ProtocolEvidence: evidenceId exceeds maximum length.";
        return;
    }
    if (m_subjectId.empty()) {
        m_rejectionReason = "ProtocolEvidence: subjectId must not be empty.";
        return;
    }
    if (m_subjectId.size() > kMaxIdLen) {
        m_rejectionReason = "ProtocolEvidence: subjectId exceeds maximum length.";
        return;
    }
    if (m_sourceId.empty()) {
        m_rejectionReason = "ProtocolEvidence: sourceId must not be empty.";
        return;
    }
    if (m_sourceId.size() > kMaxIdLen) {
        m_rejectionReason = "ProtocolEvidence: sourceId exceeds maximum length.";
        return;
    }
    if (m_ruleId.empty()) {
        m_rejectionReason = "ProtocolEvidence: ruleId must not be empty.";
        return;
    }
    if (m_ruleId.size() > kMaxIdLen) {
        m_rejectionReason = "ProtocolEvidence: ruleId exceeds maximum length.";
        return;
    }
    if (m_payloadDigest.empty()) {
        m_rejectionReason = "ProtocolEvidence: payloadDigest must not be empty.";
        return;
    }
    if (m_payloadDigest.size() > kMaxDigestLen) {
        m_rejectionReason = "ProtocolEvidence: payloadDigest exceeds maximum length.";
        return;
    }
    if (m_reason.empty()) {
        m_rejectionReason = "ProtocolEvidence: reason must not be empty.";
        return;
    }
    if (m_reason.size() > kMaxReasonLen) {
        m_rejectionReason = "ProtocolEvidence: reason exceeds maximum length.";
        return;
    }
    m_valid = true;
}

const std::string& ProtocolEvidence::evidenceId() const { return m_evidenceId; }
ProtocolEvidenceType ProtocolEvidence::evidenceType() const { return m_evidenceType; }
const std::string& ProtocolEvidence::subjectId() const { return m_subjectId; }
const std::string& ProtocolEvidence::sourceId() const { return m_sourceId; }
std::uint64_t ProtocolEvidence::blockHeight() const { return m_blockHeight; }
std::uint64_t ProtocolEvidence::epoch() const { return m_epoch; }
const std::string& ProtocolEvidence::ruleId() const { return m_ruleId; }
const std::string& ProtocolEvidence::payloadDigest() const { return m_payloadDigest; }
const std::string& ProtocolEvidence::reason() const { return m_reason; }
std::int64_t ProtocolEvidence::createdAt() const { return m_createdAt; }
bool ProtocolEvidence::isValid() const { return m_valid; }
const std::string& ProtocolEvidence::rejectionReason() const { return m_rejectionReason; }

std::string ProtocolEvidence::serialize() const {
    std::ostringstream oss;
    oss << "ProtocolEvidence{"
        << "evidenceId=" << m_evidenceId
        << ";evidenceType=" << protocolEvidenceTypeToString(m_evidenceType)
        << ";subjectId=" << m_subjectId
        << ";sourceId=" << m_sourceId
        << ";blockHeight=" << m_blockHeight
        << ";epoch=" << m_epoch
        << ";ruleId=" << m_ruleId
        << ";payloadDigest=" << m_payloadDigest
        << ";reason=" << m_reason
        << ";createdAt=" << m_createdAt
        << "}";
    return oss.str();
}

namespace {

// Known field names in canonical serialization order.
const std::string kKnownFields[] = {
    "evidenceId", "evidenceType", "subjectId", "sourceId",
    "blockHeight", "epoch", "ruleId", "payloadDigest", "reason", "createdAt"
};

std::map<std::string, std::string> parseProtocolEvidenceFields(
    const std::string& serialized
) {
    const std::string prefix = "ProtocolEvidence{";
    if (serialized.rfind(prefix, 0) != 0 ||
        serialized.size() <= prefix.size() ||
        serialized.back() != '}') {
        throw std::invalid_argument(
            "ProtocolEvidence::deserialize: malformed input."
        );
    }
    const std::string inner =
        serialized.substr(prefix.size(), serialized.size() - prefix.size() - 1);

    std::map<std::string, std::string> fields;
    std::size_t pos = 0;
    while (pos < inner.size()) {
        const std::size_t eq = inner.find('=', pos);
        if (eq == std::string::npos) {
            throw std::invalid_argument(
                "ProtocolEvidence::deserialize: missing '=' in field."
            );
        }
        const std::string key = inner.substr(pos, eq - pos);

        // Reject unknown fields.
        bool known = false;
        for (const auto& k : kKnownFields) {
            if (k == key) { known = true; break; }
        }
        if (!known) {
            throw std::invalid_argument(
                "ProtocolEvidence::deserialize: unknown field '" + key + "'."
            );
        }

        // Reject duplicate fields.
        if (fields.count(key) > 0) {
            throw std::invalid_argument(
                "ProtocolEvidence::deserialize: duplicate field '" + key + "'."
            );
        }

        const std::size_t semi = inner.find(';', eq + 1);
        const std::string val = (semi == std::string::npos)
            ? inner.substr(eq + 1)
            : inner.substr(eq + 1, semi - eq - 1);
        fields[key] = val;
        pos = (semi == std::string::npos) ? inner.size() : semi + 1;
    }
    return fields;
}

} // namespace

ProtocolEvidence ProtocolEvidence::deserialize(const std::string& serialized) {
    const auto fields = parseProtocolEvidenceFields(serialized);

    auto get = [&](const std::string& key) -> const std::string& {
        const auto it = fields.find(key);
        if (it == fields.end()) {
            throw std::invalid_argument(
                "ProtocolEvidence::deserialize: missing field '" + key + "'."
            );
        }
        return it->second;
    };

    ProtocolEvidenceType evidenceType = ProtocolEvidenceType::P2P_INVALID_MESSAGE;
    if (!protocolEvidenceTypeFromString(get("evidenceType"), evidenceType)) {
        throw std::invalid_argument(
            "ProtocolEvidence::deserialize: unknown evidenceType."
        );
    }

    std::uint64_t blockHeight = 0;
    std::uint64_t epoch = 0;
    std::int64_t createdAt = 0;
    try {
        blockHeight = std::stoull(get("blockHeight"));
        epoch       = std::stoull(get("epoch"));
        createdAt   = std::stoll(get("createdAt"));
    } catch (const std::exception& e) {
        throw std::invalid_argument(
            std::string("ProtocolEvidence::deserialize: invalid numeric field: ") + e.what()
        );
    }

    return ProtocolEvidence(
        get("evidenceId"),
        evidenceType,
        get("subjectId"),
        get("sourceId"),
        blockHeight,
        epoch,
        get("ruleId"),
        get("payloadDigest"),
        get("reason"),
        createdAt
    );
}

} // namespace nodo::economics
