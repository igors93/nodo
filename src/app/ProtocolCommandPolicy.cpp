#include "app/ProtocolCommandPolicy.hpp"

#include "config/NetworkProfileRegistry.hpp"

namespace nodo::app {

bool ProtocolCommandPolicy::legacyCommandBlockingEnforced(
    const std::string& networkName
) {
    return config::NetworkProfileRegistry::isOfficialNetwork(networkName);
}

} // namespace nodo::app
