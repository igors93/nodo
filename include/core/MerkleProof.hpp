#ifndef NODO_CORE_MERKLE_PROOF_HPP
#define NODO_CORE_MERKLE_PROOF_HPP

#include <string>
#include <vector>

namespace nodo::core {

struct MerkleProofNode {
    bool isRight;
    std::string hash;
};

class MerkleProof {
public:
    MerkleProof(const std::string& leaf, const std::vector<MerkleProofNode>& path);

    bool verify(const std::string& rootHash) const;

    const std::string& leaf() const;
    const std::vector<MerkleProofNode>& path() const;

private:
    std::string m_leaf;
    std::vector<MerkleProofNode> m_path;
};

} // namespace nodo::core

#endif
