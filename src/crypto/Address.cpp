#include "crypto/Address.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <utility>

namespace nodo::crypto {

Address::Address()
    : m_value("") {}

Address::Address(
    std::string value
)
    : m_value(std::move(value)) {}

Address Address::fromString(
    const std::string& value
) {
    return Address(value);
}

std::string Address::networkPrefix() {
    return "nodo1";
}

std::size_t Address::payloadHexSize() {
    return 40U;
}

std::size_t Address::checksumHexSize() {
    return 8U;
}

std::size_t Address::totalSize() {
    return networkPrefix().size() +
           payloadHexSize() +
           checksumHexSize();
}

const std::string& Address::value() const {
    return m_value;
}

bool Address::isValid() const {
    if (m_value.size() != totalSize()) {
        return false;
    }

    if (m_value.rfind(networkPrefix(), 0) != 0) {
        return false;
    }

    if (!hasOnlyLowercaseHexAfterPrefix(m_value)) {
        return false;
    }

    if (!hasValidChecksum(m_value)) {
        return false;
    }

    return true;
}

std::string Address::serialize() const {
    std::ostringstream oss;

    oss << "Address{"
        << "value=" << m_value
        << "}";

    return oss.str();
}

bool Address::hasOnlyLowercaseHexAfterPrefix(
    const std::string& value
) {
    if (value.rfind(networkPrefix(), 0) != 0) {
        return false;
    }

    for (std::size_t i = networkPrefix().size(); i < value.size(); ++i) {
        const char current = value[i];

        const bool isDigit = current >= '0' && current <= '9';
        const bool isLowerHex = current >= 'a' && current <= 'f';

        if (!isDigit && !isLowerHex) {
            return false;
        }
    }

    return true;
}

bool Address::hasValidChecksum(
    const std::string& value
) {
    if (value.size() != totalSize()) {
        return false;
    }

    const std::string body =
        value.substr(0, networkPrefix().size() + payloadHexSize());

    const std::string checksum =
        value.substr(networkPrefix().size() + payloadHexSize());

    return checksum == computeChecksum(body);
}

std::string Address::computeChecksum(
    const std::string& body
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};

    const std::string payload =
        "NODO_ADDRESS_CHECKSUM_V1|" + body;

    nodo_hash_string(
        payload.c_str(),
        output,
        sizeof(output)
    );

    return std::string(output).substr(0, checksumHexSize());
}

} // namespace nodo::crypto
