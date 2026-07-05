#include "consensus/ConsensusWeight.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testDeterministicSquareRoot() {
    requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(0) == 0, "sqrt(0) should be 0");
    requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(1) == 1, "sqrt(1) should be 1");
    requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(3) == 1, "sqrt(3) should be 1");
    requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(4) == 2, "sqrt(4) should be 2");
    requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(99) == 9, "sqrt(99) should be 9");
    requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(100) == 10, "sqrt(100) should be 10");
    
    // Large values
    requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(1'000'000ULL) == 1000, "sqrt(1,000,000) should be 1000");
    requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(1'000'000'000ULL) == 31622, "sqrt(1,000,000,000) should be 31622");
    
    // Maximum uint64
    requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(18'446'744'073'709'551'615ULL) == 4'294'967'295ULL, "sqrt(MAX) should be 4,294,967,295");
}

} // namespace

int main() {
    try {
        testDeterministicSquareRoot();
        std::cout << "ConsensusWeight deterministic square root tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ConsensusWeight tests failed: " << error.what() << "\n";
        return 1;
    }
}
