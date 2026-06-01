#include "p2p/EncryptedPeerChannel.hpp"

#include "serialization/CanonicalHash.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "serialization/ProtocolMessageCodec.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::p2p {

namespace {

constexpr const char* FRAME_CODEC_VERSION = "NODO_ENCRYPTED_PEER_CHANNEL_FRAME_V1";
constexpr const char* SESSION_DOMAIN = "NODO_ENCRYPTED_PEER_SESSION_ID_V1";
constexpr const char* KEY_DOMAIN = "NODO_ENCRYPTED_PEER_SESSION_KEY_V1";
constexpr const char* NONCE_DOMAIN = "NODO_ENCRYPTED_PEER_NONCE_V1";
constexpr const char* MAC_DOMAIN = "NODO_ENCRYPTED_PEER_MAC_V1";
constexpr const char* STREAM_DOMAIN = "NODO_ENCRYPTED_PEER_STREAM_V1";

bool isHex(const std::string& value) {
    if (value.empty() || (value.size() % 2) != 0) {
        return false;
    }

    for (const char character : value) {
        const bool hex =
            (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F');
        if (!hex) {
            return false;
        }
    }

    return true;
}

bool isSafeScalar(const std::string& value, std::size_t maxSize = 256) {
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

std::string hexEncode(const std::vector<unsigned char>& bytes) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const unsigned char byte : bytes) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return output.str();
}

unsigned char hexValue(char character) {
    if (character >= '0' && character <= '9') {
        return static_cast<unsigned char>(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
        return static_cast<unsigned char>(10 + character - 'a');
    }
    if (character >= 'A' && character <= 'F') {
        return static_cast<unsigned char>(10 + character - 'A');
    }
    throw std::invalid_argument("Invalid hex character.");
}

std::vector<unsigned char> hexDecode(const std::string& hex) {
    if (!isHex(hex)) {
        throw std::invalid_argument("Malformed hexadecimal value.");
    }

    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);

    for (std::size_t index = 0; index < hex.size(); index += 2) {
        bytes.push_back(
            static_cast<unsigned char>(
                (hexValue(hex[index]) << 4) | hexValue(hex[index + 1])
            )
        );
    }

    return bytes;
}

std::string hashText(
    const std::string& value,
    const std::string& domain
) {
    return serialization::CanonicalHash::hashString(value, domain);
}

std::vector<unsigned char> deriveStream(
    const std::string& keyMaterial,
    const std::string& nonceHex,
    std::uint64_t sequence,
    std::size_t byteCount
) {
    std::vector<unsigned char> stream;
    stream.reserve(byteCount);

    std::uint64_t counter = 0;
    while (stream.size() < byteCount) {
        const std::string blockHex = hashText(
            keyMaterial + "|" + nonceHex + "|" +
                std::to_string(sequence) + "|" + std::to_string(counter),
            STREAM_DOMAIN
        );
        const std::vector<unsigned char> block = hexDecode(blockHex);
        for (const unsigned char byte : block) {
            if (stream.size() == byteCount) {
                break;
            }
            stream.push_back(byte);
        }
        ++counter;
    }

    return stream;
}

std::vector<unsigned char> xorWithStream(
    const std::vector<unsigned char>& input,
    const std::string& keyMaterial,
    const std::string& nonceHex,
    std::uint64_t sequence
) {
    const std::vector<unsigned char> stream =
        deriveStream(keyMaterial, nonceHex, sequence, input.size());

    std::vector<unsigned char> output;
    output.reserve(input.size());

    for (std::size_t index = 0; index < input.size(); ++index) {
        output.push_back(static_cast<unsigned char>(input[index] ^ stream[index]));
    }

    return output;
}

std::string computeTag(
    const std::string& keyMaterial,
    const EncryptedPeerChannelFrame& frameWithoutTrustedTag
) {
    return hashText(
        keyMaterial + "|" + frameWithoutTrustedTag.aadPayload() + "|" +
            frameWithoutTrustedTag.ciphertextHex(),
        MAC_DOMAIN
    );
}

} // namespace

