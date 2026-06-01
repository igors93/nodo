#ifndef NODO_SERIALIZATION_CANONICAL_WRITER_HPP
#define NODO_SERIALIZATION_CANONICAL_WRITER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * CanonicalWriter is the byte-level boundary for protocol data that must be
 * identical on every node. It writes fixed-endian integers and length-prefixed
 * byte/string fields. No locale, streams or text formatting are involved.
 */
class CanonicalWriter {
public:
    CanonicalWriter();

    void writeUInt8(std::uint8_t value);
    void writeBool(bool value);
    void writeUInt32(std::uint32_t value);
    void writeUInt64(std::uint64_t value);
    void writeInt64(std::int64_t value);
    void writeString(const std::string& value);
    void writeBytes(const std::vector<unsigned char>& value);

    const std::vector<unsigned char>& bytes() const;
    std::string byteString() const;
    std::string hex() const;
    bool empty() const;
    std::size_t size() const;

private:
    std::vector<unsigned char> m_bytes;
};

} // namespace nodo::serialization

#endif
