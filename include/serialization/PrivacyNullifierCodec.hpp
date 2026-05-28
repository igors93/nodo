#ifndef NODO_SERIALIZATION_PRIVACY_NULLIFIER_CODEC_HPP
#define NODO_SERIALIZATION_PRIVACY_NULLIFIER_CODEC_HPP

#include "privacy/PrivacyNullifier.hpp"

#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * PrivacyNullifierCodec centralizes safe reconstruction of PrivacyNullifier
 * objects from deterministic text serialization.
 *
 * Security principle:
 * Nullifiers are the private-accounting double-spend protection boundary.
 * Parsing them must happen through one audited serialization module instead
 * of scattered ad-hoc parsing helpers.
 *
 * Current status:
 * This codec supports the current deterministic text format. Future versions
 * should migrate this boundary to a stricter canonical encoding.
 */
class PrivacyNullifierCodec {
public:
    static privacy::PrivacyNullifier deserialize(
        const std::string& serialized
    );

    static std::vector<privacy::PrivacyNullifier> deserializeList(
        const std::string& serializedList
    );

private:
    static privacy::PrivacyNullifierType parsePrivacyNullifierType(
        const std::string& value
    );

    static std::int64_t parseSigned64(
        const std::string& value,
        const std::string& fieldName
    );

    static void assertSafeHexLikeField(
        const std::string& value,
        const std::string& fieldName
    );
};

} // namespace nodo::serialization

#endif