EncryptedPeerChannelFrame::EncryptedPeerChannelFrame()
    : m_sessionId(),
      m_fromNodeId(),
      m_toNodeId(),
      m_sequence(0),
      m_createdAt(0),
      m_nonceHex(),
      m_ciphertextHex(),
      m_authenticationTagHex() {}

EncryptedPeerChannelFrame::EncryptedPeerChannelFrame(
    std::string sessionId,
    std::string fromNodeId,
    std::string toNodeId,
    std::uint64_t sequence,
    std::int64_t createdAt,
    std::string nonceHex,
    std::string ciphertextHex,
    std::string authenticationTagHex
) : m_sessionId(std::move(sessionId)),
    m_fromNodeId(std::move(fromNodeId)),
    m_toNodeId(std::move(toNodeId)),
    m_sequence(sequence),
    m_createdAt(createdAt),
    m_nonceHex(std::move(nonceHex)),
    m_ciphertextHex(std::move(ciphertextHex)),
    m_authenticationTagHex(std::move(authenticationTagHex)) {}

const std::string& EncryptedPeerChannelFrame::sessionId() const { return m_sessionId; }
const std::string& EncryptedPeerChannelFrame::fromNodeId() const { return m_fromNodeId; }
const std::string& EncryptedPeerChannelFrame::toNodeId() const { return m_toNodeId; }
std::uint64_t EncryptedPeerChannelFrame::sequence() const { return m_sequence; }
std::int64_t EncryptedPeerChannelFrame::createdAt() const { return m_createdAt; }
const std::string& EncryptedPeerChannelFrame::nonceHex() const { return m_nonceHex; }
const std::string& EncryptedPeerChannelFrame::ciphertextHex() const { return m_ciphertextHex; }
const std::string& EncryptedPeerChannelFrame::authenticationTagHex() const { return m_authenticationTagHex; }

bool EncryptedPeerChannelFrame::isValid() const {
    return isHex(m_sessionId) &&
           isSafeScalar(m_fromNodeId) &&
           isSafeScalar(m_toNodeId) &&
           m_fromNodeId != m_toNodeId &&
           m_sequence > 0 &&
           m_createdAt > 0 &&
           isHex(m_nonceHex) &&
           isHex(m_ciphertextHex) &&
           isHex(m_authenticationTagHex);
}

std::string EncryptedPeerChannelFrame::aadPayload() const {
    std::ostringstream output;
    output << "EncryptedPeerChannelAAD{"
           << "sessionId=" << m_sessionId
           << ";fromNodeId=" << m_fromNodeId
           << ";toNodeId=" << m_toNodeId
           << ";sequence=" << m_sequence
           << ";createdAt=" << m_createdAt
           << ";nonceHex=" << m_nonceHex
           << "}";
    return output.str();
}

std::string EncryptedPeerChannelFrame::serialize() const {
    std::ostringstream output;
    output << "EncryptedPeerChannelFrame{"
           << "sessionId=" << m_sessionId
           << ";fromNodeId=" << m_fromNodeId
           << ";toNodeId=" << m_toNodeId
           << ";sequence=" << m_sequence
           << ";createdAt=" << m_createdAt
           << ";nonceHex=" << m_nonceHex
           << ";ciphertextHex=" << m_ciphertextHex
           << ";authenticationTagHex=" << m_authenticationTagHex
           << "}";
    return output.str();
}

