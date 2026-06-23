#include "core/MerkleTree.hpp"

#include "crypto/hash.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace nodo::core {

namespace {

std::string hashString(const std::string& input) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_bytes(
        reinterpret_cast<const unsigned char*>(input.data()),
        input.size(),
        output,
        sizeof(output)
    );
    return std::string(output, NODO_HASH_HEX_SIZE);
}

} // namespace

std::string MerkleTree::hashLeaf(const std::string& payload) {
    return hashString(std::string(LEAF_PREFIX) + payload);
}

std::string MerkleTree::hashNode(
    const std::string& left,
    const std::string& right
) {
    return hashString(std::string(NODE_PREFIX) + left + right);
}

std::string MerkleTree::hashLayer(
    const std::vector<std::string>& hashes
) {
    if (hashes.empty()) {
        return hashString(EMPTY_PAYLOAD);
    }
    if (hashes.size() == 1) {
        return hashes[0];
    }

    std::vector<std::string> next;
    next.reserve((hashes.size() + 1) / 2);

    for (std::size_t i = 0; i < hashes.size(); i += 2) {
        const std::string& left  = hashes[i];
        const std::string& right = (i + 1 < hashes.size()) ? hashes[i + 1] : hashes[i];
        next.push_back(hashNode(left, right));
    }

    return hashLayer(next);
}

std::string MerkleTree::buildRoot(
    std::vector<std::string> leafPayloads
) {
    if (leafPayloads.empty()) {
        return hashString(EMPTY_PAYLOAD);
    }

    std::sort(leafPayloads.begin(), leafPayloads.end());

    std::vector<std::string> leaves;
    leaves.reserve(leafPayloads.size());
    for (const auto& p : leafPayloads) {
        leaves.push_back(hashLeaf(p));
    }

    return hashLayer(leaves);
}

MerkleProof::MerkleProof(
    std::string leafHash,
    std::vector<MerkleProofStep> steps
)
    : m_leafHash(std::move(leafHash))
    , m_steps(std::move(steps))
{}

const std::string& MerkleProof::leafHash() const { return m_leafHash; }
const std::vector<MerkleProofStep>& MerkleProof::steps() const { return m_steps; }

std::string MerkleProof::reconstructRoot() const {
    std::string current = m_leafHash;
    for (const auto& step : m_steps) {
        if (step.siblingIsLeft) {
            current = MerkleTree::hashNode(step.siblingHash, current);
        } else {
            current = MerkleTree::hashNode(current, step.siblingHash);
        }
    }
    return current;
}

bool MerkleProof::verify(const std::string& expectedRoot) const {
    return isValid() && reconstructRoot() == expectedRoot;
}

bool MerkleProof::isValid() const {
    return !m_leafHash.empty();
}

std::string MerkleProof::serialize() const {
    std::ostringstream oss;
    oss << "MerkleProof{leafHash=" << m_leafHash
        << ";steps=" << m_steps.size() << "}";
    return oss.str();
}

} // namespace nodo::core
