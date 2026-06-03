#include "app/CommandLineInterface.hpp"
#include "app/ProtocolCommandPolicy.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::app::CommandLineInterface;
using nodo::app::CommandLineResult;
using nodo::app::CommandLineStatus;
using nodo::app::ProtocolCommandPolicy;

// CLI dispatcher rejects the demo command on an official network.
void testCLIRejectsDemoCommandOnOfficialNetwork() {
    const auto result = CommandLineInterface::execute({
        "demo",
        "--network", "testnet-candidate"
    });
    assert(!result.success());
    assert(result.status() == CommandLineStatus::COMMAND_FAILED);
    assert(!result.message().empty());
}

// CLI dispatcher rejects the demo command on mainnet.
void testCLIRejectsDemoCommandOnMainnet() {
    const auto result = CommandLineInterface::execute({
        "demo",
        "--network", "mainnet"
    });
    assert(!result.success());
    assert(result.status() == CommandLineStatus::COMMAND_FAILED);
}

// Blocked command handler is not reached when policy blocks it.
// The policy block message contains the network name; it does not mention
// data directory or runtime (which would indicate the handler ran).
void testBlockedCommandNotReached() {
    const auto result = CommandLineInterface::execute({
        "demo",
        "--network", "testnet-candidate"
    });
    assert(!result.success());
    assert(result.message().find("testnet-candidate") != std::string::npos ||
           result.message().find("demo") != std::string::npos);
    assert(result.message().find("data directory") == std::string::npos);
    assert(result.message().find("Cannot load runtime") == std::string::npos);
}

// Policy allows canonical commands on all networks.
void testCanonicalCommandsAllowedOnOfficialNetwork() {
    assert(ProtocolCommandPolicy::isCommandAllowed("block produce", "testnet-candidate"));
    assert(ProtocolCommandPolicy::isCommandAllowed("tx submit", "testnet-candidate"));
    assert(ProtocolCommandPolicy::isCommandAllowed("node reload", "testnet-candidate"));
}

// Policy allows demo on localnet; blocks it on official networks.
void testDemoBlockingPolicyConsistency() {
    assert(ProtocolCommandPolicy::isCommandAllowed("demo", "localnet"));
    assert(!ProtocolCommandPolicy::isCommandAllowed("demo", "testnet-candidate"));
    assert(ProtocolCommandPolicy::legacyCommandBlockingEnforced("testnet-candidate"));
    assert(!ProtocolCommandPolicy::legacyCommandBlockingEnforced("localnet"));
}

} // namespace

int main() {
    testCLIRejectsDemoCommandOnOfficialNetwork();
    testCLIRejectsDemoCommandOnMainnet();
    testBlockedCommandNotReached();
    testCanonicalCommandsAllowedOnOfficialNetwork();
    testDemoBlockingPolicyConsistency();
    return 0;
}
