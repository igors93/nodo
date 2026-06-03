#include "app/CommandLineInterface.hpp"

#include "app/DemoScenario.hpp"
#include "app/ProtocolCommandPolicy.hpp"
#include "config/GenesisRegistry.hpp"
#include "config/NetworkProfileRegistry.hpp"
#include "node/RuntimeStartupService.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signer.hpp"
#include "crypto/SignatureBundle.hpp"
#include "economics/EpochTreasuryReport.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "node/ChainAuditor.hpp"
#include "node/ChainAuditResult.hpp"
#include "node/EpochTreasuryReportStore.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/FinalizedTreasuryAudit.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/OperatorDiagnostics.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/ProductionKeySafetyGate.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeMonetaryReportService.hpp"
#include "node/ReadinessContext.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "node/TestnetReadinessChecker.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "utils/Amount.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace nodo::app {

namespace {

std::int64_t nowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string defaultLocalnetUserKeyId() {
    return "local-user";
}

std::string defaultLocalnetUserKeySeed() {
    return "nodo-localnet-user-key-v1";
}

bool isOption(
    const std::string& value
) {
    return value.rfind("--", 0) == 0;
}

bool isCommandGroup(
    const std::string& value
) {
    return value == "tx" ||
           value == "block" ||
           value == "node" ||
           value == "chain" ||
           value == "keys" ||
           value == "validator" ||
           value == "testnet";
}

config::NetworkParameters networkParametersForOptions(
    const CommandLineOptions& options
) {
    return config::NetworkProfileRegistry::get(options.networkName);
}

CommandLineResult validateSelectedNetwork(
    const CommandLineOptions& options
) {
    if (!config::NetworkProfileRegistry::isKnown(options.networkName)) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Unknown network profile: " + options.networkName + "\n"
        );
    }

    const config::NetworkParameters params =
        networkParametersForOptions(options);

    const node::StartupValidationResult profileCheck =
        node::RuntimeStartupService::validateNetworkProfile(params);

    if (!profileCheck.valid()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            profileCheck.reason() + "\n"
        );
    }

    return CommandLineResult::success("");
}

std::optional<std::string> manifestNetworkMismatch(
    const node::NodeRuntimeManifest& manifest,
    const CommandLineOptions& options
) {
    const config::NetworkParameters selected =
        networkParametersForOptions(options);

    const node::StartupValidationResult compatCheck =
        node::RuntimeStartupService::validateDataDirectoryCompatibility(
            manifest, selected
        );

    if (!compatCheck.valid()) {
        return compatCheck.reason();
    }

    return std::nullopt;
}

} // namespace

CommandLineOptions::CommandLineOptions()
    : command("help"),
      dataDirectory(".nodo"),
      networkName("localnet"),
      peerId("local-node"),
      endpoint("127.0.0.1:9000"),
      keyId("local-validator"),
      keyType("both"),
      toAddress("nodo-localnet-recipient"),
      amountRaw(1000),
      feeRaw(100),
      nonce(0),
      timestamp(nowUnixSeconds()),
      showHelp(false),
      keyIdProvided(false) {}

std::string commandLineStatusToString(
    CommandLineStatus status
) {
    switch (status) {
        case CommandLineStatus::SUCCESS:
            return "SUCCESS";
        case CommandLineStatus::INVALID_ARGUMENTS:
            return "INVALID_ARGUMENTS";
        case CommandLineStatus::COMMAND_FAILED:
            return "COMMAND_FAILED";
        default:
            return "COMMAND_FAILED";
    }
}

CommandLineResult::CommandLineResult()
    : m_status(CommandLineStatus::COMMAND_FAILED),
      m_message("Uninitialized command result.") {}

CommandLineResult CommandLineResult::success(
    std::string message
) {
    CommandLineResult result;
    result.m_status = CommandLineStatus::SUCCESS;
    result.m_message = std::move(message);
    return result;
}

CommandLineResult CommandLineResult::failure(
    CommandLineStatus status,
    std::string message
) {
    CommandLineResult result;
    result.m_status = status;
    result.m_message = std::move(message);
    return result;
}

CommandLineStatus CommandLineResult::status() const {
    return m_status;
}

const std::string& CommandLineResult::message() const {
    return m_message;
}

bool CommandLineResult::success() const {
    return m_status == CommandLineStatus::SUCCESS;
}

int CommandLineInterface::run(
    int argc,
    char** argv
) {
    std::vector<std::string> args;

    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }

    const CommandLineResult result =
        execute(args);

    if (result.success()) {
        std::cout << result.message();
        return 0;
    }

    std::cerr << result.message();
    return 1;
}

