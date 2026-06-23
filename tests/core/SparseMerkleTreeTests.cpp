#include "core/SparseMerkleTree.hpp"
#include "crypto/hash.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string hashString(const std::string& input) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(input.c_str(), output, sizeof(output));
    return std::string(output);
}

// Generate a valid 64-character hex key from a seed
std::string makeKey(const std::string& seed) {
    return hashString(seed);
}

void testEmptyTree() {
    nodo::core::SparseMerkleTree smt;
    std::string emptyRoot = smt.root();
    
    requireCondition(!emptyRoot.empty(), "Empty root should not be empty");
    
    // An empty SMT root should match proof verification of empty leaf
    std::string key = makeKey("non-existent");
    nodo::core::MerkleProof proof = smt.generateProof(key);
    requireCondition(proof.verify(emptyRoot), "Proof for empty tree should verify against empty root");
}

void testOrderIndependence() {
    std::string k1 = makeKey("key1");
    std::string k2 = makeKey("key2");
    std::string k3 = makeKey("key3");
    
    std::string v1 = hashString("val1");
    std::string v2 = hashString("val2");
    std::string v3 = hashString("val3");

    // SMT 1: 1, 2, 3
    nodo::core::SparseMerkleTree smt1;
    smt1.update(k1, v1);
    smt1.update(k2, v2);
    smt1.update(k3, v3);

    // SMT 2: 3, 2, 1
    nodo::core::SparseMerkleTree smt2;
    smt2.update(k3, v3);
    smt2.update(k2, v2);
    smt2.update(k1, v1);

    requireCondition(smt1.root() == smt2.root(), "SMT roots must be order-independent");
}

void testUpdateValue() {
    std::string k1 = makeKey("key1");
    std::string v1 = hashString("val1");
    std::string v2 = hashString("val2");

    nodo::core::SparseMerkleTree smt;
    smt.update(k1, v1);
    std::string root1 = smt.root();

    smt.update(k1, v2);
    std::string root2 = smt.root();

    requireCondition(root1 != root2, "Updating key value must change the root");

    // SMT starting with v2 directly
    nodo::core::SparseMerkleTree smtDirect;
    smtDirect.update(k1, v2);
    requireCondition(smtDirect.root() == root2, "Root with updated value must match root initialized with that value");
}

void testProofVerification() {
    std::string k1 = makeKey("key1");
    std::string k2 = makeKey("key2");
    
    std::string v1 = hashString("val1");
    std::string v2 = hashString("val2");

    nodo::core::SparseMerkleTree smt;
    smt.update(k1, v1);
    smt.update(k2, v2);

    std::string root = smt.root();

    // Verify inclusion proof for k1
    nodo::core::MerkleProof p1 = smt.generateProof(k1);
    requireCondition(p1.verify(root), "Inclusion proof for k1 must verify against the SMT root");
    requireCondition(p1.leafHash() == v1, "Proof leaf hash must be v1");

    // Verify inclusion proof for k2
    nodo::core::MerkleProof p2 = smt.generateProof(k2);
    requireCondition(p2.verify(root), "Inclusion proof for k2 must verify against the SMT root");
    requireCondition(p2.leafHash() == v2, "Proof leaf hash must be v2");

    // Verify non-inclusion proof for k3
    std::string k3 = makeKey("key3");
    nodo::core::MerkleProof p3 = smt.generateProof(k3);
    requireCondition(p3.verify(root), "Non-inclusion proof for k3 must verify against the SMT root");
    
    // Sibling proof steps must be exactly 256
    requireCondition(p1.steps().size() == 256, "SMT proof must have exactly 256 steps");
    requireCondition(p3.steps().size() == 256, "SMT non-inclusion proof must have exactly 256 steps");
}

} // namespace

int main() {
    try {
        testEmptyTree();
        testOrderIndependence();
        testUpdateValue();
        testProofVerification();

        std::cout << "SparseMerkleTree tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SparseMerkleTree tests failed: " << error.what() << "\n";
        return 1;
    }
}
