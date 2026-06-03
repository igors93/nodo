#include "app/ProtocolCommandPolicy.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::app::ProtocolCommandPolicy;

// Test 7 (item 3): Official network rejects unsafe legacy commands.
void testOfficialNetworkRejectsLegacyCommands() {
    assert(!ProtocolCommandPolicy::isCommandAllowed("produce-demo-block", "testnet-candidate"));
    assert(!ProtocolCommandPolicy::isCommandAllowed("submit-demo-transaction", "testnet-candidate"));
    assert(!ProtocolCommandPolicy::isCommandAllowed("demo", "testnet-candidate"));
    assert(!ProtocolCommandPolicy::isCommandAllowed("reload", "testnet-candidate"));
}

// Test 8 (item 3): Local development network allows canonical handlers.
void testLocalnetAllowsCanonicalCommands() {
    assert(ProtocolCommandPolicy::isCommandAllowed("block produce", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("tx submit", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("chain audit", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("testnet readiness", "localnet"));
}

// Test 8: Local development aliases call canonical handlers on localnet (not blocked).
void testLocalnetAllowsLegacyAliasesAsRedirects() {
    // On localnet, legacy aliases are allowed (they redirect to canonical paths).
    assert(ProtocolCommandPolicy::isCommandAllowed("produce-demo-block", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("submit-demo-transaction", "localnet"));
}

// Blocking reason is non-empty for blocked commands.
void testBlockingReasonNonEmptyWhenBlocked() {
    const std::string reason =
        ProtocolCommandPolicy::blockingReason("produce-demo-block", "testnet-candidate");
    assert(!reason.empty());
    assert(reason.find("produce-demo-block") != std::string::npos);
    assert(reason.find("testnet-candidate") != std::string::npos);
}

// Blocking reason is empty when not blocked.
void testBlockingReasonEmptyWhenAllowed() {
    assert(ProtocolCommandPolicy::blockingReason("chain audit", "testnet-candidate").empty());
    assert(ProtocolCommandPolicy::blockingReason("produce-demo-block", "localnet").empty());
}

// legacyCommandBlockingEnforced for official vs non-official.
void testLegacyBlockingEnforcedForOfficialNetworks() {
    assert(ProtocolCommandPolicy::legacyCommandBlockingEnforced("testnet-candidate"));
    assert(!ProtocolCommandPolicy::legacyCommandBlockingEnforced("localnet"));
}

} // namespace

int main() {
    testOfficialNetworkRejectsLegacyCommands();
    testLocalnetAllowsCanonicalCommands();
    testLocalnetAllowsLegacyAliasesAsRedirects();
    testBlockingReasonNonEmptyWhenBlocked();
    testBlockingReasonEmptyWhenAllowed();
    testLegacyBlockingEnforcedForOfficialNetworks();
    return 0;
}
