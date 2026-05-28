#ifndef NODO_SERIALIZATION_PRIVACY_COMMITMENT_CODEC_HPP
#define NODO_SERIALIZATION_PRIVACY_COMMITMENT_CODEC_HPP

#include "privacy/PrivacyCommitment.hpp"

#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * PrivacyCommitmentCodec centralizes safe reconstruction of PrivacyCommitment
 * objects from deterministic text serialization.
 *
 * Security principle:
 * Private accounting objects must not be parsed through scattered ad-hoc
 * helpers. Each privacy object should have one audited serialization boundary.
 *
 * Current status:
 * This codec supports the current deterministic text format. Future versions
 * should migrate this boundary to a stricter canonical encoding.
 */
class PrivacyCommitmentCodec {
public:
    static privacy::PrivacyCommitment deserialize(
        const std::string& serialized
    );

    static std::vector<privacy::PrivacyCommitment> deserializeList(
        const std::string& serializedList
    );

private:
    static privacy::PrivacyCommitmentType parsePrivacyCommitmentType(
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