std::string encryptedPeerChannelStatusToString(
    EncryptedPeerChannelStatus status
) {
    switch (status) {
        case EncryptedPeerChannelStatus::OPENED: return "OPENED";
        case EncryptedPeerChannelStatus::REJECTED: return "REJECTED";
        case EncryptedPeerChannelStatus::INVALID_FRAME: return "INVALID_FRAME";
        case EncryptedPeerChannelStatus::WRONG_SESSION: return "WRONG_SESSION";
        case EncryptedPeerChannelStatus::AUTHENTICATION_FAILED: return "AUTHENTICATION_FAILED";
        case EncryptedPeerChannelStatus::REPLAY_DETECTED: return "REPLAY_DETECTED";
        default: return "REJECTED";
    }
}

EncryptedPeerOpenResult::EncryptedPeerOpenResult()
    : m_status(EncryptedPeerChannelStatus::REJECTED),
      m_reason("Uninitialized encrypted peer open result."),
      m_envelope(std::nullopt) {}

EncryptedPeerOpenResult::EncryptedPeerOpenResult(
    EncryptedPeerChannelStatus status,
    std::string reason,
    std::optional<NetworkEnvelope> envelope
) : m_status(status),
    m_reason(std::move(reason)),
    m_envelope(std::move(envelope)) {}

EncryptedPeerChannelStatus EncryptedPeerOpenResult::status() const { return m_status; }
const std::string& EncryptedPeerOpenResult::reason() const { return m_reason; }
bool EncryptedPeerOpenResult::opened() const { return m_status == EncryptedPeerChannelStatus::OPENED && m_envelope.has_value(); }
const std::optional<NetworkEnvelope>& EncryptedPeerOpenResult::envelope() const { return m_envelope; }

std::string EncryptedPeerOpenResult::serialize() const {
    std::ostringstream output;
    output << "EncryptedPeerOpenResult{status="
           << encryptedPeerChannelStatusToString(m_status)
           << ";reason=" << m_reason
           << ";hasEnvelope=" << (m_envelope.has_value() ? "true" : "false")
           << "}";
    return output.str();
}

EncryptedPeerSession::EncryptedPeerSession()
    : m_localNodeId(),
      m_remoteNodeId(),
      m_sharedSecret(),
      m_sessionId(),
      m_nextOutboundSequence(1),
      m_lastInboundSequence(0),
      m_establishedAt(0) {}

EncryptedPeerSession::EncryptedPeerSession(
    std::string localNodeId,
    std::string remoteNodeId,
    std::string sharedSecret,
    std::int64_t establishedAt
) : m_localNodeId(std::move(localNodeId)),
    m_remoteNodeId(std::move(remoteNodeId)),
    m_sharedSecret(std::move(sharedSecret)),
    m_sessionId(deriveSessionId(m_localNodeId, m_remoteNodeId, m_sharedSecret)),
    m_nextOutboundSequence(1),
    m_lastInboundSequence(0),
    m_establishedAt(establishedAt) {}

const std::string& EncryptedPeerSession::localNodeId() const { return m_localNodeId; }
const std::string& EncryptedPeerSession::remoteNodeId() const { return m_remoteNodeId; }
const std::string& EncryptedPeerSession::sessionId() const { return m_sessionId; }
std::uint64_t EncryptedPeerSession::nextOutboundSequence() const { return m_nextOutboundSequence; }
std::uint64_t EncryptedPeerSession::lastInboundSequence() const { return m_lastInboundSequence; }
std::int64_t EncryptedPeerSession::establishedAt() const { return m_establishedAt; }

bool EncryptedPeerSession::isValid() const {
    return isSafeScalar(m_localNodeId) &&
           isSafeScalar(m_remoteNodeId) &&
           m_localNodeId != m_remoteNodeId &&
           !m_sharedSecret.empty() &&
           isHex(m_sessionId) &&
           m_establishedAt > 0;
}

