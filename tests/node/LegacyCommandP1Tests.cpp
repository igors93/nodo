// P1 tests: Official network classification verified through NetworkProfileRegistry.
#include "config/NetworkProfileRegistry.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::config::NetworkProfileRegistry;

// Test 28: testnet-candidate is an official network (demo command blocked).
void testTestnetCandidateIsOfficial() {
    assert(NetworkProfileRegistry::isOfficialNetwork("testnet-candidate"));
}

// Test 29: localnet is NOT an official network (all commands allowed).
void testLocalnetIsNotOfficial() {
    assert(!NetworkProfileRegistry::isOfficialNetwork("localnet"));
}

// Test 30: mainnet is an official network.
void testMainnetIsOfficial() {
    assert(NetworkProfileRegistry::isOfficialNetwork("mainnet"));
}

} // namespace

int main() {
    testTestnetCandidateIsOfficial();
    testLocalnetIsNotOfficial();
    testMainnetIsOfficial();
    return 0;
}
