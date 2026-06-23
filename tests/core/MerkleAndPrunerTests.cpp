#include "core/MerkleProof.hpp"
#include "core/StatePruner.hpp"
#include "crypto/hash.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::PrunerMerkleProof;
using nodo::core::PrunerMerkleProofNode;
using nodo::core::StatePruner;

std::string proofHash(
    const std::string& left,
    const std::string& right
) {
    const std::string combined = left + right;
    char hashOut[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_bytes(
        reinterpret_cast<const unsigned char*>(combined.data()),
        combined.size(),
        hashOut,
        NODO_HASH_BUFFER_SIZE
    );

    return std::string(hashOut, NODO_HASH_HEX_SIZE);
}

void requireCondition(bool condition, const std::string& failureMessage) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testMerkleProofVerification() {
    // Let's create a mockup of Merkle proof validation.
    // Leaves: L1="apple", L2="banana"
    // H1 = hash(L1), H2 = hash(L2)
    // Root = hash(H1 + H2)
    char h1[NODO_HASH_BUFFER_SIZE];
    char h2[NODO_HASH_BUFFER_SIZE];
    nodo_hash_bytes(reinterpret_cast<const unsigned char*>("apple"), 5, h1, NODO_HASH_BUFFER_SIZE);
    nodo_hash_bytes(reinterpret_cast<const unsigned char*>("banana"), 6, h2, NODO_HASH_BUFFER_SIZE);

    std::string h1Str(h1, NODO_HASH_HEX_SIZE);
    std::string h2Str(h2, NODO_HASH_HEX_SIZE);

    std::string rootStr =
        proofHash(h1Str, h2Str);

    // Proof for L1 ("apple", which hashes to h1Str)
    // The sibling is h2Str, which is on the right.
    std::vector<PrunerMerkleProofNode> path = {
        {true, h2Str} // isRight = true, hash = h2Str
    };

    PrunerMerkleProof proof(h1Str, path);
    requireCondition(proof.verify(rootStr), "Merkle proof should verify successfully for L1");
    requireCondition(!proof.verify("invalid-root"), "Merkle proof should fail for incorrect root");
}

void testMerkleProofPreservesBinaryHashInputs() {
    const std::string leafWithZero(
        std::string("left", 4) + '\0' + std::string("leaf", 4)
    );
    const std::string siblingWithZero(
        std::string("right", 5) + '\0' + std::string("sibling", 7)
    );

    const std::string root =
        proofHash(leafWithZero, siblingWithZero);

    std::vector<PrunerMerkleProofNode> path = {
        {true, siblingWithZero}
    };

    PrunerMerkleProof proof(leafWithZero, path);
    requireCondition(
        proof.leaf().size() == 9U,
        "Merkle proof fixture should contain an embedded zero byte."
    );
    requireCondition(
        proof.verify(root),
        "Merkle proof should verify binary hash inputs without C-string truncation."
    );
}

void testStatePruner() {
    StatePruner pruner(3); // Keep only 3 recent states

    pruner.recordStateRoot(1, "root1");
    pruner.recordStateRoot(2, "root2");
    pruner.recordStateRoot(3, "root3");
    pruner.recordStateRoot(4, "root4");

    // Prune history at height 4. Since keep count is 3, threshold is 4 - 3 = 1.
    // So height < 1 (none) should be pruned.
    pruner.pruneHistory(4);
    requireCondition(pruner.hasStateRoot(1), "Height 1 should still exist");

    // Prune history at height 5. Threshold is 5 - 3 = 2.
    // So height < 2 (height 1) should be pruned.
    pruner.pruneHistory(5);
    requireCondition(!pruner.hasStateRoot(1), "Height 1 should be pruned");
    requireCondition(pruner.hasStateRoot(2), "Height 2 should still exist");
    requireCondition(pruner.hasStateRoot(3), "Height 3 should still exist");
    requireCondition(pruner.hasStateRoot(4), "Height 4 should still exist");
    requireCondition(pruner.prunedCount() == 1, "Should have pruned 1 state");
}

} // namespace

int main() {
    try {
        testMerkleProofVerification();
        testMerkleProofPreservesBinaryHashInputs();
        testStatePruner();
        std::cout << "Nodo Merkle and state pruner tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo Merkle and state pruner tests failed: " << error.what() << "\n";
        return 1;
    }
}
