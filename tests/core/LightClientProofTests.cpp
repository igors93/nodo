#include "core/LightClientProof.hpp"
#include "core/MerkleTree.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::AccountStateProof;
using nodo::core::LightClientVerifier;
using nodo::core::MerkleTree;
using nodo::core::TransactionInclusionProof;

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Build a minimal 2-leaf Merkle tree and return the path for leaf index 0.
// Tree: root = hashNode(hashLeaf(leaf0), hashLeaf(leaf1))
//       leaf0 is left child, sibling is hashLeaf(leaf1)
void testVerifyTransactionInclusionValid() {
    const std::string txId   = "tx-abc-123";
    const std::string sibling = "tx-def-456";

    const std::string leafHash    = MerkleTree::hashLeaf(txId);
    const std::string siblingHash = MerkleTree::hashLeaf(sibling);

    // The leaf is the left child, so root = hashNode(leafHash, siblingHash)
    const std::string expectedRoot = MerkleTree::hashNode(leafHash, siblingHash);

    // Construct proof: leaf is left child (isLeftChild=true), one path step = siblingHash
    TransactionInclusionProof proof(
        txId,
        100,
        "block-hash-abc",
        expectedRoot,
        { siblingHash },
        true   // isLeftChild
    );

    requireCondition(
        proof.isValid(),
        "TransactionInclusionProof should be valid."
    );

    requireCondition(
        LightClientVerifier::verifyTransactionInclusion(proof),
        "verifyTransactionInclusion should succeed for valid 2-level Merkle path."
    );
}

void testVerifyTransactionInclusionTampered() {
    const std::string txId   = "tx-abc-123";
    const std::string sibling = "tx-def-456";

    const std::string leafHash    = MerkleTree::hashLeaf(txId);
    const std::string siblingHash = MerkleTree::hashLeaf(sibling);
    const std::string expectedRoot = MerkleTree::hashNode(leafHash, siblingHash);

    // Tamper: use a wrong transactionsRoot
    TransactionInclusionProof proof(
        txId,
        100,
        "block-hash-abc",
        "tampered-root-value",
        { siblingHash },
        true
    );

    requireCondition(
        !LightClientVerifier::verifyTransactionInclusion(proof),
        "verifyTransactionInclusion should fail when transactionsRoot is tampered."
    );
}

void testVerifyAccountStateValid() {
    const std::string snapshot = "addr=0xDEAD;balance=5000;nonce=3";
    const std::string sibling  = "addr=0xBEEF;balance=1000;nonce=1";

    const std::string leafHash    = MerkleTree::hashLeaf(snapshot);
    const std::string siblingHash = MerkleTree::hashLeaf(sibling);

    // snapshot leaf is right child; sibling is on the left
    const std::string expectedRoot = MerkleTree::hashNode(siblingHash, leafHash);

    AccountStateProof proof(
        "0xDEAD",
        200,
        "block-hash-xyz",
        expectedRoot,
        snapshot,
        { siblingHash },
        false   // isLeftChild = false → snapshot is right child
    );

    requireCondition(
        proof.isValid(),
        "AccountStateProof should be valid."
    );

    requireCondition(
        LightClientVerifier::verifyAccountState(proof),
        "verifyAccountState should succeed for valid path."
    );
}

void testDefaultConstructedProofsAreInvalid() {
    TransactionInclusionProof txProof;
    requireCondition(
        !txProof.isValid(),
        "Default-constructed TransactionInclusionProof should be invalid."
    );

    AccountStateProof accProof;
    requireCondition(
        !accProof.isValid(),
        "Default-constructed AccountStateProof should be invalid."
    );
}

} // namespace

int main() {
    try {
        testVerifyTransactionInclusionValid();
        testVerifyTransactionInclusionTampered();
        testVerifyAccountStateValid();
        testDefaultConstructedProofsAreInvalid();

        std::cout << "Nodo LightClientProof tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo LightClientProof tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