EncryptedPeerChannelFrame EncryptedPeerSession::sealEnvelope(
    const NetworkEnvelope& envelope,
    std::int64_t now
) {
    if (!isValid() || envelope.senderNodeId() != m_localNodeId || now <= 0) {
        return EncryptedPeerChannelFrame();
    }

    const std::uint64_t sequence = m_nextOutboundSequence++;
    const std::string nonceHex = hashText(
        m_sessionId + "|" + m_localNodeId + "|" + m_remoteNodeId + "|" +
            std::to_string(sequence) + "|" + std::to_string(now),
        NONCE_DOMAIN
    );

    const std::vector<unsigned char> plaintext =
        serialization::ProtocolMessageCodec::encodeNetworkEnvelope(envelope);
    const std::vector<unsigned char> ciphertext =
        xorWithStream(plaintext, deriveKeyMaterial(), nonceHex, sequence);

    EncryptedPeerChannelFrame frameWithoutTag(
        m_sessionId,
        m_localNodeId,
        m_remoteNodeId,
        sequence,
        now,
        nonceHex,
        hexEncode(ciphertext),
        "00"
    );

    const std::string tag = computeTag(deriveKeyMaterial(), frameWithoutTag);

    return EncryptedPeerChannelFrame(
        frameWithoutTag.sessionId(),
        frameWithoutTag.fromNodeId(),
        frameWithoutTag.toNodeId(),
        frameWithoutTag.sequence(),
        frameWithoutTag.createdAt(),
        frameWithoutTag.nonceHex(),
        frameWithoutTag.ciphertextHex(),
        tag
    );
}

EncryptedPeerOpenResult EncryptedPeerSession::openFrame(
    const EncryptedPeerChannelFrame& frame
) {
    if (!isValid()) {
        return EncryptedPeerOpenResult(
            EncryptedPeerChannelStatus::REJECTED,
            "Encrypted peer session is invalid.",
            std::nullopt
        );
    }

    if (!frame.isValid()) {
        return EncryptedPeerOpenResult(
            EncryptedPeerChannelStatus::INVALID_FRAME,
            "Encrypted peer frame is invalid.",
            std::nullopt
        );
    }

    if (frame.sessionId() != m_sessionId ||
        frame.fromNodeId() != m_remoteNodeId ||
        frame.toNodeId() != m_localNodeId) {
        return EncryptedPeerOpenResult(
            EncryptedPeerChannelStatus::WRONG_SESSION,
            "Encrypted peer frame does not belong to this session direction.",
            std::nullopt
        );
    }

    if (frame.sequence() <= m_lastInboundSequence) {
        return EncryptedPeerOpenResult(
            EncryptedPeerChannelStatus::REPLAY_DETECTED,
            "Encrypted peer frame sequence was already processed.",
            std::nullopt
        );
    }

    const EncryptedPeerChannelFrame frameWithoutTag(
        frame.sessionId(),
        frame.fromNodeId(),
        frame.toNodeId(),
        frame.sequence(),
        frame.createdAt(),
        frame.nonceHex(),
        frame.ciphertextHex(),
        "00"
    );

    const std::string expectedTag = computeTag(deriveKeyMaterial(), frameWithoutTag);
    if (expectedTag != frame.authenticationTagHex()) {
        return EncryptedPeerOpenResult(
            EncryptedPeerChannelStatus::AUTHENTICATION_FAILED,
            "Encrypted peer frame authentication tag is invalid.",
            std::nullopt
        );
    }

    try {
        const std::vector<unsigned char> ciphertext = hexDecode(frame.ciphertextHex());
        const std::vector<unsigned char> plaintext =
            xorWithStream(ciphertext, deriveKeyMaterial(), frame.nonceHex(), frame.sequence());
        NetworkEnvelope envelope =
            serialization::ProtocolMessageCodec::decodeNetworkEnvelope(plaintext);

        if (envelope.senderNodeId() != m_remoteNodeId) {
            return EncryptedPeerOpenResult(
                EncryptedPeerChannelStatus::AUTHENTICATION_FAILED,
                "Decrypted envelope sender does not match secure peer session.",
                std::nullopt
            );
        }

        m_lastInboundSequence = frame.sequence();

        return EncryptedPeerOpenResult(
            EncryptedPeerChannelStatus::OPENED,
            "Encrypted peer frame opened successfully.",
            envelope
        );
    } catch (const std::exception& error) {
        return EncryptedPeerOpenResult(
            EncryptedPeerChannelStatus::AUTHENTICATION_FAILED,
            std::string("Encrypted peer frame could not be decoded: ") + error.what(),
            std::nullopt
        );
    }
}

