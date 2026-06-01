#include "p2p/Transport.hpp"

#include <sstream>
#include <utility>

namespace nodo::p2p {

namespace {

bool isSafeNodeId(const std::string& value) {
    if (value.empty() || value.size() > 160) {
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

} // namespace

std::string transportStatusToString(TransportStatus status) {
    switch (status) {
        case TransportStatus::SENT:
            return "SENT";
        case TransportStatus::REJECTED:
            return "REJECTED";
        case TransportStatus::NOT_CONNECTED:
            return "NOT_CONNECTED";
        case TransportStatus::INVALID_MESSAGE:
            return "INVALID_MESSAGE";
        default:
            return "REJECTED";
    }
}

TransportResult::TransportResult()
    : m_status(TransportStatus::REJECTED),
      m_reason("Uninitialized transport result.") {}

TransportResult::TransportResult(TransportStatus status, std::string reason)
    : m_status(status),
      m_reason(std::move(reason)) {}

TransportStatus TransportResult::status() const {
    return m_status;
}

const std::string& TransportResult::reason() const {
    return m_reason;
}

bool TransportResult::sent() const {
    return m_status == TransportStatus::SENT;
}

bool TransportResult::success() const {
    return sent();
}

std::string TransportResult::serialize() const {
    std::ostringstream output;
    output << "TransportResult{status=" << transportStatusToString(m_status)
           << ";reason=" << m_reason
           << "}";
    return output.str();
}

TransportMessage::TransportMessage()
    : m_fromNodeId(""),
      m_toNodeId(""),
      m_envelope(),
      m_sentAt(0) {}

TransportMessage::TransportMessage(
    std::string fromNodeId,
    std::string toNodeId,
    NetworkEnvelope envelope,
    std::int64_t sentAt
) : m_fromNodeId(std::move(fromNodeId)),
    m_toNodeId(std::move(toNodeId)),
    m_envelope(std::move(envelope)),
    m_sentAt(sentAt) {}

const std::string& TransportMessage::fromNodeId() const {
    return m_fromNodeId;
}

const std::string& TransportMessage::toNodeId() const {
    return m_toNodeId;
}

const NetworkEnvelope& TransportMessage::envelope() const {
    return m_envelope;
}

std::int64_t TransportMessage::sentAt() const {
    return m_sentAt;
}

bool TransportMessage::isValid() const {
    return isSafeNodeId(m_fromNodeId) &&
           isSafeNodeId(m_toNodeId) &&
           m_fromNodeId != m_toNodeId &&
           m_sentAt > 0 &&
           m_envelope.senderNodeId() == m_fromNodeId &&
           m_envelope.isStructurallyValid(1024 * 1024);
}

std::string TransportMessage::serialize() const {
    std::ostringstream output;
    output << "TransportMessage{"
           << "fromNodeId=" << m_fromNodeId
           << ";toNodeId=" << m_toNodeId
           << ";sentAt=" << m_sentAt
           << ";envelope=" << m_envelope.serialize()
           << "}";
    return output.str();
}

} // namespace nodo::p2p
