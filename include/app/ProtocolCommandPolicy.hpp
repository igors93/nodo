#ifndef NODO_APP_PROTOCOL_COMMAND_POLICY_HPP
#define NODO_APP_PROTOCOL_COMMAND_POLICY_HPP

#include <string>

namespace nodo::app {

/*
 * ProtocolCommandPolicy is the single authority that decides whether a CLI
 * command may execute on a given network.
 *
 * Security principle:
 * The demo command does not produce protocol-valid state and must be blocked
 * on official networks at the policy level so the check is never scattered
 * across individual call sites.
 *
 * Both the CLI dispatcher and ReadinessContextBuilder consult this policy so
 * that readiness reflects the same enforcement that the dispatcher applies.
 */
class ProtocolCommandPolicy {
public:
    // Returns true if the command is allowed to execute on the given network.
    static bool isCommandAllowed(
        const std::string& command,
        const std::string& networkName
    );

    // Returns an empty string if allowed; returns the blocking reason otherwise.
    static std::string blockingReason(
        const std::string& command,
        const std::string& networkName
    );

    // Returns true if demo command blocking is enforced for the given network.
    // Official networks enforce blocking; localnet does not.
    static bool legacyCommandBlockingEnforced(
        const std::string& networkName
    );
};

} // namespace nodo::app

#endif
