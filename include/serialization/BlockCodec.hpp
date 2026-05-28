#ifndef NODO_SERIALIZATION_BLOCK_CODEC_HPP
#define NODO_SERIALIZATION_BLOCK_CODEC_HPP

#include "core/Block.hpp"

#include <string>
#include <vector>

namespace nodo::serialization {

/*
 * BlockCodec centralizes safe reconstruction of Block objects from the
 * current deterministic text snapshot format.
 *
 * Security principle:
 * A block loaded from disk must not be trusted only because the file exists.
 * It must be parsed, reconstructed, rehashed, and compared with the original
 * serialized snapshot.
 *
 * Current status:
 * This is still a development text codec. It prepares Nodo for a future
 * Blockchain loader while keeping deserialization logic isolated.
 */
class BlockCodec {
public:
    static core::Block deserialize(
        const std::string& serializedBlock
    );

    static core::Block deserializeFromFile(
        const std::string& filePath
    );

    static std::vector<core::Block> deserializeFiles(
        const std::vector<std::string>& filePaths
    );

private:
    static std::string readFile(
        const std::string& filePath
    );
};

} // namespace nodo::serialization

#endif