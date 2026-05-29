#ifndef NODO_SERIALIZATION_BLOCK_SNAPSHOT_HEADER_CODEC_HPP
#define NODO_SERIALIZATION_BLOCK_SNAPSHOT_HEADER_CODEC_HPP

#include "storage/BlockSnapshotHeader.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nodo::serialization {

/*
 * BlockSnapshotHeaderCodec centralizes safe reconstruction of
 * BlockSnapshotHeader objects from serialized block snapshots.
 *
 * Security principle:
 * A block snapshot file must be treated as untrusted input. The header must be
 * parsed through one audited boundary before the full block is deserialized.
 *
 * Current status:
 * This codec supports the current deterministic text format. Future versions
 * should migrate this boundary to a stricter canonical encoding.
 */
class BlockSnapshotHeaderCodec {
public:
    static storage::BlockSnapshotHeader deserializeFromSerializedBlock(
        const std::string& serializedBlock
    );

    static std::string extractHeaderPayload(
        const std::string& serializedBlock
    );

    static std::size_t countLedgerRecordsInHeaderPayload(
        const std::string& headerPayload
    );

    static bool headerPayloadMatchesMetadata(
        const std::string& headerPayload,
        std::uint64_t blockIndex,
        const std::string& previousHash,
        std::int64_t timestamp,
        std::size_t recordCount
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

    static void assertSafePreviousHash(
        const std::string& value,
        const std::string& fieldName
    );

    static std::string hashString(
        const std::string& value
    );
};

} // namespace nodo::serialization

#endif