#ifndef NODO_SERIALIZATION_CANONICAL_HASH_HPP
#define NODO_SERIALIZATION_CANONICAL_HASH_HPP

#include <string>
#include <vector>

namespace nodo::serialization {

class CanonicalHash {
public:
    static std::string hashBytes(
        const std::vector<unsigned char>& bytes,
        const std::string& domain
    );

    static std::string hashString(
        const std::string& value,
        const std::string& domain
    );
};

} // namespace nodo::serialization

#endif
