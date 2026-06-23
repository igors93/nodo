#include "core/SparseMerkleTree.hpp"
#include "crypto/hash.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace nodo::core {

namespace {

std::string hashString(const std::string& input) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(input.c_str(), output, sizeof(output));
    return std::string(output);
}

} // namespace

SparseMerkleTree::SparseMerkleTree() {
    precomputeDefaultHashes();
    m_root = nullptr;
}

SparseMerkleTree::~SparseMerkleTree() = default;

void SparseMerkleTree::precomputeDefaultHashes() {
    m_defaultHashes.resize(DEPTH + 1);
    m_defaultHashes[0] = hashString("");
    for (int i = 1; i <= DEPTH; ++i) {
        m_defaultHashes[i] = MerkleTree::hashNode(m_defaultHashes[i - 1], m_defaultHashes[i - 1]);
    }
}

std::string SparseMerkleTree::getNodeHash(const Node* node, int level) const {
    if (!node || node->hash.empty()) {
        return m_defaultHashes[level];
    }
    return node->hash;
}

std::vector<bool> SparseMerkleTree::hexToBits(const std::string& hex) {
    if (hex.size() != 64) {
        throw std::invalid_argument("Key hex must be exactly 64 characters (256 bits)");
    }
    std::vector<bool> bits;
    bits.reserve(256);
    for (char c : hex) {
        unsigned int val = 0;
        if (c >= '0' && c <= '9') val = c - '0';
        else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
        else throw std::invalid_argument("Invalid hex character in key");

        for (int i = 3; i >= 0; --i) {
            bits.push_back((val >> i) & 1);
        }
    }
    return bits;
}

void SparseMerkleTree::update(const std::string& keyHex, const std::string& valueHash) {
    std::vector<bool> bits = hexToBits(keyHex);
    updateRecursive(m_root, bits, DEPTH, valueHash);
}

void SparseMerkleTree::updateRecursive(
    std::unique_ptr<Node>& node,
    const std::vector<bool>& bits,
    int level,
    const std::string& valueHash
) {
    if (!node) {
        node = std::make_unique<Node>();
    }

    if (level == 0) {
        node->hash = valueHash;
        return;
    }

    // Traverse down
    bool bit = bits[DEPTH - level];
    if (!bit) {
        updateRecursive(node->left, bits, level - 1, valueHash);
    } else {
        updateRecursive(node->right, bits, level - 1, valueHash);
    }

    std::string leftHash = getNodeHash(node->left.get(), level - 1);
    std::string rightHash = getNodeHash(node->right.get(), level - 1);
    node->hash = MerkleTree::hashNode(leftHash, rightHash);
}

std::string SparseMerkleTree::root() const {
    return getNodeHash(m_root.get(), DEPTH);
}

MerkleProof SparseMerkleTree::generateProof(const std::string& keyHex) const {
    std::vector<bool> bits = hexToBits(keyHex);
    std::vector<MerkleProofStep> steps;
    collectProofSteps(m_root.get(), bits, DEPTH, steps);
    
    // Reverse the steps so they are from leaf to root
    std::reverse(steps.begin(), steps.end());

    // The leaf hash is what we reconstruct the proof from
    // If the leaf is not set, we start reconstruction from the default hash at level 0
    std::string leafHash = m_defaultHashes[0];
    const Node* current = m_root.get();
    for (int level = DEPTH; level > 0; --level) {
        if (!current) break;
        bool bit = bits[DEPTH - level];
        current = bit ? current->right.get() : current->left.get();
    }
    if (current && !current->hash.empty()) {
        leafHash = current->hash;
    }

    return MerkleProof(leafHash, steps);
}

void SparseMerkleTree::collectProofSteps(
    const Node* node,
    const std::vector<bool>& bits,
    int level,
    std::vector<MerkleProofStep>& steps
) const {
    if (level == 0) {
        return;
    }

    bool bit = bits[DEPTH - level];
    if (!bit) {
        // We went left. Sibling is right.
        std::string siblingHash = m_defaultHashes[level - 1];
        if (node && node->right) {
            siblingHash = node->right->hash;
        }
        // siblingIsLeft = false
        steps.push_back({siblingHash, false});
        collectProofSteps(node ? node->left.get() : nullptr, bits, level - 1, steps);
    } else {
        // We went right. Sibling is left.
        std::string siblingHash = m_defaultHashes[level - 1];
        if (node && node->left) {
            siblingHash = node->left->hash;
        }
        // siblingIsLeft = true
        steps.push_back({siblingHash, true});
        collectProofSteps(node ? node->right.get() : nullptr, bits, level - 1, steps);
    }
}

} // namespace nodo::core
