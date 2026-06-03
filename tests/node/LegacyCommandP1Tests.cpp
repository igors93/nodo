// P1 tests: Legacy command blocking behavior verified through config NetworkProfileRegistry.
#include "config/NetworkProfileRegistry.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::config::NetworkProfileRegistry;

// Test 28: testnet-candidate is an official network (legacy commands blocked).
void testTestnetCandidateIsOfficial() {
    assert(NetworkProfileRegistry::isOfficialNetwork("testnet-candidate"));
}

// Test 29: localnet is NOT an official network (legacy commands allowed as redirects).
void testLocalnetIsNotOfficial() {
    assert(!NetworkProfileRegistry::isOfficialNetwork("localnet"));
}

// Test 30: mainnet is blocked (expected behavior — mainnet not reachable).
void testMainnetIsBlocked() {
    assert(NetworkProfileRegistry::isOfficialNetwork("mainnet"));
}

} // namespace

int main() {
    testTestnetCandidateIsOfficial();
    testLocalnetIsNotOfficial();
    testMainnetIsBlocked();
    return 0;
}
