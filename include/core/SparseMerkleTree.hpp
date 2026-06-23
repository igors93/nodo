#ifndef NODO_CORE_SPARSE_MERKLE_TREE_HPP
#define NODO_CORE_SPARSE_MERKLE_TREE_HPP

#include "core/MerkleTree.hpp"
#include <string>
#include <vector>
#include <memory>

namespace nodo::core {

/*
 * SparseMerkleTree implements a binary trie of depth 256 for secure state commitment.
 * Key: 256-bit address hash (represented as 64-char hex string).
 * Value: Leaf hash of the account state.
 */
class SparseMerkleTree {
public:
    static constexpr int DEPTH = 256;

    SparseMerkleTree();
    ~SparseMerkleTree();

    // Inserts or updates a leaf in the tree.
    // @param keyHex     64-character hex string representing a 256-bit key.
    // @param valueHash  Pre-calculated hash of the leaf content.
    void update(const std::string& keyHex, const std::string& valueHash);

    // Returns the root hash of the 256-depth tree.
    std::string root() const;

    // Generates a Merkle Proof for the given key.
    MerkleProof generateProof(const std::string& keyHex) const;

private:
    struct Node {
        std::string hash;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

        Node() = default;
    };

    std::unique_ptr<Node> m_root;
    std::vector<std::string> m_defaultHashes;

    void precomputeDefaultHashes();
    std::string getNodeHash(const Node* node, int level) const;
    void updateRecursive(std::unique_ptr<Node>& node, const std::vector<bool>& bits, int level, const std::string& valueHash);
    void collectProofSteps(const Node* node, const std::vector<bool>& bits, int level, std::vector<MerkleProofStep>& steps) const;

    static std::vector<bool> hexToBits(const std::string& hex);
};

} // namespace nodo::core

#endif