std::string EncryptedPeerSession::serialize() const {
    std::ostringstream output;
    output << "EncryptedPeerSession{"
           << "localNodeId=" << m_localNodeId
           << ";remoteNodeId=" << m_remoteNodeId
           << ";sessionId=" << m_sessionId
           << ";nextOutboundSequence=" << m_nextOutboundSequence
           << ";lastInboundSequence=" << m_lastInboundSequence
           << ";establishedAt=" << m_establishedAt
           << "}";
    return output.str();
}

std::string EncryptedPeerSession::deriveSessionId(
    const std::string& leftNodeId,
    const std::string& rightNodeId,
    const std::string& sharedSecret
) {
    std::string first = leftNodeId;
    std::string second = rightNodeId;
    if (second < first) {
        std::swap(first, second);
    }
    return hashText(first + "|" + second + "|" + sharedSecret, SESSION_DOMAIN);
}

std::string EncryptedPeerSession::deriveKeyMaterial() const {
    return hashText(m_sessionId + "|" + m_sharedSecret, KEY_DOMAIN);
}

std::vector<unsigned char> EncryptedPeerChannelCodec::encodeFrame(
    const EncryptedPeerChannelFrame& frame
) {
    serialization::CanonicalWriter writer;
    writer.writeString(FRAME_CODEC_VERSION);
    writer.writeString(frame.sessionId());
    writer.writeString(frame.fromNodeId());
    writer.writeString(frame.toNodeId());
    writer.writeUInt64(frame.sequence());
    writer.writeInt64(frame.createdAt());
    writer.writeString(frame.nonceHex());
    writer.writeString(frame.ciphertextHex());
    writer.writeString(frame.authenticationTagHex());
    return writer.bytes();
}

EncryptedPeerChannelFrame EncryptedPeerChannelCodec::decodeFrame(
    const std::vector<unsigned char>& bytes
) {
    serialization::CanonicalReader reader(bytes, MAX_ENCRYPTED_FRAME_BYTES);
    const std::string version = reader.readString();
    if (version != FRAME_CODEC_VERSION) {
        throw std::runtime_error("Unsupported encrypted peer channel frame version.");
    }

    const std::string sessionId = reader.readString();
    const std::string fromNodeId = reader.readString();
    const std::string toNodeId = reader.readString();
    const std::uint64_t sequence = reader.readUInt64();
    const std::int64_t createdAt = reader.readInt64();
    const std::string nonceHex = reader.readString();
    const std::string ciphertextHex = reader.readString();
    const std::string authenticationTagHex = reader.readString();

    EncryptedPeerChannelFrame frame(
        sessionId,
        fromNodeId,
        toNodeId,
        sequence,
        createdAt,
        nonceHex,
        ciphertextHex,
        authenticationTagHex
    );

    reader.requireFullyConsumed();
    return frame;
}

std::string EncryptedPeerChannelCodec::encodeFrameToString(
    const EncryptedPeerChannelFrame& frame
) {
    return hexEncode(encodeFrame(frame));
}

EncryptedPeerChannelFrame EncryptedPeerChannelCodec::decodeFrameFromString(
    const std::string& bytes
) {
    return decodeFrame(hexDecode(bytes));
}

bool EncryptedPeerChannelCodec::isValidFrameBytes(
    const std::vector<unsigned char>& bytes
) {
    try {
        return decodeFrame(bytes).isValid();
    } catch (...) {
        return false;
    }
}

} // namespace nodo::p2p
