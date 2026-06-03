#ifndef NODO_APP_PROTOCOL_COMMAND_POLICY_HPP
#define NODO_APP_PROTOCOL_COMMAND_POLICY_HPP

#include <string>

namespace nodo::app {

/*
 * ProtocolCommandPolicy is the single authority that decides whether a CLI
 * command may execute on a given network.
 *
 * Security principle:
 * Commands that produce transactions, blocks, state, keys, treasury effects,
 * or runtime safety state changes must be approved by this policy before
 * execution. Legacy and demo commands that bypass the official runtime pipeline
 * must be blocked on official networks at the policy level, not at the
 * individual call site.
 *
 * Both the CLI dispatcher and ReadinessContextBuilder consult this policy so
 * that readiness reflects the same enforcement that the dispatcher applies.
 */
class ProtocolCommandPolicy {
public:
    // Returns true if the command is allowed to execute on the given network.
    // Blocks demo and legacy alias commands on official networks.
    static bool isCommandAllowed(
        const std::string& command,
        const std::string& networkName
    );

    // Returns an empty string if allowed; returns the blocking reason otherwise.
    static std::string blockingReason(
        const std::string& command,
        const std::string& networkName
    );

    // Returns true if legacy command blocking is enforced for the given network.
    // Official networks always enforce blocking; localnet does not require it.
    static bool legacyCommandBlockingEnforced(
        const std::string& networkName
    );
};

} // namespace nodo::app

#endif