CommandLineResult CommandLineInterface::execute(
    const std::vector<std::string>& args
) {
    try {
        const CommandLineOptions options =
            parse(args);

        if (options.showHelp ||
            options.command == "help") {
            return CommandLineResult::success(
                helpText()
            );
        }

        // Central command safety policy check. Must happen before any execution,
        // including the demo command, so that official networks are always rejected
        // through the shared policy rather than per-command ad hoc logic.
        {
            const std::string policyBlock =
                ProtocolCommandPolicy::blockingReason(
                    options.command,
                    options.networkName
                );
            if (!policyBlock.empty()) {
                return CommandLineResult::failure(
                    CommandLineStatus::COMMAND_FAILED,
                    policyBlock + "\n"
                );
            }
        }

        if (options.command == "demo") {
            const int demoStatus =
                runBlockchainFoundationDemo();

            if (demoStatus == 0) {
                return CommandLineResult::success(
                    "Nodo demo completed successfully (localnet educational mode only).\n"
                );
            }

            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                "Nodo demo failed.\n"
            );
        }

        const CommandLineResult networkValidation =
            validateSelectedNetwork(options);

        if (!networkValidation.success()) {
            return networkValidation;
        }

        if (options.command == "init") {
            return executeInit(options);
        }

        if (options.command == "status") {
            return executeStatus(options);
        }

        if (options.command == "inspect") {
            return executeInspect(options);
        }

        if (options.command == "node reload") {
            return executeReload(options);
        }

        if (options.command == "chain audit") {
            return executeChainAudit(options);
        }

        if (options.command == "testnet readiness") {
            return executeTestnetReadiness(options);
        }

        if (options.command == "diagnostics") {
            return executeDiagnostics(options);
        }

        if (options.command == "block produce") {
            return executeProduceBlock(options);
        }

        if (options.command == "tx submit") {
            return executeSubmitTransaction(options);
        }

        if (options.command == "keys create") {
            return executeKeysCreate(options);
        }

        if (options.command == "keys list") {
            return executeKeysList(options);
        }

        if (options.command == "validator list") {
            return executeValidatorList(options);
        }

        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Unknown command: " + options.command + "\n\n" + helpText()
        );
    } catch (const std::exception& error) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            std::string("Invalid command line: ") + error.what() + "\n\n" + helpText()
        );
    }
}

CommandLineOptions CommandLineInterface::parse(
    const std::vector<std::string>& args
) {
    CommandLineOptions options;

    std::size_t index = 0;

    if (!args.empty() &&
        !isOption(args.front())) {
        options.command = args.front();
        index = 1;

        if (isCommandGroup(options.command) &&
            index < args.size() &&
            !isOption(args[index])) {
            options.command += " " + args[index];
            ++index;
        }
    }

    while (index < args.size()) {
        const std::string& option =
            args[index];

        if (option == "--help" ||
            option == "-h") {
            options.showHelp = true;
            ++index;
            continue;
        }

        if (option == "--data-dir") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--data-dir requires a value.");
            }

            options.dataDirectory = args[index + 1];
            index += 2;
            continue;
        }

        if (option == "--network") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--network requires a value.");
            }

            options.networkName = args[index + 1];
            index += 2;
            continue;
        }

        if (option == "--peer-id") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--peer-id requires a value.");
            }

            options.peerId = args[index + 1];
            index += 2;
            continue;
        }

        if (option == "--endpoint") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--endpoint requires a value.");
            }

            options.endpoint = args[index + 1];
            index += 2;
            continue;
        }

        if (option == "--from" ||
            option == "--key-id") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument(option + " requires a value.");
            }

            options.keyId = args[index + 1];
            options.keyIdProvided = true;
            index += 2;
            continue;
        }

        if (option == "--type") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--type requires a value.");
            }

            options.keyType = args[index + 1];

            if (options.keyType != "user" &&
                options.keyType != "validator" &&
                options.keyType != "both") {
                throw std::invalid_argument("--type must be user, validator, or both.");
            }

            index += 2;
            continue;
        }

        if (option == "--to") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--to requires a value.");
            }

            options.toAddress = args[index + 1];
            index += 2;
            continue;
        }

        if (option == "--amount") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--amount requires a value.");
            }

            options.amountRaw =
                std::stoll(args[index + 1]);
            index += 2;
            continue;
        }

        if (option == "--fee") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--fee requires a value.");
            }

            options.feeRaw =
                std::stoll(args[index + 1]);
            index += 2;
            continue;
        }

        if (option == "--nonce") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--nonce requires a value.");
            }

            options.nonce =
                static_cast<std::uint64_t>(
                    std::stoull(args[index + 1])
                );
            index += 2;
            continue;
        }

        if (option == "--timestamp") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--timestamp requires a value.");
            }

            options.timestamp =
                static_cast<std::int64_t>(
                    std::stoll(args[index + 1])
                );

            index += 2;
            continue;
        }

        throw std::invalid_argument("Unknown option: " + option);
    }

    return options;
}

