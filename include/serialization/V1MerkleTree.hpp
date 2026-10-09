#ifndef NODO_SERIALIZATION_V1_MERKLE_TREE_HPP
#define NODO_SERIALIZATION_V1_MERKLE_TREE_HPP

#include "serialization/V1EncodingPrimitives.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace nodo::serialization {

// Inclusion proofs for the ordered, count-committed v1 Merkle tree of ADR
// 0008. Leaves are H("MERKLE-LEAF/"+kind, u32(index)||element), interior
// nodes H("MERKLE-NODE/"+kind, left||right) with the RFC 6962 split, and the
// root H("MERKLE-ROOT/"+kind, u32(n)||tree_hash). For the ADR 0008 kinds the
// root equals V1EncodingPrimitives::merkleRoot; ADR 0014 adds the history
// kinds below. A proof lists sibling hashes from the leaf upwards.
class V1MerkleTree {
public:
  using Digest = V1EncodingPrimitives::Digest;

  static constexpr std::uint64_t kMaxLeaves =
      V1EncodingPrimitives::kMaxListElements;
  static constexpr std::size_t kMaxProofSiblings = 64;

  struct InclusionProof {
    std::uint64_t leafIndex = 0;
    std::uint64_t treeSize = 0;
    std::vector<Digest> siblings;
  };

  // tx, receipt, evidence and state (ADR 0008); archive, archive-index and
  // snapshot-chunk (ADR 0014).
  static bool isAllowedKind(std::string_view kind);

  static Digest leafHash(std::string_view kind, std::uint64_t index,
                         std::span<const unsigned char> element);

  static Digest root(std::string_view kind,
                     const std::vector<std::vector<unsigned char>> &elements);

  // Root over leaf hashes already produced by leafHash, so callers can stream
  // large inputs without holding every element in memory.
  static Digest rootFromLeafHashes(std::string_view kind,
                                   const std::vector<Digest> &leafHashes);

  // One proof per requested index, in the order requested. Every index must
  // be below the leaf count. Builds all proofs in a single pass.
  static std::vector<InclusionProof>
  prove(std::string_view kind, const std::vector<Digest> &leafHashes,
        const std::vector<std::uint64_t> &indices);

  static bool verify(std::string_view kind, const Digest &expectedRoot,
                     const InclusionProof &proof,
                     std::span<const unsigned char> element);

  static bool verifyLeafHash(std::string_view kind, const Digest &expectedRoot,
                             const InclusionProof &proof,
                             const Digest &leafHash);

  static std::optional<Digest> digestFromHex(std::string_view hex);
};

} // namespace nodo::serialization

#endif
