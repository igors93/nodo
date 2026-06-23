#include "core/MerkleProof.hpp"
#include "crypto/hash.h"

namespace nodo::core {

namespace {

std::string computeHash(const std::string& left, const std::string& right) {
    std::string combined = left + right;
    char hashOut[NODO_HASH_BUFFER_SIZE];
    nodo_hash_bytes(
        reinterpret_cast<const unsigned char*>(combined.data()),
        combined.size(),
        hashOut,
        NODO_HASH_BUFFER_SIZE
    );
    return std::string(hashOut, NODO_HASH_HEX_SIZE);
}

} // namespace

PrunerMerkleProof::PrunerMerkleProof(
    const std::string& leaf,
    const std::vector<PrunerMerkleProofNode>& path
) : m_leaf(leaf), m_path(path) {}

bool PrunerMerkleProof::verify(const std::string& rootHash) const {
    std::string currentHash = m_leaf;

    for (const auto& node : m_path) {
        if (node.isRight) {
            currentHash = computeHash(currentHash, node.hash);
        } else {
            currentHash = computeHash(node.hash, currentHash);
        }
    }

    return currentHash == rootHash;
}

const std::string& PrunerMerkleProof::leaf() const {
    return m_leaf;
}

const std::vector<PrunerMerkleProofNode>& PrunerMerkleProof::path() const {
    return m_path;
}

} // namespace nodo::core
