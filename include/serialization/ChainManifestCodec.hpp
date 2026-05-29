#ifndef NODO_SERIALIZATION_CHAIN_MANIFEST_CODEC_HPP
#define NODO_SERIALIZATION_CHAIN_MANIFEST_CODEC_HPP

#include "storage/ChainManifest.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nodo::serialization {

/*
 * ChainManifestCodec centralizes safe reconstruction of ChainManifest objects
 * from deterministic text serialization.
 *
 * Security principle:
 * The chain manifest is the compact storage summary that tells Nodo which
 * persisted chain it expects. It must be parsed through one audited boundary
 * instead of ad-hoc parsing inside the storage object.
 *
 * Current status:
 * This codec supports the current deterministic text format. Future versions
 * should migrate this boundary to a stricter canonical encoding.
 */
class ChainManifestCodec {
public:
    static storage::ChainManifest deserialize(
        const std::string& serialized
    );

private:
    static std::size_t parseSize(
        const std::string& value,
        const std::string& fieldName
    );

    static std::int64_t parseSigned64(
        const std::string& value,
        const std::string& fieldName
    );

    static void assertSafeHashLikeField(
        const std::string& value,
        const std::string& fieldName
    );
};

} // namespace nodo::serialization

#endif