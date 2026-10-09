#include "serialization/V1MerkleTree.hpp"

#include "serialization/CanonicalWriter.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace nodo::serialization {

namespace {

using Digest = V1MerkleTree::Digest;

std::string domain(std::string_view label, std::string_view kind) {
  if (!V1MerkleTree::isAllowedKind(kind)) {
    throw std::invalid_argument("Unknown v1 Merkle kind.");
  }
  return std::string(label) + std::string(kind);
}

std::uint64_t splitPoint(std::uint64_t count) {
  // RFC 6962 split: largest power of two strictly less than count.
  std::uint64_t split = 1;
  while (split <= (count - 1) / 2) {
    split *= 2;
  }
  return split;
}

Digest nodeHash(std::string_view kind, const Digest &left,
                const Digest &right) {
  std::vector<unsigned char> joined(left.begin(), left.end());
  joined.insert(joined.end(), right.begin(), right.end());
  return V1EncodingPrimitives::hash(domain("MERKLE-NODE/", kind), joined);
}

Digest countedRoot(std::string_view kind, std::uint64_t count,
                   const Digest &treeHash) {
  CanonicalWriter writer;
  writer.writeUInt32(static_cast<std::uint32_t>(count));
  std::vector<unsigned char> counted = writer.bytes();
  counted.insert(counted.end(), treeHash.begin(), treeHash.end());
  return V1EncodingPrimitives::hash(domain("MERKLE-ROOT/", kind), counted);
}

Digest subtree(std::string_view kind, const std::vector<Digest> &leaves,
               std::uint64_t begin, std::uint64_t end) {
  const std::uint64_t count = end - begin;
  if (count == 1) {
    return leaves[begin];
  }
  const std::uint64_t split = splitPoint(count);
  return nodeHash(kind, subtree(kind, leaves, begin, begin + split),
                  subtree(kind, leaves, begin + split, end));
}

struct PendingProof {
  std::uint64_t index;
  std::size_t slot;
};

Digest buildWithProofs(std::string_view kind, const std::vector<Digest> &leaves,
                       std::uint64_t begin, std::uint64_t end,
                       const std::vector<PendingProof> &pending,
                       std::vector<V1MerkleTree::InclusionProof> &proofs) {
  const std::uint64_t count = end - begin;
  if (pending.empty()) {
    return subtree(kind, leaves, begin, end);
  }
  if (count == 1) {
    return leaves[begin];
  }
  const std::uint64_t split = splitPoint(count);
  std::vector<PendingProof> left;
  std::vector<PendingProof> right;
  for (const PendingProof &item : pending) {
    (item.index < begin + split ? left : right).push_back(item);
  }
  const Digest leftHash =
      buildWithProofs(kind, leaves, begin, begin + split, left, proofs);
  const Digest rightHash =
      buildWithProofs(kind, leaves, begin + split, end, right, proofs);
  // Children finish first, so deeper siblings are appended first and each
  // proof is ordered from the leaf upwards.
  for (const PendingProof &item : left) {
    proofs[item.slot].siblings.push_back(rightHash);
  }
  for (const PendingProof &item : right) {
    proofs[item.slot].siblings.push_back(leftHash);
  }
  return nodeHash(kind, leftHash, rightHash);
}

std::optional<Digest> rebuild(std::string_view kind, std::uint64_t count,
                              std::uint64_t index, const Digest &leaf,
                              const std::vector<Digest> &siblings,
                              std::size_t &remaining) {
  if (count == 1) {
    return leaf;
  }
  if (remaining == 0) {
    return std::nullopt;
  }
  const Digest &sibling = siblings[--remaining];
  const std::uint64_t split = splitPoint(count);
  if (index < split) {
    const auto left = rebuild(kind, split, index, leaf, siblings, remaining);
    if (!left) {
      return std::nullopt;
    }
    return nodeHash(kind, *left, sibling);
  }
  const auto right =
      rebuild(kind, count - split, index - split, leaf, siblings, remaining);
  if (!right) {
    return std::nullopt;
  }
  return nodeHash(kind, sibling, *right);
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

} // namespace

bool V1MerkleTree::isAllowedKind(std::string_view kind) {
  return kind == "tx" || kind == "receipt" || kind == "evidence" ||
         kind == "state" || kind == "archive" || kind == "archive-index" ||
         kind == "snapshot-chunk";
}

V1MerkleTree::Digest
V1MerkleTree::leafHash(std::string_view kind, std::uint64_t index,
                       std::span<const unsigned char> element) {
  if (index >= kMaxLeaves) {
    throw std::length_error("V1 Merkle leaf index exceeds the leaf limit.");
  }
  CanonicalWriter writer;
  writer.writeUInt32(static_cast<std::uint32_t>(index));
  std::vector<unsigned char> bytes = writer.bytes();
  bytes.insert(bytes.end(), element.begin(), element.end());
  return V1EncodingPrimitives::hash(domain("MERKLE-LEAF/", kind), bytes);
}

V1MerkleTree::Digest
V1MerkleTree::root(std::string_view kind,
                   const std::vector<std::vector<unsigned char>> &elements) {
  if (elements.size() > kMaxLeaves) {
    throw std::length_error("V1 Merkle tree exceeds its leaf count limit.");
  }
  std::vector<Digest> leaves;
  leaves.reserve(elements.size());
  for (std::size_t index = 0; index < elements.size(); ++index) {
    leaves.push_back(leafHash(kind, index, elements[index]));
  }
  return rootFromLeafHashes(kind, leaves);
}

V1MerkleTree::Digest
V1MerkleTree::rootFromLeafHashes(std::string_view kind,
                                 const std::vector<Digest> &leafHashes) {
  if (leafHashes.size() > kMaxLeaves) {
    throw std::length_error("V1 Merkle tree exceeds its leaf count limit.");
  }
  if (leafHashes.empty()) {
    return V1EncodingPrimitives::hash(domain("MERKLE-EMPTY/", kind), {});
  }
  return countedRoot(kind, leafHashes.size(),
                     subtree(kind, leafHashes, 0, leafHashes.size()));
}

std::vector<V1MerkleTree::InclusionProof>
V1MerkleTree::prove(std::string_view kind,
                    const std::vector<Digest> &leafHashes,
                    const std::vector<std::uint64_t> &indices) {
  if (!isAllowedKind(kind)) {
    throw std::invalid_argument("Unknown v1 Merkle kind.");
  }
  if (leafHashes.empty() || leafHashes.size() > kMaxLeaves) {
    throw std::invalid_argument("V1 Merkle proof requires a bounded tree.");
  }
  std::vector<InclusionProof> proofs(indices.size());
  std::vector<PendingProof> pending;
  pending.reserve(indices.size());
  for (std::size_t slot = 0; slot < indices.size(); ++slot) {
    if (indices[slot] >= leafHashes.size()) {
      throw std::out_of_range("V1 Merkle proof index is outside the tree.");
    }
    proofs[slot].leafIndex = indices[slot];
    proofs[slot].treeSize = leafHashes.size();
    pending.push_back({indices[slot], slot});
  }
  buildWithProofs(kind, leafHashes, 0, leafHashes.size(), pending, proofs);
  return proofs;
}

bool V1MerkleTree::verify(std::string_view kind, const Digest &expectedRoot,
                          const InclusionProof &proof,
                          std::span<const unsigned char> element) {
  try {
    if (proof.treeSize == 0 || proof.treeSize > kMaxLeaves ||
        proof.leafIndex >= proof.treeSize) {
      return false;
    }
    return verifyLeafHash(kind, expectedRoot, proof,
                          leafHash(kind, proof.leafIndex, element));
  } catch (const std::exception &) {
    return false;
  }
}

bool V1MerkleTree::verifyLeafHash(std::string_view kind,
                                  const Digest &expectedRoot,
                                  const InclusionProof &proof,
                                  const Digest &leafHashValue) {
  try {
    if (!isAllowedKind(kind) || proof.treeSize == 0 ||
        proof.treeSize > kMaxLeaves || proof.leafIndex >= proof.treeSize ||
        proof.siblings.size() > kMaxProofSiblings) {
      return false;
    }
    std::size_t remaining = proof.siblings.size();
    const std::optional<Digest> treeHash =
        rebuild(kind, proof.treeSize, proof.leafIndex, leafHashValue,
                proof.siblings, remaining);
    if (!treeHash || remaining != 0) {
      return false;
    }
    return countedRoot(kind, proof.treeSize, *treeHash) == expectedRoot;
  } catch (const std::exception &) {
    return false;
  }
}

std::optional<V1MerkleTree::Digest>
V1MerkleTree::digestFromHex(std::string_view hex) {
  Digest digest{};
  if (hex.size() != digest.size() * 2) {
    return std::nullopt;
  }
  for (std::size_t index = 0; index < digest.size(); ++index) {
    const int high = hexValue(hex[index * 2]);
    const int low = hexValue(hex[index * 2 + 1]);
    if (high < 0 || low < 0) {
      return std::nullopt;
    }
    digest[index] = static_cast<unsigned char>((high << 4) | low);
  }
  return digest;
}

} // namespace nodo::serialization
