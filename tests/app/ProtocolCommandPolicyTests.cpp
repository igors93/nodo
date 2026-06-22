#include "app/ProtocolCommandPolicy.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::app::ProtocolCommandPolicy;

void testBlockingEnforcedForOfficialNetworks() {
    assert(ProtocolCommandPolicy::legacyCommandBlockingEnforced("testnet-candidate"));
    assert(ProtocolCommandPolicy::legacyCommandBlockingEnforced("mainnet"));
    assert(!ProtocolCommandPolicy::legacyCommandBlockingEnforced("localnet"));
}

} // namespace

int main() {
    testBlockingEnforcedForOfficialNetworks();
    return 0;
}
