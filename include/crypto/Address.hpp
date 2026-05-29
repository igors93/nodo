#ifndef NODO_CRYPTO_ADDRESS_HPP
#define NODO_CRYPTO_ADDRESS_HPP

#include <string>

namespace nodo::crypto {

/*
 * Address represents a deterministic Nodo address string.
 *
 * Current development format:
 *   nodo1 + 40 lowercase hexadecimal payload chars + 8 checksum chars
 *
 * This is not the final production wallet format. It is a deterministic
 * foundation that can later be migrated to Bech32/Base32 or another audited
 * address format.
 */
class Address {
public:
    Address();

    explicit Address(
        std::string value
    );

    static Address fromString(
        const std::string& value
    );

    static std::string networkPrefix();

    static std::size_t payloadHexSize();
    static std::size_t checksumHexSize();
    static std::size_t totalSize();

    const std::string& value() const;

    bool isValid() const;

    std::string serialize() const;

private:
    static bool hasOnlyLowercaseHexAfterPrefix(
        const std::string& value
    );

    static bool hasValidChecksum(
        const std::string& value
    );

    static std::string computeChecksum(
        const std::string& body
    );

    std::string m_value;
};

} // namespace nodo::crypto

#endif
