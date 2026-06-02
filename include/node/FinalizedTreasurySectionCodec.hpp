#ifndef NODO_NODE_FINALIZED_TREASURY_SECTION_CODEC_HPP
#define NODO_NODE_FINALIZED_TREASURY_SECTION_CODEC_HPP

#include "node/FinalizedTreasurySection.hpp"

#include <string>

namespace nodo::node {

/*
 * FinalizedTreasurySectionCodec serializes and deserializes a
 * FinalizedTreasurySection using the deterministic key=value text format
 * shared across all Nodo artifact codecs.
 *
 * Schema id: NODO_FINALIZED_TREASURY_SECTION (versionless)
 */
class FinalizedTreasurySectionCodec {
public:
    static const std::string& schemaId();

    static std::string encode(const FinalizedTreasurySection& section);
    static FinalizedTreasurySection decode(const std::string& contents);
};

} // namespace nodo::node

#endif
