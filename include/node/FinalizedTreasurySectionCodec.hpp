#ifndef NODO_NODE_FINALIZED_TREASURY_SECTION_CODEC_HPP
#define NODO_NODE_FINALIZED_TREASURY_SECTION_CODEC_HPP

#include "node/FinalizedTreasurySection.hpp"
#include "serialization/KeyValueFileCodec.hpp"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace nodo::node {

/*
 * FinalizedTreasurySectionCodec serializes and deserializes a
 * FinalizedTreasurySection using the deterministic key=value text format
 * shared across all Nodo artifact codecs.
 *
 * Two modes of operation:
 *
 * 1. Standalone (with schema header) — for persisting the section as its own file.
 *    Schema id: NODO_FINALIZED_TREASURY_SECTION (versionless)
 *    Use encode() / decode().
 *
 * 2. Embedded (without schema header) — for including the section inside a larger
 *    artifact document (FinalizedBlockArtifact).
 *    Fields use a "treasury." prefix following FinalizedMonetarySectionCodec patterns.
 *    Use appendFields() / spendCountFromDocument() / addAllowedFields() / decodeFromDocument().
 */
class FinalizedTreasurySectionCodec {
public:
    using FieldList = std::vector<std::pair<std::string, std::string>>;

    static const std::string& schemaId();

    // Standalone encode/decode (with schema header).
    static std::string encode(const FinalizedTreasurySection& section);
    static FinalizedTreasurySection decode(const std::string& contents);

    // Embedded mode: read the spend record count from an artifact document.
    static std::size_t spendCountFromDocument(
        const serialization::KeyValueFileDocument& doc
    );

    // Embedded mode: register expected treasury section field names in the
    // allowed set. Must be called before requireOnlyFields on the document.
    static void addAllowedFields(
        std::set<std::string>& allowed,
        std::size_t spendCount
    );

    // Embedded mode: decode treasury section from an artifact document.
    // spendCount must match spendCountFromDocument(doc).
    static FinalizedTreasurySection decodeFromDocument(
        const serialization::KeyValueFileDocument& doc,
        std::size_t spendCount
    );

    // Embedded mode: append treasury section fields to an artifact field list.
    static void appendFields(
        const FinalizedTreasurySection& section,
        FieldList& fields
    );
};

} // namespace nodo::node

#endif