std::string CommandLineInterface::helpText() {
    return
        "Nodo command line\n"
        "-----------------\n"
        "\n"
        "Usage:\n"
        "  nodo help\n"
        "  nodo init [--network localnet|testnet-candidate] [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
        "  nodo status [--network localnet|testnet-candidate] [--data-dir PATH]\n"
        "  nodo inspect [--network localnet|testnet-candidate] [--data-dir PATH]\n"
        "  nodo node reload [--network localnet|testnet-candidate] [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
        "  nodo keys create [--network localnet|testnet-candidate] [--data-dir PATH] [--type user|validator|both] [--key-id ID]\n"
        "  nodo keys list [--data-dir PATH]\n"
        "  nodo tx submit [--data-dir PATH] [--from KEY_ID] [--to ADDRESS] [--amount RAW_UNITS] [--fee RAW_UNITS] [--nonce VALUE]\n"
        "  nodo block produce [--data-dir PATH]\n"
        "  nodo chain audit [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
        "  nodo validator list [--data-dir PATH]\n"
        "  nodo testnet readiness [--network localnet|testnet-candidate] [--data-dir PATH] [--key-id ID]\n"
        "  nodo diagnostics [--network localnet|testnet-candidate] [--data-dir PATH] [--key-id ID]\n"
        "\n"
        "Localnet-only commands (not valid on official networks):\n"
        "  nodo demo              Educational blockchain foundation demo. Does not produce protocol-valid state.\n"
        "\n"
        "Options:\n"
        "  --data-dir PATH      Node data directory. Default: .nodo\n"
        "  --network NAME       Network profile: localnet or testnet-candidate. mainnet is blocked.\n"
        "  --peer-id ID         Local peer id for init/load. Default: local-node\n"
        "  --endpoint HOST:PORT Local endpoint for init/load. Default: 127.0.0.1:9000\n"
        "  --key-id ID          Key id for keys create or signing. Defaults depend on command.\n"
        "  --type TYPE          Key type for keys create. Default: both\n"
        "  --from KEY_ID        Alias for --key-id in tx submit.\n"
        "  --to ADDRESS         Recipient address for tx submit.\n"
        "  --amount RAW_UNITS   Transfer amount for tx submit. Default: 1000\n"
        "  --fee RAW_UNITS      Transfer fee for tx submit. Default: 100\n"
        "  --nonce VALUE        Transaction nonce for tx submit. Default: next account nonce\n"
        "  --timestamp SECONDS  Deterministic timestamp override for tests.\n";
}

config::GenesisConfig CommandLineInterface::developmentGenesisConfig() {
    const config::GenesisLookupResult result =
        config::GenesisRegistry::get("localnet");
    return result.found() ? result.genesis() : config::GenesisConfig();
}

std::string CommandLineInterface::defaultLocalnetKeyId() {
    return "local-validator";
}

std::string CommandLineInterface::defaultLocalnetKeySeed() {
    return "nodo-localnet-validator-key-v1";
}

p2p::PeerInfo CommandLineInterface::localPeerFromOptions(
    const CommandLineOptions& options
) {
    const config::NetworkParameters params =
        networkParametersForOptions(options);

    return p2p::PeerInfo(
        options.peerId,
        options.endpoint,
        params.protocolVersion(),
        0,
        options.timestamp
    );
}

