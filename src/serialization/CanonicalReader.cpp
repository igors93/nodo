#include "serialization/CanonicalReader.hpp"

#include <stdexcept>
#include <utility>

namespace nodo::serialization {

CanonicalReader::CanonicalReader(
    std::vector<unsigned char> bytes,
    std::size_t maxFieldBytes
) : m_bytes(std::move(bytes)),
    m_position(0),
    m_maxFieldBytes(maxFieldBytes) {
    if (m_maxFieldBytes == 0) {
        throw std::invalid_argument("CanonicalReader maxFieldBytes must be positive.");
    }
}

CanonicalReader::CanonicalReader(
    const std::string& bytes,
    std::size_t maxFieldBytes
) : CanonicalReader(
        std::vector<unsigned char>(bytes.begin(), bytes.end()),
        maxFieldBytes
    ) {}

std::uint8_t CanonicalReader::readUInt8() {
    requireAvailable(1);
    return static_cast<std::uint8_t>(m_bytes[m_position++]);
}

bool CanonicalReader::readBool() {
    const std::uint8_t value = readUInt8();

    if (value == 0) {
        return false;
    }

    if (value == 1) {
        return true;
    }

    throw std::runtime_error("Canonical boolean field must be 0 or 1.");
}

std::uint32_t CanonicalReader::readUInt32() {
    requireAvailable(4);

    std::uint32_t value = 0;

    for (int index = 0; index < 4; ++index) {
        value = static_cast<std::uint32_t>((value << 8) | m_bytes[m_position++]);
    }

    return value;
}

std::uint64_t CanonicalReader::readUInt64() {
    requireAvailable(8);

    std::uint64_t value = 0;

    for (int index = 0; index < 8; ++index) {
        value = static_cast<std::uint64_t>((value << 8) | m_bytes[m_position++]);
    }

    return value;
}

std::int64_t CanonicalReader::readInt64() {
    return static_cast<std::int64_t>(readUInt64());
}

std::string CanonicalReader::readString() {
    const std::uint32_t length = readUInt32();
    requireAllowedFieldSize(length);
    requireAvailable(length);

    std::string value(
        m_bytes.begin() + static_cast<std::ptrdiff_t>(m_position),
        m_bytes.begin() + static_cast<std::ptrdiff_t>(m_position + length)
    );

    m_position += length;
    return value;
}

std::vector<unsigned char> CanonicalReader::readBytes() {
    const std::uint32_t length = readUInt32();
    requireAllowedFieldSize(length);
    requireAvailable(length);

    std::vector<unsigned char> value(
        m_bytes.begin() + static_cast<std::ptrdiff_t>(m_position),
        m_bytes.begin() + static_cast<std::ptrdiff_t>(m_position + length)
    );

    m_position += length;
    return value;
}

std::size_t CanonicalReader::position() const {
    return m_position;
}

std::size_t CanonicalReader::remaining() const {
    return m_bytes.size() - m_position;
}

bool CanonicalReader::fullyConsumed() const {
    return m_position == m_bytes.size();
}

void CanonicalReader::requireFullyConsumed() const {
    if (!fullyConsumed()) {
        throw std::runtime_error("Canonical payload contains trailing bytes.");
    }
}

void CanonicalReader::requireAvailable(
    std::size_t count
) const {
    if (count > remaining()) {
        throw std::runtime_error("Canonical payload is truncated.");
    }
}

void CanonicalReader::requireAllowedFieldSize(
    std::size_t count
) const {
    if (count > m_maxFieldBytes) {
        throw std::runtime_error("Canonical field exceeds configured size limit.");
    }
}

} // namespace nodo::serialization
