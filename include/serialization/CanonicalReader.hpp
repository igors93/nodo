#ifndef NODO_SERIALIZATION_CANONICAL_READER_HPP
#define NODO_SERIALIZATION_CANONICAL_READER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * CanonicalReader is intentionally strict. Any truncated field, oversized field
 * or boolean outside {0,1} throws. Call requireFullyConsumed() at codec
 * boundaries so trailing bytes cannot be ignored.
 */
class CanonicalReader {
public:
    explicit CanonicalReader(
        std::vector<unsigned char> bytes,
        std::size_t maxFieldBytes = 16 * 1024 * 1024
    );

    explicit CanonicalReader(
        const std::string& bytes,
        std::size_t maxFieldBytes = 16 * 1024 * 1024
    );

    std::uint8_t readUInt8();
    bool readBool();
    std::uint32_t readUInt32();
    std::uint64_t readUInt64();
    std::int64_t readInt64();
    std::string readString();
    std::vector<unsigned char> readBytes();

    std::size_t position() const;
    std::size_t remaining() const;
    bool fullyConsumed() const;
    void requireFullyConsumed() const;

private:
    std::vector<unsigned char> m_bytes;
    std::size_t m_position;
    std::size_t m_maxFieldBytes;

    void requireAvailable(std::size_t count) const;
    void requireAllowedFieldSize(std::size_t count) const;
};

} // namespace nodo::serialization

#endif
