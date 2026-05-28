#ifndef NODO_SERIALIZATION_MINT_RECORD_CODEC_HPP
#define NODO_SERIALIZATION_MINT_RECORD_CODEC_HPP

#include "economics/MintRecord.hpp"

#include <string>

namespace nodo::serialization {

/*
 * MintRecordCodec centralizes safe reconstruction of MintRecord objects from
 * deterministic text serialization.
 *
 * Security principle:
 * Coin creation records are part of the monetary supply boundary. Parsing them
 * must happen through a single audited serialization module.
 *
 * Current status:
 * This codec supports the current deterministic text format. Future versions
 * should migrate this boundary to a stricter canonical encoding.
 */
class MintRecordCodec {
public:
    static economics::MintRecord deserialize(
        const std::string& serialized
    );

private:
    static std::int64_t parseSigned64(
        const std::string& value,
        const std::string& fieldName
    );

    static std::uint64_t parseUnsigned64(
        const std::string& value,
        const std::string& fieldName
    );
};

} // namespace nodo::serialization

#endif