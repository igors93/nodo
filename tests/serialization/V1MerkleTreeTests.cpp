#include "../common/TestFramework.hpp"

#include "serialization/V1EncodingPrimitives.hpp"
#include "serialization/V1MerkleTree.hpp"

#include <iostream>
#include <string>
#include <vector>

using nodo::serialization::V1EncodingPrimitives;
using nodo::serialization::V1MerkleTree;
using nodo::test::require;

namespace {

std::vector<std::vector<unsigned char>> elements(std::size_t count) {
  std::vector<std::vector<unsigned char>> result;
  for (std::size_t index = 0; index < count; ++index) {
    const std::string text = "element-" + std::to_string(index);
    result.emplace_back(text.begin(), text.end());
  }
  return result;
}

std::vector<V1MerkleTree::Digest>
leaves(const std::string &kind, const std::vector<std::vector<unsigned char>> &items) {
  std::vector<V1MerkleTree::Digest> result;
  for (std::size_t index = 0; index < items.size(); ++index) {
    result.push_back(V1MerkleTree::leafHash(kind, index, items[index]));
  }
  return result;
}

void testMatchesAdr0008Roots() {
  for (const std::string kind : {"tx", "receipt", "evidence", "state"}) {
    for (std::size_t count = 0; count <= 9; ++count) {
      require(V1MerkleTree::root(kind, elements(count)) ==
                  V1EncodingPrimitives::merkleRoot(kind, elements(count)),
              "history Merkle tree must equal the ADR 0008 tree for " + kind);
    }
  }
}

void testEveryProofVerifies() {
  for (std::size_t count = 1; count <= 33; ++count) {
    const auto items = elements(count);
    const auto hashes = leaves("archive", items);
    const auto root = V1MerkleTree::rootFromLeafHashes("archive", hashes);
    std::vector<std::uint64_t> indices;
    for (std::size_t index = 0; index < count; ++index) {
      indices.push_back(index);
    }
    const auto proofs = V1MerkleTree::prove("archive", hashes, indices);
    for (std::size_t index = 0; index < count; ++index) {
      require(V1MerkleTree::verify("archive", root, proofs[index], items[index]),
              "proof must verify for size " + std::to_string(count));
      // Wrong element, index, size, sibling or kind must all fail.
      require(!V1MerkleTree::verify("archive", root, proofs[index],
                                    items[(index + 1) % count]) ||
                  count == 1,
              "proof must bind the element");
      auto wrongSize = proofs[index];
      wrongSize.treeSize = count + 1;
      require(!V1MerkleTree::verify("archive", root, wrongSize, items[index]),
              "proof must bind the tree size");
      if (!proofs[index].siblings.empty()) {
        auto tampered = proofs[index];
        tampered.siblings.front()[0] ^= 0x01;
        require(!V1MerkleTree::verify("archive", root, tampered, items[index]),
                "proof must bind every sibling");
        auto shortened = proofs[index];
        shortened.siblings.pop_back();
        require(!V1MerkleTree::verify("archive", root, shortened, items[index]),
                "proof must have exactly the tree depth");
      }
      require(!V1MerkleTree::verify("snapshot-chunk", root, proofs[index],
                                    items[index]),
              "proof must bind the Merkle kind");
    }
  }
}

void testOrderAndDuplicationMatter() {
  auto items = elements(3);
  const auto root = V1MerkleTree::root("archive", items);
  std::swap(items[0], items[1]);
  require(V1MerkleTree::root("archive", items) != root, "order is committed");
  auto duplicated = elements(3);
  duplicated.push_back(duplicated.back());
  require(V1MerkleTree::root("archive", duplicated) != root,
          "odd leaves are never duplicated (CVE-2012-2459 class)");
  bool rejected = false;
  try {
    (void)V1MerkleTree::root("unknown-kind", items);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "unknown kinds are refused");
  require(!V1MerkleTree::digestFromHex(std::string(63, 'a')).has_value() &&
              !V1MerkleTree::digestFromHex(std::string(64, 'G')).has_value() &&
              V1MerkleTree::digestFromHex(std::string(64, 'a')).has_value(),
          "digest hex parsing is strict");
}

} // namespace

int main() {
  try {
    testMatchesAdr0008Roots();
    testEveryProofVerifies();
    testOrderAndDuplicationMatter();
    std::cout << "V1 Merkle tree tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "V1 Merkle tree tests failed: " << error.what() << "\n";
    return 1;
  }
}
