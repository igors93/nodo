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
 * 1. Standalone (with schema header) — for persisting the section as its own
 * file. Schema id: NODO_FINALIZED_TREASURY_SECTION (versionless) Use encode() /
 * decode().
 *
 * 2. Embedded (without schema header) — for including the section inside a
 * larger artifact document (FinalizedBlockArtifact). Fields use a
 * "treasurySection." prefix following FinalizedMonetarySectionCodec patterns,
 * mirroring the standalone mode's evidence-based encoding exactly (an empty
 * section is just evidenceCount=0). Use appendFields() /
 * evidenceCountFromDocument() / addAllowedFields() / decodeFromDocument().
 */
class FinalizedTreasurySectionCodec {
public:
  using FieldList = std::vector<std::pair<std::string, std::string>>;

  static const std::string &schemaId();

  // Standalone encode/decode (with schema header).
  static std::string encode(const FinalizedTreasurySection &section);
  static FinalizedTreasurySection decode(const std::string &contents);

  // Embedded mode: read the treasury execution evidence count from an
  // artifact document.
  static std::size_t
  evidenceCountFromDocument(const serialization::KeyValueFileDocument &doc);

  // Embedded mode: register expected treasury section field names in the
  // allowed set. Must be called before requireOnlyFields on the document.
  static void addAllowedFields(const serialization::KeyValueFileDocument &doc,
                               std::set<std::string> &allowed,
                               std::size_t evidenceCount);

  // Embedded mode: decode treasury section from an artifact document.
  // evidenceCount must match evidenceCountFromDocument(doc).
  static FinalizedTreasurySection
  decodeFromDocument(const serialization::KeyValueFileDocument &doc,
                     std::size_t evidenceCount);

  // Embedded mode: append treasury section fields to an artifact field list.
  static void appendFields(const FinalizedTreasurySection &section,
                           FieldList &fields);
};

} // namespace nodo::node

#endif
