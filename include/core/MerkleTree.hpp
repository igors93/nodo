#ifndef NODO_CORE_MERKLE_TREE_HPP
#define NODO_CORE_MERKLE_TREE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * MerkleTree builds a binary Merkle tree over a sorted set of leaf payloads
 * and returns the root hash.
 *
 * Properties:
 * - Leaves are sorted before hashing for determinism.
 * - Each leaf is hashed individually before insertion.
 * - Internal nodes are the hash of (left || right).
 * - An odd number of nodes at any level duplicates the last node (standard
 *   Bitcoin-style promotion).
 * - An empty tree returns the hash of the empty string.
 *
 * Security principle:
 * Leaf and node hashes use a domain-separation prefix so a leaf value cannot
 * be confused with an internal node hash. This prevents second-preimage
 * attacks on the tree structure.
 */
class MerkleTree {
public:
    static constexpr const char* LEAF_PREFIX   = "NODO_MERKLE_LEAF:";
    static constexpr const char* NODE_PREFIX   = "NODO_MERKLE_NODE:";
    static constexpr const char* EMPTY_PAYLOAD = "NODO_MERKLE_EMPTY";

    /*
     * Build a Merkle root from an arbitrary list of string payloads.
     * Payloads are sorted lexicographically before hashing.
     */
    static std::string buildRoot(
        std::vector<std::string> leafPayloads
    );

    /*
     * Hash a single leaf with the leaf domain prefix.
     */
    static std::string hashLeaf(const std::string& payload);

    /*
     * Hash two child hashes into a parent node.
     */
    static std::string hashNode(
        const std::string& left,
        const std::string& right
    );

private:
    static std::string hashLayer(
        const std::vector<std::string>& hashes
    );
};

/*
 * MerkleProof holds the sibling hashes needed to reconstruct the root
 * from a single leaf, in order from the leaf to the root.
 */
struct MerkleProofStep {
    std::string siblingHash;
    bool siblingIsLeft;
};

class MerkleProof {
public:
    MerkleProof() = default;

    explicit MerkleProof(
        std::string leafHash,
        std::vector<MerkleProofStep> steps
    );

    const std::string& leafHash() const;
    const std::vector<MerkleProofStep>& steps() const;

    std::string reconstructRoot() const;
    bool verify(const std::string& expectedRoot) const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_leafHash;
    std::vector<MerkleProofStep> m_steps;
};

} // namespace nodo::core

#endif
