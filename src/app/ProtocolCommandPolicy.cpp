#include "app/ProtocolCommandPolicy.hpp"

#include "config/NetworkProfileRegistry.hpp"

namespace nodo::app {

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

    if (command == "demo") {
        return "Command 'demo' is not permitted on official network '" +
               networkName + "'. The demo command is localnet-only and does not produce "
               "protocol-valid state. Use the canonical protocol commands.";
    }

    return "";
}

bool ProtocolCommandPolicy::legacyCommandBlockingEnforced(
    const std::string& networkName
) {
    return config::NetworkProfileRegistry::isOfficialNetwork(networkName);
}

} // namespace nodo::app
