#include "serialization/CanonicalWriter.hpp"

#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace nodo::serialization {

CanonicalWriter::CanonicalWriter()
    : m_bytes() {}

void CanonicalWriter::writeUInt8(
    std::uint8_t value
) {
    m_bytes.push_back(static_cast<unsigned char>(value));
}

void CanonicalWriter::writeBool(
    bool value
) {
    writeUInt8(value ? 1U : 0U);
}

void CanonicalWriter::writeUInt32(
    std::uint32_t value
) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        m_bytes.push_back(
            static_cast<unsigned char>((value >> shift) & 0xffU)
        );
    }
}

void CanonicalWriter::writeUInt64(
    std::uint64_t value
) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        m_bytes.push_back(
            static_cast<unsigned char>((value >> shift) & 0xffULL)
        );
    }
}

void CanonicalWriter::writeInt64(
    std::int64_t value
) {
    writeUInt64(static_cast<std::uint64_t>(value));
}

void CanonicalWriter::writeString(
    const std::string& value
) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Canonical string field is too large.");
    }

    writeUInt32(static_cast<std::uint32_t>(value.size()));
    m_bytes.insert(m_bytes.end(), value.begin(), value.end());
}

void CanonicalWriter::writeBytes(
    const std::vector<unsigned char>& value
) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Canonical bytes field is too large.");
    }

    writeUInt32(static_cast<std::uint32_t>(value.size()));
    m_bytes.insert(m_bytes.end(), value.begin(), value.end());
}

const std::vector<unsigned char>& CanonicalWriter::bytes() const {
    return m_bytes;
}

std::string CanonicalWriter::byteString() const {
    return std::string(m_bytes.begin(), m_bytes.end());
}

std::string CanonicalWriter::hex() const {
    std::ostringstream output;

    for (const unsigned char byte : m_bytes) {
        output << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(byte);
    }

    return output.str();
}

bool CanonicalWriter::empty() const {
    return m_bytes.empty();
}

std::size_t CanonicalWriter::size() const {
    return m_bytes.size();
}

} // namespace nodo::serialization
