#include "serialization/CanonicalHash.hpp"

#include "crypto/hash.h"
#include "serialization/CanonicalWriter.hpp"

namespace nodo::serialization {

std::string CanonicalHash::hashBytes(
    const std::vector<unsigned char>& bytes,
    const std::string& domain
) {
    CanonicalWriter writer;
    writer.writeString(domain);
    writer.writeBytes(bytes);

    char output[NODO_HASH_BUFFER_SIZE] = {0};
    const std::vector<unsigned char>& canonical = writer.bytes();

    nodo_hash_bytes(
        canonical.data(),
        static_cast<unsigned long long>(canonical.size()),
        output,
        sizeof(output)
    );

    return std::string(output);
}

std::string CanonicalHash::hashString(
    const std::string& value,
    const std::string& domain
) {
    return hashBytes(
        std::vector<unsigned char>(value.begin(), value.end()),
        domain
    );
}

} // namespace nodo::serialization
