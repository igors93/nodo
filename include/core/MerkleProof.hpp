#ifndef NODO_CORE_MERKLE_PROOF_HPP
#define NODO_CORE_MERKLE_PROOF_HPP

#include <string>
#include <vector>

namespace nodo::core {

struct PrunerMerkleProofNode {
    bool isRight;
    std::string hash;
};

class PrunerMerkleProof {
public:
    PrunerMerkleProof(const std::string& leaf, const std::vector<PrunerMerkleProofNode>& path);

    bool verify(const std::string& rootHash) const;

    const std::string& leaf() const;
    const std::vector<PrunerMerkleProofNode>& path() const;

private:
    std::string m_leaf;
    std::vector<PrunerMerkleProofNode> m_path;
};

} // namespace nodo::core

#endif
