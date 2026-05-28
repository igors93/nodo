#ifndef NODO_SERIALIZATION_PRIVATE_ACCOUNTING_RECORD_CODEC_HPP
#define NODO_SERIALIZATION_PRIVATE_ACCOUNTING_RECORD_CODEC_HPP

#include "privacy/PrivateAccountingRecord.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * PrivateAccountingRecordCodec centralizes safe reconstruction of
 * PrivateAccountingRecord objects from deterministic text serialization.
 *
 * Security principle:
 * Private accounting records define how commitments, nullifiers, and public
 * supply effects are connected. Parsing them must happen through one audited
 * serialization boundary.
 *
 * Current status:
 * This codec supports the current deterministic text format. Future versions
 * should migrate this boundary to a stricter canonical encoding.
 */
class PrivateAccountingRecordCodec {
public:
    static privacy::PrivateAccountingRecord deserialize(
        const std::string& serialized
    );

    static std::vector<privacy::PrivateAccountingRecord> deserializeList(
        const std::string& serializedList
    );

private:
    static privacy::PrivateAccountingRecordType parsePrivateAccountingRecordType(
        const std::string& value
    );

    static privacy::PublicSupplyEffect parsePublicSupplyEffect(
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