CommandLineResult CommandLineInterface::executeInit(
    const CommandLineOptions& options
) {
    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);

    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const config::GenesisConfig genesisConfig = genesisLookup.genesis();

    const node::NodeDataDirectoryInitResult result =
        node::NodeDataDirectory::initialize(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            genesisConfig,
            localPeerFromOptions(options),
            options.timestamp
        );

    if (!result.success()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to initialize Nodo data directory: "
            + result.reason()
            + "\n"
        );
    }

    std::ostringstream output;

    output << "Nodo data directory "
           << (result.initialized() ? "initialized" : "already initialized")
           << ".\n"
           << "Data directory: " << options.dataDirectory.string() << "\n"
           << "Network: " << result.manifest().networkName() << "\n"
           << "Chain id: " << result.manifest().chainId() << "\n"
           << "Genesis id: " << result.manifest().genesisConfigId() << "\n"
           << "Latest height: " << result.manifest().latestBlockHeight() << "\n"
           << "Latest state root: " << result.manifest().latestStateRoot() << "\n";

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeStatus(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryReadResult result =
        node::NodeDataDirectory::loadManifest(
            node::NodeDataDirectoryConfig(options.dataDirectory)
        );

    if (!result.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to read Nodo status: "
            + result.reason()
            + "\n"
        );
    }

    const node::NodeRuntimeManifest& manifest =
        result.manifest();

    const std::optional<std::string> mismatch =
        manifestNetworkMismatch(manifest, options);

    if (mismatch.has_value()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            *mismatch + "\n"
        );
    }

    std::ostringstream output;

    output << "Nodo status\n"
           << "-----------\n"
           << "Data directory: " << options.dataDirectory.string() << "\n"
           << "Chain id: " << manifest.chainId() << "\n"
           << "Network: " << manifest.networkName() << "\n"
           << "Protocol: " << manifest.protocolVersion() << "\n"
           << "Latest height: " << manifest.latestBlockHeight() << "\n"
           << "Latest hash: " << manifest.latestBlockHash() << "\n"
           << "Latest state root: " << manifest.latestStateRoot() << "\n"
           << "Validators: " << manifest.validatorCount() << "\n"
           << "Peers: " << manifest.peerCount() << "\n";

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeInspect(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryReadResult result =
        node::NodeDataDirectory::loadManifest(
            node::NodeDataDirectoryConfig(options.dataDirectory)
        );

    if (!result.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to inspect Nodo data directory: "
            + result.reason()
            + "\n"
        );
    }

    const std::optional<std::string> mismatch =
        manifestNetworkMismatch(result.manifest(), options);

    if (mismatch.has_value()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            *mismatch + "\n"
        );
    }

    std::ostringstream output;

    output << "Nodo inspect\n"
           << "------------\n"
           << "Data directory: " << options.dataDirectory.string() << "\n"
           << result.manifest().serialize() << "\n";

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeReload(
    const CommandLineOptions& options
) {
    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);

    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const config::GenesisConfig genesisConfig = genesisLookup.genesis();

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            genesisConfig,
            localPeerFromOptions(options)
        );

    if (!load.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to reload Nodo runtime: "
            + load.reason()
            + "\n"
        );
    }

    std::ostringstream output;

    output << "Nodo runtime reloaded.\n"
           << "Data directory: " << options.dataDirectory.string() << "\n"
           << "Latest height: " << load.manifest().latestBlockHeight() << "\n"
           << "Latest hash: " << load.manifest().latestBlockHash() << "\n"
           << "Latest state root: " << load.manifest().latestStateRoot() << "\n"
           << "Loaded finalized blocks: " << load.loadedBlockCount() << "\n"
           << "Loaded mempool transactions: " << load.loadedMempoolTransactionCount() << "\n";

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeChainAudit(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryConfig directoryConfig(
        options.dataDirectory
    );

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);

    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const config::GenesisConfig genesisConfig = genesisLookup.genesis();

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig,
            localPeerFromOptions(options)
        );

    const node::ChainAuditResult audit =
        node::ChainAuditor::auditLoadedRuntime(
            load,
            directoryConfig.epochMonetaryReportPath(),
            directoryConfig.epochTreasuryReportPath()
        );

    if (!audit.passed()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            audit.toHumanReadableString()
        );
    }

    std::ostringstream output;

    output << audit.toHumanReadableString()
           << "Data directory: " << directoryConfig.rootPath().string() << "\n"
           << "Genesis id: " << load.manifest().genesisConfigId() << "\n";

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeTestnetReadiness(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryConfig directoryConfig(
        options.dataDirectory
    );

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);

    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const config::GenesisConfig genesisConfig = genesisLookup.genesis();

    const config::NetworkParameters params =
        genesisConfig.networkParameters();

    const node::NodeDataDirectoryReadResult manifest =
        node::NodeDataDirectory::loadManifest(directoryConfig);

    if (manifest.loaded()) {
        const std::optional<std::string> mismatch =
            manifestNetworkMismatch(manifest.manifest(), options);

        if (mismatch.has_value()) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                *mismatch + "\n"
            );
        }
    }

    if (config::NetworkProfileRegistry::isOfficialNetwork(options.networkName) &&
        !options.keyIdProvided) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Official network readiness requires --key-id. "
            "Do not default to localnet development keys.\n"
        );
    }

    const std::string validatorKeyId =
        options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();

    const crypto::KeyStoreLoadResult key =
        crypto::KeyStore::loadKey(
            directoryConfig.keysDirectoryPath(),
            validatorKeyId
        );

    bool keyPolicyPassed = false;
    std::vector<std::string> warnings;

    if (key.loaded()) {
        const node::KeySafetyCheckResult keySafety =
            node::ProductionKeySafetyGate::check(
                key.metadata(),
                params.networkName()
            );
        keyPolicyPassed = keySafety.isApproved();

        if (!keyPolicyPassed) {
            warnings.push_back(keySafety.reason());
        }
    } else {
        warnings.push_back(
            "Validator key '" + validatorKeyId + "' is not loaded: " +
            key.reason()
        );
    }

    const bool genesisVerified = true; // resolveAndVerify() already verified

    const std::size_t connectedPeers =
        manifest.loaded() ? manifest.manifest().peerCount() : 0;

    const std::uint64_t finalizedHeight =
        manifest.loaded() ? manifest.manifest().latestBlockHeight() : 0;

    const std::size_t validatorCount =
        manifest.loaded()
            ? manifest.manifest().validatorCount()
            : genesisConfig.bootstrapValidators().size();

    // Attempt runtime load and chain audit to derive real chain safety facts.
    bool chainAuditPassed = false;
    bool treasuryReportVerified = false;
    if (manifest.loaded() && finalizedHeight > 0) {
        const node::RuntimeStateLoadResult rtLoad =
            node::RuntimeStateLoader::loadFromDataDirectory(
                directoryConfig,
                genesisConfig,
                localPeerFromOptions(options)
            );
        if (rtLoad.loaded()) {
            const node::ChainAuditResult auditResult =
                node::ChainAuditor::auditLoadedRuntime(
                    rtLoad,
                    directoryConfig.epochMonetaryReportPath(),
                    directoryConfig.epochTreasuryReportPath()
                );
            chainAuditPassed = auditResult.passed();
            treasuryReportVerified = auditResult.passed();
        }
    } else {
        // No finalized blocks yet: chain audit and treasury report are vacuously valid.
        chainAuditPassed = true;
        treasuryReportVerified = true;
    }

    // Build the readiness context from real runtime inspection.
    const crypto::StoredKeyMetadata keyMetadata =
        key.loaded() ? key.metadata() : crypto::StoredKeyMetadata();
    node::ReadinessContextBuilder ctxBuilder(
        directoryConfig, params, keyMetadata
    );
    ctxBuilder
        .withManifest(manifest)
        .withGenesisFacts(
            genesisVerified,
            config::GenesisRegistry::hasRegisteredGenesis(options.networkName),
            genesisConfig.deterministicId(),
            config::networkClassToString(params.networkClass())
        )
        .withChainAuditResult(chainAuditPassed, treasuryReportVerified)
        .withSafetyState()
        .withKeyPolicyResult(keyPolicyPassed);

    for (const auto& w : warnings) {
        ctxBuilder.addWarning(w);
    }

    const node::ReadinessContext readinessCtx = ctxBuilder.build();

    // Propagate any safety state warnings back to warnings list.
    for (const auto& w : readinessCtx.warnings) {
        bool alreadyPresent = false;
        for (const auto& existing : warnings) {
            if (existing == w) {
                alreadyPresent = true;
                break;
            }
        }
        if (!alreadyPresent) {
            warnings.push_back(w);
        }
    }

    std::vector<node::ReadinessDiagnostic> checks;

    if (key.loaded()) {
        const node::TestnetReadinessCheckerConfig readinessConfig(
            readinessCtx.connectedPeers,
            readinessCtx.genesisVerified,
            readinessCtx.finalizedHeight,
            readinessCtx.governanceLifecycleIntegrated,
            readinessCtx.defenseModeInactive,
            readinessCtx.legacyCommandsBlocked,
            readinessCtx.treasuryReportVerified,
            readinessCtx.evidenceCaptureHealthy,
            readinessCtx.chainAuditPassed,
            readinessCtx.genesisRegistered,
            readinessCtx.networkClass
        );
        checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
            params,
            key.metadata(),
            readinessConfig
        );
    } else {
        checks.emplace_back(
            "validator_key_loaded",
            false,
            "Validator key '" + validatorKeyId + "' is missing or unreadable."
        );
        checks.emplace_back(
            "network_parameters_valid",
            params.isValid(),
            params.isValid() ? "Network parameters are valid for " + params.networkName()
                             : "Network parameters are invalid."
        );
        checks.emplace_back(
            "genesis_verified",
            genesisVerified,
            genesisVerified ? "Genesis has been verified."
                            : "Genesis verification failed."
        );
        checks.emplace_back(
            "peers_connected",
            connectedPeers > 0,
            std::to_string(connectedPeers) + " peer(s) connected."
        );
    }

    const node::ReadinessStatus status =
        key.loaded() && keyPolicyPassed
            ? node::TestnetReadinessChecker::summarize(checks)
            : node::ReadinessStatus::NOT_READY;

    std::ostringstream output;

    output << "Nodo testnet readiness\n"
           << "----------------------\n"
           << "Network: " << params.networkName() << "\n"
           << "Chain id: " << params.chainId() << "\n"
           << "Protocol: " << params.protocolVersion() << "\n"
           << "Genesis verified: " << (readinessCtx.genesisVerified ? "yes" : "no") << "\n"
           << "Key policy passed: " << (readinessCtx.keyPolicyPassed ? "yes" : "no") << "\n"
           << "Defense mode: " << (readinessCtx.defenseModeInactive ? "INACTIVE" : "ACTIVE or UNKNOWN") << "\n"
           << "Peers: " << readinessCtx.connectedPeers << "\n"
           << "Validators: " << validatorCount << "\n"
           << "Finalized height: " << readinessCtx.finalizedHeight << "\n"
           << "Readiness: " << node::readinessStatusToString(status) << "\n";

    for (const node::ReadinessDiagnostic& check : checks) {
        output << check.serialize() << "\n";
    }

    for (const std::string& warning : warnings) {
        output << "NOT_READY: " << warning;
        if (warning.empty() || warning.back() != '\n') {
            output << "\n";
        }
    }

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeDiagnostics(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryConfig directoryConfig(
        options.dataDirectory
    );

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);

    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const config::GenesisConfig genesisConfig = genesisLookup.genesis();

    const config::NetworkParameters params =
        genesisConfig.networkParameters();

    const node::NodeDataDirectoryReadResult manifest =
        node::NodeDataDirectory::loadManifest(directoryConfig);

    if (manifest.loaded()) {
        const std::optional<std::string> mismatch =
            manifestNetworkMismatch(manifest.manifest(), options);

        if (mismatch.has_value()) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                *mismatch + "\n"
            );
        }
    }

    if (config::NetworkProfileRegistry::isOfficialNetwork(options.networkName) &&
        !options.keyIdProvided) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Official network diagnostics requires --key-id. "
            "Do not default to localnet development keys.\n"
        );
    }

    const std::string validatorKeyId =
        options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();

    const crypto::KeyStoreLoadResult key =
        crypto::KeyStore::loadKey(
            directoryConfig.keysDirectoryPath(),
            validatorKeyId
        );

    bool keyPolicyPassed = false;
    std::vector<std::string> warnings;

    if (key.loaded()) {
        const node::KeySafetyCheckResult keySafety =
            node::ProductionKeySafetyGate::check(
                key.metadata(),
                params.networkName()
            );
        keyPolicyPassed = keySafety.isApproved();
        if (!keyPolicyPassed) {
            warnings.push_back(keySafety.reason());
        }
    } else {
        warnings.push_back(
            "Validator key '" + validatorKeyId + "' is not loaded: " +
            key.reason()
        );
    }

    if (!manifest.loaded()) {
        warnings.push_back("Node data directory is not initialized: " + manifest.reason());
    }

    const node::OperatorDiagnosticsReport report =
        node::OperatorDiagnostics::collect(
            params,
            genesisConfig.deterministicId(),
            config::networkClassToString(params.networkClass()),
            manifest.loaded() ? manifest.manifest().latestBlockHeight() : 0,
            manifest.loaded() ? manifest.manifest().validatorCount()
                              : genesisConfig.bootstrapValidators().size(),
            manifest.loaded() ? manifest.manifest().peerCount() : 0,
            true, // resolveAndVerify() already verified genesis
            keyPolicyPassed,
            warnings
        );

    return CommandLineResult::success(report.serialize() + "\n");
}

