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

// Test 1: CLI dispatcher rejects blocked command through the shared policy.
void testCLIRejectsLegacyCommandOnOfficialNetwork() {
    // produce-demo-block on testnet-candidate must be blocked by the policy.
    const auto result = CommandLineInterface::execute({
        "produce-demo-block",
        "--network", "testnet-candidate"
    });
    assert(!result.success());
    assert(result.status() == CommandLineStatus::COMMAND_FAILED);
    assert(!result.message().empty());
}

// Test 1: submit-demo-transaction on testnet-candidate is blocked.
void testCLIRejectsSubmitDemoOnOfficialNetwork() {
    const auto result = CommandLineInterface::execute({
        "submit-demo-transaction",
        "--network", "testnet-candidate"
    });
    assert(!result.success());
    assert(result.status() == CommandLineStatus::COMMAND_FAILED);
}

// Test 1: demo command on testnet-candidate is blocked.
void testCLIRejectsDemoCommandOnOfficialNetwork() {
    const auto result = CommandLineInterface::execute({
        "demo",
        "--network", "testnet-candidate"
    });
    assert(!result.success());
    assert(result.status() == CommandLineStatus::COMMAND_FAILED);
}

// Test 1: reload alias on testnet-candidate is blocked.
void testCLIRejectsReloadAliasOnOfficialNetwork() {
    const auto result = CommandLineInterface::execute({
        "reload",
        "--network", "testnet-candidate"
    });
    assert(!result.success());
    assert(result.status() == CommandLineStatus::COMMAND_FAILED);
}

// Test 2: Blocked command handler is not reached when policy blocks it.
// If the handler were reached it would need a data directory and would fail
// differently (COMMAND_FAILED with a specific I/O error message).
// The policy block message contains the command name.
void testBlockedCommandNotReached() {
    const auto result = CommandLineInterface::execute({
        "produce-demo-block",
        "--network", "testnet-candidate"
    });
    assert(!result.success());
    // The policy block message mentions the command.
    assert(result.message().find("produce-demo-block") != std::string::npos ||
           result.message().find("testnet-candidate") != std::string::npos);
    // If the handler were reached it would say something about a data directory.
    // The policy block must not mention "data directory" or "runtime".
    assert(result.message().find("data directory") == std::string::npos);
    assert(result.message().find("Cannot load runtime") == std::string::npos);
}

// Test 3: Local development alias calls canonical handler on localnet.
// On localnet produce-demo-block and block produce share the same handler
// (both call executeProduceDemoBlock). The policy must not block it on localnet.
void testLocalnetAliasNotBlocked() {
    // Policy must allow produce-demo-block on localnet.
    assert(ProtocolCommandPolicy::isCommandAllowed("produce-demo-block", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("submit-demo-transaction", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("block produce", "localnet"));
    assert(ProtocolCommandPolicy::isCommandAllowed("tx submit", "localnet"));
}

// Test 4: Readiness command safety uses the same policy as the dispatcher.
void testReadinessCommandPolicyAlignedWithDispatcher() {
    // If a command is blocked on an official network, the policy says so
    // consistently for both readiness and the dispatcher.
    const bool blockProduceDemoBlock =
        !ProtocolCommandPolicy::isCommandAllowed("produce-demo-block", "testnet-candidate");
    assert(blockProduceDemoBlock);

    const bool blockSubmitDemo =
        !ProtocolCommandPolicy::isCommandAllowed("submit-demo-transaction", "testnet-candidate");
    assert(blockSubmitDemo);

    // The legacyCommandBlockingEnforced flag is true for official networks.
    assert(ProtocolCommandPolicy::legacyCommandBlockingEnforced("testnet-candidate"));
    assert(!ProtocolCommandPolicy::legacyCommandBlockingEnforced("localnet"));
}

} // namespace

int main() {
    testCLIRejectsLegacyCommandOnOfficialNetwork();
    testCLIRejectsSubmitDemoOnOfficialNetwork();
    testCLIRejectsDemoCommandOnOfficialNetwork();
    testCLIRejectsReloadAliasOnOfficialNetwork();
    testBlockedCommandNotReached();
    testLocalnetAliasNotBlocked();
    testReadinessCommandPolicyAlignedWithDispatcher();
    return 0;
}
