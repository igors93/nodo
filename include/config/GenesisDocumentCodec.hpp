#ifndef NODO_CONFIG_GENESIS_DOCUMENT_CODEC_HPP
#define NODO_CONFIG_GENESIS_DOCUMENT_CODEC_HPP

#include "config/NetworkParameters.hpp"

#include <filesystem>
#include <string>

namespace nodo::config {

/*
 * GenesisDocumentCodec reads and writes the operator genesis document: the
 * file that carries a network's genesis when that genesis is not built into
 * the binary (see GenesisRegistry::requiresOperatorGenesis).
 *
 * Security principle:
 * The document carries only what a genesis ceremony decides — timestamp,
 * memo, bootstrap validator public keys with their owners, and funded
 * accounts. Network parameters (quorum, fees, limits) are never read from
 * the file; they always come from the code-defined profile of the declared
 * network, so a genesis document cannot weaken protocol rules. The declared
 * chain id and protocol version must match that profile exactly.
 *
 * Decoding is strict: unknown fields, missing fields, non-canonical hex,
 * malformed addresses, and duplicate entries are rejected.
 */
class GenesisDocumentCodec {
public:
  static constexpr const char *VERSION = "NODO_GENESIS_DOCUMENT_V1";

  // Serializes the genesis without judging it; decode() is the validator.
  // Throws std::invalid_argument if a field cannot be represented in the
  // document format (for example an empty memo).
  static std::string encode(const GenesisConfig &genesis);

  // Throws std::invalid_argument on any malformed or inconsistent document.
  static GenesisConfig decode(const std::string &contents);

  // Reads and decodes a genesis document file.
  // Throws std::invalid_argument or std::runtime_error on failure.
  static GenesisConfig loadFile(const std::filesystem::path &path);
};

} // namespace nodo::config

#endif