CommandLineResult CommandLineInterface::executeKeysCreate(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryConfig directoryConfig(
        options.dataDirectory
    );

    const node::NodeDataDirectoryReadResult manifest =
        node::NodeDataDirectory::loadManifest(directoryConfig);

    if (!manifest.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot create key before init: "
            + manifest.reason()
            + "\n"
        );
    }

    const std::optional<std::string> mismatch =
        manifestNetworkMismatch(manifest.manifest(), options);

    if (mismatch.has_value()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            *mismatch + "\n"
        );
    }

    if (config::NetworkProfileRegistry::isOfficialNetwork(
            manifest.manifest().networkName())) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot create a local plaintext development key for official network '" +
                manifest.manifest().networkName() +
                "'. Provide an audited network-appropriate key provider; no development key fallback is allowed.\n"
        );
    }

    if (options.keyType == "both" &&
        options.keyIdProvided) {
        throw std::invalid_argument("--key-id requires --type user or --type validator.");
    }

    const auto seedFor =
        [&](const std::string& keyId, crypto::KeyStoreKeyType keyType) {
            if (keyType == crypto::KeyStoreKeyType::VALIDATOR &&
                keyId == defaultLocalnetKeyId()) {
                return defaultLocalnetKeySeed();
            }

            if (keyType == crypto::KeyStoreKeyType::USER &&
                keyId == defaultLocalnetUserKeyId()) {
                return defaultLocalnetUserKeySeed();
            }

            return manifest.manifest().genesisConfigId()
                + "#"
                + crypto::keyStoreKeyTypeToString(keyType)
                + "#"
                + keyId;
        };

    const auto createKey =
        [&](const std::string& keyId, crypto::KeyStoreKeyType keyType) {
            return crypto::KeyStore::createLocalKey(
                directoryConfig.keysDirectoryPath(),
                keyId,
                keyType,
                seedFor(keyId, keyType),
                options.timestamp
            );
        };

    std::vector<crypto::KeyStoreCreateResult> createdKeys;

    if (options.keyType == "both") {
        createdKeys.push_back(
            createKey(defaultLocalnetUserKeyId(), crypto::KeyStoreKeyType::USER)
        );
        createdKeys.push_back(
            createKey(defaultLocalnetKeyId(), crypto::KeyStoreKeyType::VALIDATOR)
        );
    } else {
        const crypto::KeyStoreKeyType keyType =
            crypto::keyStoreKeyTypeFromString(options.keyType);

        const std::string keyId =
            options.keyIdProvided
                ? options.keyId
                : (keyType == crypto::KeyStoreKeyType::USER
                    ? defaultLocalnetUserKeyId()
                    : defaultLocalnetKeyId());

        createdKeys.push_back(
            createKey(keyId, keyType)
        );
    }

    for (const crypto::KeyStoreCreateResult& created : createdKeys) {
        if (!created.success()) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                "Failed to create key: "
                + created.reason()
                + "\n"
            );
        }
    }

    std::ostringstream output;

    output << "Nodo development key created.\n"
           << "WARNING: this is a deterministic localnet-only development key. "
           << "Do not use it for custody, production validators, treasury, or mainnet.\n";

    for (const crypto::KeyStoreCreateResult& created : createdKeys) {
        output << "Key id: " << created.metadata().keyId() << "\n"
               << "Key type: " << crypto::keyStoreKeyTypeToString(created.metadata().keyType()) << "\n"
               << "Address: " << created.metadata().address() << "\n"
               << "Algorithm: " << crypto::cryptoAlgorithmToString(created.metadata().algorithm()) << "\n"
               << "Provider: " << created.metadata().provider() << "\n"
               << "Network profile: " << created.metadata().networkProfile() << "\n"
               << "Key file: " << created.path().string() << "\n";
    }

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeKeysList(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryConfig directoryConfig(
        options.dataDirectory
    );

    const node::NodeDataDirectoryReadResult manifest =
        node::NodeDataDirectory::loadManifest(directoryConfig);

    if (!manifest.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot list keys before init: "
            + manifest.reason()
            + "\n"
        );
    }

    const std::optional<std::string> mismatch =
        manifestNetworkMismatch(manifest.manifest(), options);

    if (mismatch.has_value()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            *mismatch + "\n"
        );
    }

    const crypto::KeyStoreListResult listed =
        crypto::KeyStore::listKeys(
            directoryConfig.keysDirectoryPath()
        );

    if (!listed.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to list keys: "
            + listed.reason()
            + "\n"
        );
    }

    std::ostringstream output;

    output << "Nodo keys\n"
           << "---------\n"
           << "Key count: " << listed.keys().size() << "\n";

    for (const crypto::StoredKeyMetadata& key : listed.keys()) {
        output << "Key id: " << key.keyId()
               << " | Type: " << crypto::keyStoreKeyTypeToString(key.keyType())
               << " | Address: " << key.address()
               << " | Algorithm: " << crypto::cryptoAlgorithmToString(key.algorithm())
               << " | Provider: " << key.provider()
               << " | Network profile: " << key.networkProfile()
               << "\n";
    }

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeValidatorList(
    const CommandLineOptions& options
) {
    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);

    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const config::GenesisConfig genesisConfig = genesisLookup.genesis();

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            genesisConfig,
            localPeerFromOptions(options)
        );

    if (!load.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot list validators: "
            + load.reason()
            + "\n"
        );
    }

    const std::vector<std::string> validators =
        load.runtime().validatorRegistry().activeValidatorAddresses();

    std::ostringstream output;

    output << "Nodo validators\n"
           << "---------------\n"
           << "Active validators: " << validators.size() << "\n";

    for (const std::string& validator : validators) {
        output << "Validator: " << validator << "\n";
    }

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeSubmitTransaction(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryConfig directoryConfig(
        options.dataDirectory
    );

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);

    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const config::GenesisConfig genesisConfig = genesisLookup.genesis();

    const node::NodeDataDirectoryReadResult manifest =
        node::NodeDataDirectory::loadManifest(directoryConfig);

    if (!manifest.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit transaction before init: "
            + manifest.reason()
            + "\n"
        );
    }

    const config::NetworkParameters networkParameters =
        genesisConfig.networkParameters();

    if (manifest.manifest().networkName() != networkParameters.networkName()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit transaction: data directory network does not match selected network parameters.\n"
        );
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            networkParameters.networkName()
        );

    if (!cryptoContext.isValid()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit transaction: invalid crypto context for network "
            + networkParameters.networkName()
            + ": "
            + cryptoContext.rejectionReason()
            + "\n"
        );
    }

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig,
            localPeerFromOptions(options)
        );

    if (!load.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit transaction: runtime reload failed before mempool admission: "
            + load.reason()
            + "\n"
        );
    }

    const std::string signingKeyId =
        options.keyIdProvided
            ? options.keyId
            : defaultLocalnetUserKeyId();

    const crypto::KeyStoreLoadResult key =
        crypto::KeyStore::loadKey(
            directoryConfig.keysDirectoryPath(),
            signingKeyId
        );

    if (!key.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit transaction without local key '"
            + signingKeyId
            + "': "
            + key.reason()
            + "\n"
        );
    }

    const node::KeySafetyCheckResult keySafety =
        node::ProductionKeySafetyGate::check(
            key.metadata(),
            manifest.manifest().networkName()
        );

    if (!keySafety.isApproved()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit transaction: " + keySafety.reason() + "\n"
        );
    }

    const crypto::Ed25519SignatureProvider provider;
    const crypto::Signer signer(
        key.keyPair(),
        provider
    );

    const core::AccountStateView accountState =
        node::RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesisConfig,
            load.runtime().blockchain(),
            static_cast<std::int64_t>(
                networkParameters.minimumFeeRawUnits()
            )
        );

    if (!accountState.hasAccount(key.metadata().address())) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit transaction: signing account does not exist in current state.\n"
        );
    }

    const std::uint64_t nonce =
        options.nonce == 0
            ? accountState.accountOrDefault(key.metadata().address()).nonce() + 1
            : options.nonce;

    const core::Transaction transaction =
        core::TransactionBuilder::buildSignedTransfer(
            core::TransactionBuildRequest(
                options.toAddress,
                utils::Amount::fromRawUnits(options.amountRaw),
                utils::Amount::fromRawUnits(options.feeRaw),
                nonce,
                options.timestamp + 10
            ),
            signer
        );

    const node::TransactionAdmissionResult admission =
        node::TransactionAdmissionValidator::validateRuntimeSubmission(
            transaction,
            key.metadata(),
            networkParameters,
            accountState,
            load.runtime().mempool(),
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );

    if (!admission.accepted()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Transaction rejected before mempool persistence: "
            + admission.reason()
            + "\n"
        );
    }

    const node::PersistentMempoolWriteResult persisted =
        node::PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            key.metadata().publicKey(),
            options.timestamp + 11
        );

    if (!persisted.success()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to persist transaction: "
            + persisted.reason()
            + "\n"
        );
    }

    std::ostringstream output;

    output << "Nodo transaction submitted.\n"
           << "Key id: " << key.keyId() << "\n"
           << "From: " << transaction.fromAddress() << "\n"
           << "To: " << transaction.toAddress() << "\n"
           << "Nonce: " << transaction.nonce() << "\n"
           << "Transaction id: " << persisted.transactionId() << "\n"
           << "Mempool file: " << persisted.path().string() << "\n";

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeProduceBlock(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryConfig directoryConfig(
        options.dataDirectory
    );

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);

    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const config::GenesisConfig genesisConfig = genesisLookup.genesis();

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            genesisConfig,
            localPeerFromOptions(options)
        );

    if (!load.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot load runtime from data directory: "
            + load.reason()
            + "\n"
        );
    }

    node::NodeRuntime runtime =
        load.runtime();

    if (runtime.mempool().empty()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot produce block: mempool is empty and the current localnet rule requires at least one transaction.\n"
        );
    }

    const std::string validatorKeyId =
        options.keyIdProvided
            ? options.keyId
            : defaultLocalnetKeyId();

    const crypto::KeyStoreLoadResult key =
        crypto::KeyStore::loadKey(
            directoryConfig.keysDirectoryPath(),
            validatorKeyId
        );

    if (!key.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot sign validator vote without local key '"
            + validatorKeyId
            + "': "
            + key.reason()
            + "\n"
        );
    }

    const node::KeySafetyCheckResult keySafety =
        node::ProductionKeySafetyGate::check(
            key.metadata(),
            load.manifest().networkName()
        );

    if (!keySafety.isApproved()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot sign validator vote: " + keySafety.reason() + "\n"
        );
    }

    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(
        key.keyPair(),
        provider
    );

    const node::RuntimeBlockPipelineResult pipeline =
        node::RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            node::RuntimeBlockPipelineConfig(
                static_cast<std::size_t>(
                    genesisConfig.networkParameters().maxTransactionsPerBlock()
                ),
                1,
                1,
                options.timestamp + 20
            ),
            signer
        );

    if (!pipeline.finalized()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to produce finalized block: "
            + pipeline.reason()
            + "\n"
        );
    }

    const node::FinalizedBlockStoreResult persistedBlock =
        node::FinalizedBlockStore::persist(
            directoryConfig,
            runtime,
            pipeline,
            options.timestamp + 30
        );

    if (!persistedBlock.success()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to persist finalized block: "
            + persistedBlock.reason()
            + "\n"
        );
    }

    // Persist epoch monetary report after successful finalization.
    // Failure here is a hard error: a persisted block without a verifiable
    // monetary report is not acceptable in the normal production path.
    {
        utils::Amount genesisSupply;
        try {
            genesisSupply = node::MonetaryFirewall::genesisSupply(genesisConfig);
        } catch (const std::exception& e) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                std::string("Block persisted but genesis supply unavailable for monetary report: ") +
                e.what() + "\n"
            );
        }

        const economics::MonetaryPolicy reportPolicy =
            economics::MonetaryPolicy::localnetDefault(
                genesisConfig.networkParameters().chainId(),
                genesisSupply
            );

        const auto reportResult = node::RuntimeMonetaryReportService::buildAndPersist(
            reportPolicy,
            runtime.supplyState().finalizedDeltas(),
            0,
            directoryConfig.epochMonetaryReportPath()
        );

        if (!reportResult.succeeded()) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                "Block persisted but monetary report persistence failed: " +
                reportResult.reason() + "\n"
            );
        }
    }

    // Persist epoch treasury report derived from the just-persisted artifact.
    // The artifact is reloaded from disk to validate the round-trip and derive
    // the treasury report from its actual treasury section (not a placeholder).
    {
        node::FinalizedBlockArtifact persistedArtifact;
        try {
            persistedArtifact = node::FinalizedBlockArtifactCodec::readBlockArtifactFile(
                persistedBlock.blockPath()
            );
        } catch (const std::exception& e) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                std::string("Block persisted but artifact reload failed for treasury report: ") +
                e.what() + "\n"
            );
        }

        const node::FinalizedTreasuryAuditResult treasuryAudit =
            node::FinalizedTreasuryAudit::auditArtifacts(0, {persistedArtifact});

        if (!treasuryAudit.passed()) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                "Block persisted but treasury audit failed: " +
                treasuryAudit.reason() + "\n"
            );
        }

        try {
            node::EpochTreasuryReportStore::write(
                directoryConfig.epochTreasuryReportPath(),
                treasuryAudit.rebuiltReport()
            );
        } catch (const std::exception& e) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                std::string("Block persisted but treasury report persistence failed: ") +
                e.what() + "\n"
            );
        }
    }

    const std::size_t removedPendingTransactions =
        node::PersistentMempoolStore::removeTransactions(
            directoryConfig,
            pipeline.finalizedTransactionIds()
        );

    std::ostringstream output;

    output << "Nodo block finalized and persisted.\n"
           << "Block height: " << pipeline.block().index() << "\n"
           << "Block hash: " << pipeline.block().hash() << "\n"
           << "Transactions finalized: " << pipeline.finalizedTransactionIds().size() << "\n"
           << "Pending transactions removed: " << removedPendingTransactions << "\n"
           << "Block file: " << persistedBlock.blockPath().string() << "\n"
           << "Manifest latest height: " << persistedBlock.manifest().latestBlockHeight() << "\n"
           << "Manifest latest state root: " << persistedBlock.manifest().latestStateRoot() << "\n";

    return CommandLineResult::success(output.str());
}

} // namespace nodo::app
