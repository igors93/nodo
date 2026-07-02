#include "app/CommandLineInterface.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::app::CommandLineInterface;
using nodo::app::CommandLineOptions;
using nodo::app::CommandLineStatus;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testNodeRunParsedAsCommand() {
    const CommandLineOptions opts =
        CommandLineInterface::parse({"node", "run"});

    requireCondition(
        opts.command == "node run",
        "Command should be 'node run'."
    );
}

void testListenOptionSetsEndpoint() {
    const CommandLineOptions opts =
        CommandLineInterface::parse({
            "node", "run",
            "--listen", "127.0.0.1:9001"
        });

    requireCondition(
        opts.listenAddress == "127.0.0.1:9001",
        "--listen should populate listenAddress."
    );
    requireCondition(
        opts.endpoint == "127.0.0.1:9001",
        "--listen should also propagate to endpoint."
    );
}

void testRpcListenOptionIsIndependentFromP2pEndpoint() {
    const CommandLineOptions opts =
        CommandLineInterface::parse({
            "node", "run",
            "--listen", "127.0.0.1:9001",
            "--rpc-listen", "127.0.0.1:8601"
        });

    requireCondition(
        opts.endpoint == "127.0.0.1:9001",
        "--rpc-listen must not change the P2P endpoint."
    );
    requireCondition(
        opts.rpcBindAddress == "127.0.0.1" && opts.rpcPort == 8601,
        "--rpc-listen should populate the RPC bind host and port."
    );
}

void testRpcListenDefaultsAndValidation() {
    const CommandLineOptions defaults =
        CommandLineInterface::parse({"node", "run"});
    requireCondition(
        defaults.rpcBindAddress == "127.0.0.1" &&
            defaults.rpcPort == 8545,
        "node run should preserve the existing default RPC endpoint."
    );

    bool rejected = false;
    try {
        (void)CommandLineInterface::parse({
            "node", "run", "--rpc-listen", "127.0.0.1:70000"
        });
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    requireCondition(rejected, "Out-of-range RPC ports must be rejected.");
}

void testPeerOptionAccumulates() {
    const CommandLineOptions opts =
        CommandLineInterface::parse({
            "node", "run",
            "--peer", "validator2@127.0.0.1:9002",
            "--peer", "validator3@127.0.0.1:9003"
        });

    requireCondition(
        opts.peers.size() == 2,
        "Two --peer flags should produce 2 peer entries."
    );
    requireCondition(
        opts.peers[0] == "validator2@127.0.0.1:9002",
        "First peer should be validator2."
    );
    requireCondition(
        opts.peers[1] == "validator3@127.0.0.1:9003",
        "Second peer should be validator3."
    );
}

void testValidatorKeyOption() {
    const CommandLineOptions opts =
        CommandLineInterface::parse({
            "node", "run",
            "--validator-key", "my-validator-key"
        });

    requireCondition(
        opts.keyId == "my-validator-key",
        "--validator-key should set keyId."
    );
    requireCondition(
        opts.keyIdProvided,
        "--validator-key should set keyIdProvided."
    );
}

void testIdentityKeyOption() {
    const CommandLineOptions opts =
        CommandLineInterface::parse({
            "node", "run", "--identity-key", "peer-identity-a"
        });
    requireCondition(
        opts.identityKeyId == "peer-identity-a",
        "--identity-key should select the Ed25519 peer identity key."
    );
}

void testNetworkOption() {
    const CommandLineOptions opts =
        CommandLineInterface::parse({
            "node", "run",
            "--network", "testnet-candidate"
        });

    requireCondition(
        opts.networkName == "testnet-candidate",
        "--network should set networkName."
    );
}

void testFullNodeRunCommandLine() {
    const CommandLineOptions opts =
        CommandLineInterface::parse({
            "node", "run",
            "--network", "testnet-candidate",
            "--data-dir", ".nodo",
            "--listen", "127.0.0.1:9001",
            "--rpc-listen", "127.0.0.1:8601",
            "--identity-key", "peer-identity-a",
            "--peer", "validator2@127.0.0.1:9002"
        });

    requireCondition(opts.command == "node run",
        "Full command should parse as 'node run'.");
    requireCondition(opts.networkName == "testnet-candidate",
        "networkName should be 'testnet-candidate'.");
    requireCondition(opts.listenAddress == "127.0.0.1:9001",
        "listenAddress should be '127.0.0.1:9001'.");
    requireCondition(opts.rpcPort == 8601,
        "rpcPort should be 8601.");
    requireCondition(opts.identityKeyId == "peer-identity-a",
        "identityKeyId should match the explicit key.");
    requireCondition(opts.peers.size() == 1,
        "Should have 1 peer entry.");
    requireCondition(opts.peers[0] == "validator2@127.0.0.1:9002",
        "Peer entry should match.");
}

void testNoPeersDefault() {
    const CommandLineOptions opts =
        CommandLineInterface::parse({"node", "run"});

    requireCondition(
        opts.peers.empty(),
        "Default peers list should be empty."
    );
}

void testHelpTextContainsNodeRun() {
    const std::string help = CommandLineInterface::helpText();

    requireCondition(
        help.find("node run") != std::string::npos,
        "Help text should mention 'node run'."
    );
    requireCondition(
        help.find("--listen") != std::string::npos,
        "Help text should mention '--listen'."
    );
    requireCondition(
        help.find("--rpc-listen") != std::string::npos,
        "Help text should mention '--rpc-listen'."
    );
    requireCondition(
        help.find("--peer") != std::string::npos,
        "Help text should mention '--peer'."
    );
    requireCondition(
        help.find("--validator-key") != std::string::npos,
        "Help text should mention '--validator-key'."
    );
    requireCondition(
        help.find("--identity-key") != std::string::npos,
        "Help text should mention '--identity-key'."
    );
}

void testUnknownNetworkRejectsNodeRun() {
    const auto result = CommandLineInterface::execute({
        "node", "run",
        "--network", "unknown-network-xyz"
    });

    requireCondition(
        !result.success(),
        "node run with unknown network should fail."
    );
}

} // namespace

int main() {
    try {
        testNodeRunParsedAsCommand();
        testListenOptionSetsEndpoint();
        testRpcListenOptionIsIndependentFromP2pEndpoint();
        testRpcListenDefaultsAndValidation();
        testPeerOptionAccumulates();
        testValidatorKeyOption();
        testIdentityKeyOption();
        testNetworkOption();
        testFullNodeRunCommandLine();
        testNoPeersDefault();
        testHelpTextContainsNodeRun();
        testUnknownNetworkRejectsNodeRun();

        std::cout << "CommandLineNodeRun tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CommandLineNodeRun tests failed: " << error.what() << "\n";
        return 1;
    }
}
