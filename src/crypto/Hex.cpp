#include "crypto/Hex.hpp"

#include <stdexcept>

namespace nodo::crypto {

namespace {

unsigned char decodeNibble(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<unsigned char>(value - '0');
    }

    if (value >= 'a' && value <= 'f') {
        return static_cast<unsigned char>(value - 'a' + 10);
    }

    if (value >= 'A' && value <= 'F') {
        return static_cast<unsigned char>(value - 'A' + 10);
    }

    throw std::invalid_argument("Non-hex character rejected.");
}

} // namespace

std::string hexEncode(
    const unsigned char* data,
    std::size_t size
) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string output;
    output.reserve(size * 2);

    for (std::size_t index = 0; index < size; ++index) {
        const unsigned char value = data[index];
        output.push_back(kHex[value >> 4U]);
        output.push_back(kHex[value & 0x0fU]);
    }

    return output;
}

std::string hexEncode(
    const std::vector<unsigned char>& data
) {
    return hexEncode(data.data(), data.size());
}

std::vector<unsigned char> hexDecode(
    const std::string& hex
) {
    if (hex.empty() || hex.size() % 2U != 0U) {
        throw std::invalid_argument("Hex string has invalid length.");
    }

    std::vector<unsigned char> output;
    output.reserve(hex.size() / 2U);

    for (std::size_t index = 0; index < hex.size(); index += 2U) {
        output.push_back(
            static_cast<unsigned char>(
                (decodeNibble(hex[index]) << 4U) |
                decodeNibble(hex[index + 1U])
            )
        );
    }

    return output;
}

bool isHexString(
    const std::string& hex
) {
    if (hex.empty() || hex.size() % 2U != 0U) {
        return false;
    }

    for (const char value : hex) {
        const bool digit = value >= '0' && value <= '9';
        const bool lower = value >= 'a' && value <= 'f';
        const bool upper = value >= 'A' && value <= 'F';

        if (!digit && !lower && !upper) {
            return false;
        }
    }

    return true;
}

bool hasHexByteSize(
    const std::string& hex,
    std::size_t byteSize
) {
    return hex.size() == byteSize * 2U &&
           isHexString(hex);
}

} // namespace nodo::crypto
