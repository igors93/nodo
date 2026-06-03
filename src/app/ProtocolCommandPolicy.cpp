#include "app/ProtocolCommandPolicy.hpp"

#include "config/NetworkProfileRegistry.hpp"

namespace nodo::app {

namespace {

bool isLegacyOrDemoCommand(const std::string& command) {
    return command == "demo" ||
           command == "produce-demo-block" ||
           command == "submit-demo-transaction" ||
           command == "reload";
}

} // namespace

bool ProtocolCommandPolicy::isCommandAllowed(
    const std::string& command,
    const std::string& networkName
) {
    return blockingReason(command, networkName).empty();
}

std::string ProtocolCommandPolicy::blockingReason(
    const std::string& command,
    const std::string& networkName
) {
    if (!config::NetworkProfileRegistry::isOfficialNetwork(networkName)) {
        return "";
    }

    if (isLegacyOrDemoCommand(command)) {
        return "Command '" + command + "' is not permitted on official network '" +
               networkName + "'. Legacy and demo commands are blocked on official networks. "
               "Use the canonical protocol commands.";
    }

    return "";
}

bool ProtocolCommandPolicy::legacyCommandBlockingEnforced(
    const std::string& networkName
) {
    // Official networks always enforce blocking via this policy.
    // Non-official networks (localnet) redirect legacy commands to canonical handlers.
    return config::NetworkProfileRegistry::isOfficialNetwork(networkName);
}

} // namespace nodo::app
