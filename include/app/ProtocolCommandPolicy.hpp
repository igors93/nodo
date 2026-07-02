#ifndef NODO_APP_PROTOCOL_COMMAND_POLICY_HPP
#define NODO_APP_PROTOCOL_COMMAND_POLICY_HPP

#include <string>

namespace nodo::app {

class ProtocolCommandPolicy {
public:
  // Returns true if legacy/development commands are blocked on the given
  // network. Official networks enforce blocking; localnet does not.
  static bool legacyCommandBlockingEnforced(const std::string &networkName);
};

} // namespace nodo::app

#endif
