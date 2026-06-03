#include "app/ProtocolCommandPolicy.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::app::ProtocolCommandPolicy;

// Official network rejects the demo command.
void testOfficialNetworkRejectsDemoCommand() {
    assert(!ProtocolCommandPolicy::isCommandAllowed("demo", "testnet-candidate"));
    assert(!ProtocolCommandPolicy::isCommandAllowed("demo", "mainnet"));
}

// Official network allows canonical commands.
void testOfficialNetworkAllowsCanonicalCommands() {
    assert(ProtocolCommandPolicy::isCommandAllowed("block produce", "testnet-candidate"));
    assert(ProtocolCommandPolicy::isCommandAllowed("tx submit", "testnet-candidate"));
    assert(ProtocolCommandPolicy::isCommandAllowed("chain audit", "testnet-candidate"));
    assert(ProtocolCommandPolicy::isCommandAllowed("node reload", "testnet-candidate"));
}

// Local development network allows all commands including demo.
void testLocalnetAllowsAllCommands() {
    assert(ProtocolCommandPolicy::isCommandAllowed("demo", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("block produce", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("tx submit", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("chain audit", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("testnet readiness", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("node reload", "localnet"));
}

// Blocking reason is non-empty for blocked demo command.
void testBlockingReasonNonEmptyForDemo() {
    const std::string reason =
        ProtocolCommandPolicy::blockingReason("demo", "testnet-candidate");
    assert(!reason.empty());
    assert(reason.find("demo") != std::string::npos);
    assert(reason.find("testnet-candidate") != std::string::npos);
}

// Blocking reason is empty when command is allowed.
void testBlockingReasonEmptyWhenAllowed() {
    assert(ProtocolCommandPolicy::blockingReason("chain audit", "testnet-candidate").empty());
    assert(ProtocolCommandPolicy::blockingReason("demo", "localnet").empty());
    assert(ProtocolCommandPolicy::blockingReason("block produce", "testnet-candidate").empty());
}

// legacyCommandBlockingEnforced reflects official vs non-official network.
void testBlockingEnforcedForOfficialNetworks() {
    assert(ProtocolCommandPolicy::legacyCommandBlockingEnforced("testnet-candidate"));
    assert(!ProtocolCommandPolicy::legacyCommandBlockingEnforced("localnet"));
}

} // namespace

int main() {
    testOfficialNetworkRejectsDemoCommand();
    testOfficialNetworkAllowsCanonicalCommands();
    testLocalnetAllowsAllCommands();
    testBlockingReasonNonEmptyForDemo();
    testBlockingReasonEmptyWhenAllowed();
    testBlockingEnforcedForOfficialNetworks();
    return 0;
}
