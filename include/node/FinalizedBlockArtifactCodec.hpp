#ifndef NODO_NODE_FINALIZED_BLOCK_ARTIFACT_CODEC_HPP
#define NODO_NODE_FINALIZED_BLOCK_ARTIFACT_CODEC_HPP

#include "core/Block.hpp"
#include "node/FinalizedBlockArtifact.hpp"

#include <filesystem>
#include <string>

namespace nodo::node {

class FinalizedBlockArtifactCodec {
public:
    static core::Block readBlockFile(
        const std::filesystem::path& path
    );

    static core::Block decodeBlockFileContents(
        const std::string& contents
    );

    static FinalizedBlockArtifact readBlockArtifactFile(
        const std::filesystem::path& path
    );

    static FinalizedBlockArtifact decodeBlockArtifactFileContents(
        const std::string& contents
    );
};

using FinalizedBlockFileCodec = FinalizedBlockArtifactCodec;

} // namespace nodo::node

#endif
