#ifndef NODO_SERIALIZATION_BLOCK_STORAGE_INDEX_CODEC_HPP
#define NODO_SERIALIZATION_BLOCK_STORAGE_INDEX_CODEC_HPP

#include "storage/BlockStorageIndex.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * BlockStorageIndexCodec centralizes safe reconstruction of BlockIndexEntry
 * and BlockStorageIndex objects from deterministic text serialization.
 *
 * Security principle:
 * The block storage index is the map that tells Nodo which block snapshot file
 * belongs to each block height and hash. It must be parsed through one audited
 * boundary instead of ad-hoc parsing inside storage objects.
 *
 * Current status:
 * This codec supports the current deterministic text format. Future versions
 * should migrate this boundary to a stricter canonical encoding.
 */
class BlockStorageIndexCodec {
public:
    static storage::BlockIndexEntry deserializeEntry(
        const std::string& serialized
    );

    static std::vector<storage::BlockIndexEntry> deserializeEntryList(
        const std::string& serializedList
    );

    static storage::BlockStorageIndex deserialize(
        const std::string& serialized
    );

private:
    static std::uint64_t parseUnsigned64(
        const std::string& value,
        const std::string& fieldName
    );

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

    static void assertSafeFileName(
        const std::string& value,
        const std::string& fieldName
    );
};

} // namespace nodo::serialization

#endif