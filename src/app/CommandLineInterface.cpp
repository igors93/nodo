#include "app/CommandLineInterface.hpp"
#include "app/ProtocolCommandPolicy.hpp"
#include "node/NodeDaemon.hpp"


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

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace nodo::app {

namespace {

std::string readPasswordNoEcho(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    std::string password;

#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

    std::getline(std::cin, password);

    SetConsoleMode(hStdin, mode);
#else
    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::getline(std::cin, password);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif

    std::cout << "\n";
    return password;
}

crypto::KeyStoreLoadResult loadKeyWithPrompt(
    const std::filesystem::path& keysDir,
    const std::string& keyId
) {
    const crypto::KeyStoreLoadResult metaLoad =
        crypto::KeyStore::loadKey(
            keysDir,
            keyId,
            "",
            true
        );
    
    if (metaLoad.status() != crypto::KeyStoreStatus::OK) {
        return metaLoad;
    }
    
    if (metaLoad.metadata().encryptionLevel() == crypto::KeyEncryptionLevel::PLAINTEXT) {
        return crypto::KeyStore::loadKey(keysDir, keyId);
    }
    
    const char* envVal = std::getenv("NODO_KEY_PASSWORD");
    std::string password;
    if (envVal && std::strlen(envVal) > 0) {
        password = envVal;
    } else {
        password = readPasswordNoEcho("Enter password for key '" + keyId + "': ");
    }
    
    return crypto::KeyStore::loadKey(keysDir, keyId, password);
}

std::int64_t nowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::int64_t parseSignedInt64(
    const std::string& option,
    const std::string& value
) {
    if (value.empty()) {
        throw std::invalid_argument(option + " value must not be empty.");
    }

    std::size_t pos = 0;
    long long result = 0;

    try {
        result = std::stoll(value, &pos);
    } catch (const std::out_of_range&) {
        throw std::invalid_argument(option + " value out of range: " + value);
    } catch (...) {
        throw std::invalid_argument(option + " value is not a valid integer: " + value);
    }

    if (pos != value.size()) {
        throw std::invalid_argument(
            option + " value contains non-numeric characters: " + value
        );
    }

    return static_cast<std::int64_t>(result);
}

std::uint64_t parseUnsignedInt64(
    const std::string& option,
    const std::string& value
) {
    if (value.empty()) {
        throw std::invalid_argument(option + " value must not be empty.");
    }

    if (!value.empty() && value[0] == '-') {
        throw std::invalid_argument(
            option + " requires a non-negative integer, got: " + value
        );
    }

    std::size_t pos = 0;
    unsigned long long result = 0;

    try {
        result = std::stoull(value, &pos);
    } catch (const std::out_of_range&) {
        throw std::invalid_argument(option + " value out of range: " + value);
    } catch (...) {
        throw std::invalid_argument(option + " value is not a valid integer: " + value);
    }

    if (pos != value.size()) {
        throw std::invalid_argument(
            option + " value contains non-numeric characters: " + value
        );
    }

    return static_cast<std::uint64_t>(result);
}

std::string defaultLocalnetUserKeyId() {
    return "local-user";
}

std::string defaultLocalnetUserKeySeed() {
    return config::GenesisRegistry::localnetUserKeySeed();
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
           value == "stake" ||
           value == "rewards" ||
           value == "slashing" ||
           value == "testnet";
}

bool isLegacyDevelopmentCommand(
    const std::string& command
) {
    return command == "demo" ||
           command == "reload" ||
           command == "submit-demo-transaction" ||
           command == "produce-demo-block";
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
    const config::GenesisLookupResult lookup =
        node::RuntimeStartupService::resolveGenesis(options.networkName);

    if (!lookup.found()) {
        return "Cannot resolve genesis for network '" + options.networkName +
               "': " + lookup.reason();
    }

    const node::StartupValidationResult compatCheck =
        node::RuntimeStartupService::validateDataDirectoryCompatibility(
            manifest, lookup.genesis()
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
      listenAddress(""),
      keyId("local-validator"),
      keyType("both"),
      toAddress("nodo-localnet-recipient"),
      validatorAddress(""),
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

        const CommandLineResult networkValidation =
            validateSelectedNetwork(options);

        if (!networkValidation.success()) {
            return networkValidation;
        }

        if (isLegacyDevelopmentCommand(options.command) &&
            ProtocolCommandPolicy::legacyCommandBlockingEnforced(options.networkName)) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                "Legacy development command '" + options.command +
                    "' is not permitted on official network '" +
                    options.networkName + "'.\n"
            );
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

        if (options.command == "node run") {
            return executeNodeRun(options);
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

        if (options.command == "validator status") {
            return executeValidatorStatus(options);
        }

        if (options.command == "validator exit") {
            return executeValidatorExit(options);
        }

        if (options.command == "validator unjail") {
            return executeValidatorUnjail(options);
        }

        if (options.command == "stake lock") {
            return executeStakeLock(options);
        }

        if (options.command == "stake status") {
            return executeStakeStatus(options);
        }

        if (options.command == "rewards status") {
            return executeRewardsStatus(options);
        }

        if (options.command == "slashing evidence") {
            return executeSlashingEvidence(options);
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
                parseSignedInt64("--amount", args[index + 1]);

            if (options.amountRaw < 0) {
                throw std::invalid_argument("--amount must be non-negative.");
            }

            index += 2;
            continue;
        }

        if (option == "--fee") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--fee requires a value.");
            }

            options.feeRaw =
                parseSignedInt64("--fee", args[index + 1]);

            if (options.feeRaw < 0) {
                throw std::invalid_argument("--fee must be non-negative.");
            }

            index += 2;
            continue;
        }

        if (option == "--nonce") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--nonce requires a value.");
            }

            options.nonce =
                parseUnsignedInt64("--nonce", args[index + 1]);
            index += 2;
            continue;
        }

        if (option == "--timestamp") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--timestamp requires a value.");
            }

            options.timestamp =
                parseSignedInt64("--timestamp", args[index + 1]);
            index += 2;
            continue;
        }

        if (option == "--listen") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--listen requires a value (HOST:PORT).");
            }

            options.listenAddress = args[index + 1];
            // Also propagate to endpoint so localPeerFromOptions() picks it up.
            options.endpoint = args[index + 1];
            index += 2;
            continue;
        }

        if (option == "--peer") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--peer requires a value (NAME@HOST:PORT).");
            }

            options.peers.push_back(args[index + 1]);
            index += 2;
            continue;
        }

        if (option == "--validator-key") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--validator-key requires a value.");
            }

            options.keyId = args[index + 1];
            options.keyIdProvided = true;
            index += 2;
            continue;
        }

        if (option == "--validator") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--validator requires a value (validator address).");
            }

            options.validatorAddress = args[index + 1];
            index += 2;
            continue;
        }

        if (option == "--owner") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--owner requires a value (key-id of validator owner).");
            }

            options.keyId = args[index + 1];
            options.keyIdProvided = true;
            index += 2;
            continue;
        }

        if (option == "--stake") {
            if (index + 1 >= args.size()) {
                throw std::invalid_argument("--stake requires a value (amount in raw units).");
            }

            options.amountRaw = parseSignedInt64("--stake", args[index + 1]);
            if (options.amountRaw < 0) {
                throw std::invalid_argument("--stake must be non-negative.");
            }
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
        "  nodo node run [--network localnet|testnet-candidate] [--data-dir PATH] [--listen HOST:PORT] [--peer NAME@HOST:PORT]... [--validator-key ID]\n"
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
        "Options:\n"
        "  --data-dir PATH      Node data directory. Default: .nodo\n"
        "  --network NAME       Network profile: localnet or testnet-candidate. mainnet is blocked.\n"
        "  --peer-id ID         Local peer id for init/load. Default: local-node\n"
        "  --endpoint HOST:PORT Local endpoint for init/load. Default: 127.0.0.1:9000\n"
        "  --listen HOST:PORT   Bind address for node run daemon. Overrides --endpoint.\n"
        "  --peer NAME@HOST:PORT Static peer for node run daemon (repeatable).\n"
        "  --validator-key ID   Key id for the local validator in node run.\n"
        "  --key-id ID          Key id for keys create or signing. Defaults depend on command.\n"
        "  --type TYPE          Key type for keys create. Default: both\n"
        "  --from KEY_ID        Alias for --key-id in tx submit.\n"
        "  --to ADDRESS         Recipient address for tx submit.\n"
        "  --amount RAW_UNITS   Transfer amount for tx submit. Default: 1000\n"
        "  --fee RAW_UNITS      Transfer fee for tx submit. Default: 100\n"
        "  --nonce VALUE        Transaction nonce for tx submit. Default: next account nonce\n"
        "  --timestamp SECONDS  Deterministic timestamp override for tests.\n";
}

std::string CommandLineInterface::defaultLocalnetKeyId() {
    return "local-validator";
}

std::string CommandLineInterface::defaultLocalnetKeySeed() {
    return "nodo-localnet-validator-seed";
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
        loadKeyWithPrompt(
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
        loadKeyWithPrompt(
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

    const std::string registeredGenesisId = genesisConfig.deterministicId();
    const std::string manifestGenesisId =
        manifest.loaded() ? manifest.manifest().genesisConfigId() : "";
    const bool genesisCompatible =
        manifestGenesisId.empty() ||
        manifestGenesisId == registeredGenesisId;

    const node::OperatorDiagnosticsReport report =
        node::OperatorDiagnostics::collect(
            params,
            registeredGenesisId,
            manifestGenesisId,
            config::networkClassToString(params.networkClass()),
            manifest.loaded() ? manifest.manifest().latestBlockHeight() : 0,
            manifest.loaded() ? manifest.manifest().latestBlockHash() : "",
            manifest.loaded() ? manifest.manifest().validatorCount()
                              : genesisConfig.bootstrapValidators().size(),
            manifest.loaded() ? manifest.manifest().peerCount() : 0,
            true, // resolveAndVerify() already verified genesis
            genesisCompatible,
            keyPolicyPassed,
            "",   // latestImportStatus not tracked at CLI level
            "",   // latestImportRejectionReason
            false, // defenseRestrictionsActive requires runtime
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

    std::string password;
    if (crypto::KeyEncryptionPolicy::isOfficialNetwork(options.networkName)) {
        const char* envVal = std::getenv("NODO_KEY_PASSWORD");
        if (envVal && std::strlen(envVal) > 0) {
            password = envVal;
        } else {
            password = readPasswordNoEcho("Enter password to encrypt private key: ");
            if (password.size() < 8) {
                return CommandLineResult::failure(
                    CommandLineStatus::COMMAND_FAILED,
                    "Error: Password must be at least 8 characters for official networks.\n"
                );
            }
            std::string confirm = readPasswordNoEcho("Confirm password: ");
            if (password != confirm) {
                return CommandLineResult::failure(
                    CommandLineStatus::COMMAND_FAILED,
                    "Error: Passwords do not match.\n"
                );
            }
        }
    }

    const auto createKey =
        [&](const std::string& keyId, crypto::KeyStoreKeyType keyType) {
            return crypto::KeyStore::createLocalKey(
                directoryConfig.keysDirectoryPath(),
                keyId,
                keyType,
                seedFor(keyId, keyType),
                options.timestamp,
                password,
                options.networkName
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
        loadKeyWithPrompt(
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
            signer,
            networkParameters.chainId()
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
        loadKeyWithPrompt(
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
            signer,
            &directoryConfig
        );

    if (!pipeline.finalized()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to produce finalized block: "
            + pipeline.reason()
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
                node::FinalizedBlockStore::blockFilePath(directoryConfig, pipeline.block().index())
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

    const node::NodeRuntimeManifest manifest =
        node::NodeDataDirectory::loadManifest(directoryConfig).manifest();

    std::ostringstream output;

    output << "Nodo block finalized and persisted.\n"
           << "Block height: " << pipeline.block().index() << "\n"
           << "Block hash: " << pipeline.block().hash() << "\n"
           << "Transactions finalized: " << pipeline.finalizedTransactionIds().size() << "\n"
           << "Persistent mempool updated by finalization commit.\n"
           << "Block file: " << node::FinalizedBlockStore::blockFilePath(directoryConfig, pipeline.block().index()).string() << "\n"
           << "Manifest latest height: " << manifest.latestBlockHeight() << "\n"
           << "Manifest latest state root: " << manifest.latestStateRoot() << "\n";

    return CommandLineResult::success(output.str());
}

// ---------------------------------------------------------------------------
// node run — long-running daemon
// ---------------------------------------------------------------------------

namespace {

// SIGINT/SIGTERM flag — set from signal handler, polled by runBlocking loop.
std::atomic<bool> g_daemonShutdownRequested{false};

node::NodeDaemonPeerEntry parsePeerEntry(const std::string& raw) {
    // Format: NAME@HOST:PORT
    const auto atPos = raw.find('@');
    if (atPos == std::string::npos) {
        throw std::invalid_argument(
            "Invalid --peer value '" + raw + "': expected NAME@HOST:PORT"
        );
    }
    const std::string nodeId = raw.substr(0, atPos);
    const std::string hostPort = raw.substr(atPos + 1);
    const auto colonPos = hostPort.rfind(':');
    if (colonPos == std::string::npos) {
        throw std::invalid_argument(
            "Invalid --peer value '" + raw + "': missing port in HOST:PORT"
        );
    }
    const std::string host = hostPort.substr(0, colonPos);
    const std::string portStr = hostPort.substr(colonPos + 1);

    std::uint16_t port = 0;
    try {
        const unsigned long raw_port = std::stoul(portStr);
        if (raw_port == 0 || raw_port > 65535) {
            throw std::out_of_range("port out of range");
        }
        port = static_cast<std::uint16_t>(raw_port);
    } catch (...) {
        throw std::invalid_argument(
            "Invalid --peer port in '" + raw + "'"
        );
    }

    node::NodeDaemonPeerEntry entry;
    entry.nodeId = nodeId;
    entry.host   = host;
    entry.port   = port;
    return entry;
}

} // namespace

CommandLineResult CommandLineInterface::executeNodeRun(
    const CommandLineOptions& options
) {
    // Security gate: reject mainnet.
    const config::NetworkParameters params =
        networkParametersForOptions(options);

    if (params.networkClass() == config::NetworkClass::LOCKED_PRODUCTION) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "node run: mainnet (locked production) is not supported in this build.\n"
        );
    }

    // Resolve and verify genesis — refuse to start with an unknown genesis.
    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);

    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "node run: " + genesisLookup.reason() + "\n"
        );
    }

    const config::GenesisConfig genesisConfig = genesisLookup.genesis();
    const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

    // If already initialized, verify data-dir belongs to the same genesis.
    if (node::NodeDataDirectory::isInitialized(directoryConfig)) {
        const node::NodeDataDirectoryReadResult manifest =
            node::NodeDataDirectory::loadManifest(directoryConfig);

        if (manifest.loaded()) {
            const std::optional<std::string> mismatch =
                manifestNetworkMismatch(manifest.manifest(), options);

            if (mismatch.has_value()) {
                return CommandLineResult::failure(
                    CommandLineStatus::COMMAND_FAILED,
                    "node run: data directory belongs to a different genesis: "
                    + *mismatch + "\n"
                );
            }
        }
    }

    // BLS provider must outlive the Signer and the daemon.
    const crypto::Bls12381SignatureProvider blsProvider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();

    // Load optional validator key (for signing blocks / votes).
    std::optional<crypto::Signer> localSigner;
    std::string localValidatorAddress;

    const std::string validatorKeyId =
        options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();

    const crypto::KeyStoreLoadResult key =
        loadKeyWithPrompt(
            directoryConfig.keysDirectoryPath(),
            validatorKeyId
        );

    if (key.loaded()) {
        const node::KeySafetyCheckResult keySafety =
            node::ProductionKeySafetyGate::check(
                key.metadata(),
                params.networkName()
            );

        if (!keySafety.isApproved()) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                "node run: key safety check failed: " + keySafety.reason() + "\n"
            );
        }

        localSigner.emplace(key.keyPair(), blsProvider);
        localValidatorAddress = localSigner->address();
    }

    // Parse static peers.
    std::vector<node::NodeDaemonPeerEntry> staticPeers;
    for (const auto& rawPeer : options.peers) {
        try {
            staticPeers.push_back(parsePeerEntry(rawPeer));
        } catch (const std::exception& e) {
            return CommandLineResult::failure(
                CommandLineStatus::INVALID_ARGUMENTS,
                std::string("node run: ") + e.what() + "\n"
            );
        }
    }

    // Build daemon config.
    const node::NodeOrchestratorConfig orchestratorConfig(
        genesisConfig,
        directoryConfig,
        localPeerFromOptions(options),
        localValidatorAddress,
        8545,
        "127.0.0.1",
        100,
        static_cast<std::size_t>(
            genesisConfig.networkParameters().maxTransactionsPerBlock()
        )
    );

    node::NodeDaemonConfig daemonConfig;
    daemonConfig.orchestratorConfig = orchestratorConfig;
    daemonConfig.staticPeers        = std::move(staticPeers);

    node::NodeDaemon daemon(daemonConfig, policy, blsProvider);

    if (localSigner.has_value()) {
        daemon.setLocalSigner(std::move(*localSigner));
    }

    // Set up SIGINT/SIGTERM handler.
    g_daemonShutdownRequested.store(false);
    std::signal(SIGINT,  [](int) { g_daemonShutdownRequested.store(true); });
    std::signal(SIGTERM, [](int) { g_daemonShutdownRequested.store(true); });

    const auto startResult = daemon.start();
    if (!startResult.running()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "node run: failed to start daemon: " + startResult.reason + "\n"
        );
    }

    std::cout << "Nodo daemon running on " << options.endpoint
              << " (network: " << options.networkName << ")\n"
              << "Press Ctrl+C to stop.\n";
    std::cout.flush();

    // Poll g_daemonShutdownRequested in the tick loop instead of runBlocking().
    while (!g_daemonShutdownRequested.load() && daemon.isRunning()) {
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        daemon.tick(static_cast<std::int64_t>(now));

        std::this_thread::sleep_for(
            std::chrono::milliseconds(node::NodeDaemon::DEFAULT_TICK_INTERVAL_MS)
        );
    }

    daemon.stop();

    return CommandLineResult::success("Nodo daemon stopped.\n");
}

// ---------------------------------------------------------------------------
// Validator lifecycle + staking CLI commands
// ---------------------------------------------------------------------------

CommandLineResult CommandLineInterface::executeValidatorStatus(
    const CommandLineOptions& options
) {
    const std::string addr = options.validatorAddress.empty()
        ? options.toAddress
        : options.validatorAddress;

    if (addr.empty()) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Provide --validator <address> to inspect a validator.\n"
        );
    }

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);
    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            genesisLookup.genesis(),
            localPeerFromOptions(options)
        );

    if (!load.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot load runtime state: " + load.reason() + "\n"
        );
    }

    const core::ValidatorRegistryEntry* entry =
        load.runtime().validatorRegistry().entryForAddress(addr);

    std::ostringstream out;
    out << "Validator status\n"
        << "----------------\n"
        << "Address: " << addr << "\n";

    if (!entry) {
        out << "Status: NOT REGISTERED\n";
    } else {
        out << "Status: "
            << core::validatorRegistrationStatusToString(entry->status()) << "\n"
            << "Eligible for consensus: "
            << (entry->eligibleForConsensus() ? "yes" : "no") << "\n"
            << "Stake (raw units): " << entry->stakeAmount() << "\n"
            << "Activation epoch: "
            << entry->registrationRecord().activationEpoch() << "\n"
            << "Owner: " << entry->ownerAddress() << "\n";

        if (entry->jailed()) {
            out << "Jail until epoch: " << entry->jailUntilEpoch() << "\n";
        }
        if (entry->status() == core::ValidatorRegistrationStatus::EXIT_REQUESTED) {
            out << "Exit request height: " << entry->exitRequestHeight() << "\n";
        }
    }

    return CommandLineResult::success(out.str());
}

CommandLineResult CommandLineInterface::executeValidatorExit(
    const CommandLineOptions& options
) {
    const std::string validatorAddr = options.validatorAddress.empty()
        ? options.toAddress
        : options.validatorAddress;

    if (validatorAddr.empty()) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Provide --validator <address> to request exit for.\n"
        );
    }

    const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);
    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }
    const config::GenesisConfig genesisConfig = genesisLookup.genesis();
    const config::NetworkParameters networkParameters = genesisConfig.networkParameters();

    const node::NodeDataDirectoryReadResult manifest =
        node::NodeDataDirectory::loadManifest(directoryConfig);
    if (!manifest.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit before init: " + manifest.reason() + "\n"
        );
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(networkParameters.networkName());
    if (!cryptoContext.isValid()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Invalid crypto context: " + cryptoContext.rejectionReason() + "\n"
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
            "Runtime reload failed: " + load.reason() + "\n"
        );
    }

    const std::string keyId = options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();
    const crypto::KeyStoreLoadResult key =
        loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), keyId);
    if (!key.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot load key '" + keyId + "': " + key.reason() + "\n"
        );
    }

    const node::KeySafetyCheckResult keySafety =
        node::ProductionKeySafetyGate::check(key.metadata(), manifest.manifest().networkName());
    if (!keySafety.isApproved()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Key safety: " + keySafety.reason() + "\n"
        );
    }

    const crypto::Ed25519SignatureProvider provider;
    const crypto::Signer signer(key.keyPair(), provider);

    const core::AccountStateView accountState =
        node::RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesisConfig,
            load.runtime().blockchain(),
            static_cast<std::int64_t>(networkParameters.minimumFeeRawUnits())
        );
    if (!accountState.hasAccount(key.metadata().address())) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Signing account does not exist in current state.\n"
        );
    }

    const std::uint64_t nextNonce = options.nonce == 0
        ? accountState.accountOrDefault(key.metadata().address()).nonce() + 1
        : options.nonce;

    const core::Transaction tx =
        core::TransactionBuilder::buildSignedValidatorExitRequest(
            core::TransactionBuildRequest(
                validatorAddr,
                utils::Amount(),
                utils::Amount::fromRawUnits(options.feeRaw),
                nextNonce,
                options.timestamp + 10
            ),
            signer,
            networkParameters.chainId()
        );

    const node::TransactionAdmissionResult admission =
        node::TransactionAdmissionValidator::validateRuntimeSubmission(
            tx,
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
            "Transaction rejected: " + admission.reason() + "\n"
        );
    }

    const node::PersistentMempoolWriteResult persisted =
        node::PersistentMempoolStore::persistTransaction(
            directoryConfig, tx, key.metadata().publicKey(), tx.timestamp() + 1
        );
    if (!persisted.success()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to persist: " + persisted.reason() + "\n"
        );
    }

    std::ostringstream out;
    out << "Validator exit request submitted.\n"
        << "Validator: " << validatorAddr << "\n"
        << "Transaction id: " << persisted.transactionId() << "\n";
    return CommandLineResult::success(out.str());
}

CommandLineResult CommandLineInterface::executeValidatorUnjail(
    const CommandLineOptions& options
) {
    const std::string validatorAddr = options.validatorAddress.empty()
        ? options.toAddress
        : options.validatorAddress;

    if (validatorAddr.empty()) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Provide --validator <address> to unjail.\n"
        );
    }

    const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);
    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }
    const config::GenesisConfig genesisConfig = genesisLookup.genesis();
    const config::NetworkParameters networkParameters = genesisConfig.networkParameters();

    const node::NodeDataDirectoryReadResult manifest =
        node::NodeDataDirectory::loadManifest(directoryConfig);
    if (!manifest.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit before init: " + manifest.reason() + "\n"
        );
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(networkParameters.networkName());
    if (!cryptoContext.isValid()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Invalid crypto context: " + cryptoContext.rejectionReason() + "\n"
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
            "Runtime reload failed: " + load.reason() + "\n"
        );
    }

    const std::string keyId = options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();
    const crypto::KeyStoreLoadResult key =
        loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), keyId);
    if (!key.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot load key '" + keyId + "': " + key.reason() + "\n"
        );
    }

    const node::KeySafetyCheckResult keySafety =
        node::ProductionKeySafetyGate::check(key.metadata(), manifest.manifest().networkName());
    if (!keySafety.isApproved()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Key safety: " + keySafety.reason() + "\n"
        );
    }

    const crypto::Ed25519SignatureProvider provider;
    const crypto::Signer signer(key.keyPair(), provider);

    const core::AccountStateView accountState =
        node::RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesisConfig,
            load.runtime().blockchain(),
            static_cast<std::int64_t>(networkParameters.minimumFeeRawUnits())
        );
    if (!accountState.hasAccount(key.metadata().address())) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Signing account does not exist in current state.\n"
        );
    }

    const std::uint64_t nextNonce = options.nonce == 0
        ? accountState.accountOrDefault(key.metadata().address()).nonce() + 1
        : options.nonce;

    const core::Transaction tx =
        core::TransactionBuilder::buildSignedValidatorUnjailRequest(
            core::TransactionBuildRequest(
                validatorAddr,
                utils::Amount(),
                utils::Amount::fromRawUnits(options.feeRaw),
                nextNonce,
                options.timestamp + 10
            ),
            signer,
            networkParameters.chainId()
        );

    const node::TransactionAdmissionResult admission =
        node::TransactionAdmissionValidator::validateRuntimeSubmission(
            tx,
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
            "Transaction rejected: " + admission.reason() + "\n"
        );
    }

    const node::PersistentMempoolWriteResult persisted =
        node::PersistentMempoolStore::persistTransaction(
            directoryConfig, tx, key.metadata().publicKey(), tx.timestamp() + 1
        );
    if (!persisted.success()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to persist: " + persisted.reason() + "\n"
        );
    }

    std::ostringstream out;
    out << "Validator unjail request submitted.\n"
        << "Validator: " << validatorAddr << "\n"
        << "Transaction id: " << persisted.transactionId() << "\n";
    return CommandLineResult::success(out.str());
}

CommandLineResult CommandLineInterface::executeStakeLock(
    const CommandLineOptions& options
) {
    const std::string validatorAddr = options.validatorAddress.empty()
        ? options.toAddress
        : options.validatorAddress;

    if (validatorAddr.empty()) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Provide --validator <address> to stake into.\n"
        );
    }

    if (options.amountRaw <= 0) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Provide --stake <amount> (positive integer raw units).\n"
        );
    }

    const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);
    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }
    const config::GenesisConfig genesisConfig = genesisLookup.genesis();
    const config::NetworkParameters networkParameters = genesisConfig.networkParameters();

    const node::NodeDataDirectoryReadResult manifest =
        node::NodeDataDirectory::loadManifest(directoryConfig);
    if (!manifest.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit before init: " + manifest.reason() + "\n"
        );
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(networkParameters.networkName());
    if (!cryptoContext.isValid()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Invalid crypto context: " + cryptoContext.rejectionReason() + "\n"
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
            "Runtime reload failed: " + load.reason() + "\n"
        );
    }

    const std::string keyId = options.keyIdProvided
        ? options.keyId
        : defaultLocalnetUserKeyId();
    const crypto::KeyStoreLoadResult key =
        loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), keyId);
    if (!key.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot load key '" + keyId + "': " + key.reason() + "\n"
        );
    }

    const node::KeySafetyCheckResult keySafety =
        node::ProductionKeySafetyGate::check(key.metadata(), manifest.manifest().networkName());
    if (!keySafety.isApproved()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Key safety: " + keySafety.reason() + "\n"
        );
    }

    const crypto::Ed25519SignatureProvider provider;
    const crypto::Signer signer(key.keyPair(), provider);

    const core::AccountStateView accountState =
        node::RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesisConfig,
            load.runtime().blockchain(),
            static_cast<std::int64_t>(networkParameters.minimumFeeRawUnits())
        );
    if (!accountState.hasAccount(key.metadata().address())) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Signing account does not exist in current state.\n"
        );
    }

    const std::uint64_t nextNonce = options.nonce == 0
        ? accountState.accountOrDefault(key.metadata().address()).nonce() + 1
        : options.nonce;

    const core::Transaction tx =
        core::TransactionBuilder::buildSignedStakeLock(
            core::TransactionBuildRequest(
                validatorAddr,
                utils::Amount::fromRawUnits(options.amountRaw),
                utils::Amount::fromRawUnits(options.feeRaw),
                nextNonce,
                options.timestamp + 10
            ),
            signer,
            networkParameters.chainId()
        );

    const node::TransactionAdmissionResult admission =
        node::TransactionAdmissionValidator::validateRuntimeSubmission(
            tx,
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
            "Transaction rejected: " + admission.reason() + "\n"
        );
    }

    const node::PersistentMempoolWriteResult persisted =
        node::PersistentMempoolStore::persistTransaction(
            directoryConfig, tx, key.metadata().publicKey(), tx.timestamp() + 1
        );
    if (!persisted.success()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to persist: " + persisted.reason() + "\n"
        );
    }

    std::ostringstream out;
    out << "Stake lock submitted.\n"
        << "Validator: " << validatorAddr << "\n"
        << "Amount: " << options.amountRaw << " raw units\n"
        << "Transaction id: " << persisted.transactionId() << "\n";
    return CommandLineResult::success(out.str());
}

CommandLineResult CommandLineInterface::executeStakeStatus(
    const CommandLineOptions& options
) {
    const std::string addr = options.validatorAddress.empty()
        ? options.toAddress
        : options.validatorAddress;

    if (addr.empty()) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Provide --validator <address> to inspect stake for.\n"
        );
    }

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);
    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            genesisLookup.genesis(),
            localPeerFromOptions(options)
        );

    if (!load.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot load runtime state: " + load.reason() + "\n"
        );
    }

    const core::ValidatorRegistryEntry* entry =
        load.runtime().validatorRegistry().entryForAddress(addr);

    std::ostringstream out;
    out << "Stake status\n"
        << "------------\n"
        << "Validator: " << addr << "\n";

    if (!entry) {
        out << "No validator registration found for this address.\n";
    } else {
        out << "Registry status: "
            << core::validatorRegistrationStatusToString(entry->status()) << "\n"
            << "Bonded stake (raw units): " << entry->stakeAmount() << "\n";
    }

    return CommandLineResult::success(out.str());
}

CommandLineResult CommandLineInterface::executeRewardsStatus(
    const CommandLineOptions& options
) {
    const std::string validatorAddr = options.validatorAddress.empty()
        ? options.toAddress
        : options.validatorAddress;

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);
    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            genesisLookup.genesis(),
            localPeerFromOptions(options)
        );
    if (!load.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot load runtime state: " + load.reason() + "\n"
        );
    }

    const core::Blockchain& blockchain = load.runtime().blockchain();
    const std::uint64_t chainHeight = blockchain.empty()
        ? 0u
        : blockchain.latestBlock().index();
    const std::vector<node::FinalizedBlockArtifact>& artifacts = load.loadedArtifacts();

    std::ostringstream output;
    output << "Nodo rewards status\n"
           << "-------------------\n"
           << "Chain height: " << chainHeight << "\n";

    if (!validatorAddr.empty()) {
        output << "Validator: " << validatorAddr << "\n";

        if (!artifacts.empty()) {
            const node::FinalizedBlockArtifact& tipArtifact = artifacts.back();
            std::int64_t totalReward = 0;

            for (const auto& dist : tipArtifact.rewardDistributions()) {
                if (dist.validatorAddress() == validatorAddr) {
                    totalReward += dist.liquidReward().rawUnits();
                    output << "  Block " << dist.blockHeight()
                           << " liquid reward: " << dist.liquidReward().rawUnits() << "\n";
                }
            }
            output << "Total liquid rewards at tip: " << totalReward << " raw units\n";
        } else {
            output << "No finalized artifacts loaded.\n";
        }
    } else {
        output << "Use --validator <address> to see rewards for a specific validator.\n";
    }

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeSlashingEvidence(
    const CommandLineOptions& options
) {
    const std::string validatorAddr = options.validatorAddress.empty()
        ? options.toAddress
        : options.validatorAddress;

    const config::GenesisLookupResult genesisLookup =
        node::RuntimeStartupService::resolveAndVerify(options.networkName);
    if (!genesisLookup.found()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            genesisLookup.reason() + "\n"
        );
    }

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            genesisLookup.genesis(),
            localPeerFromOptions(options)
        );
    if (!load.loaded()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot load runtime state: " + load.reason() + "\n"
        );
    }

    const core::Blockchain& blockchain = load.runtime().blockchain();
    const std::uint64_t chainHeight = blockchain.empty()
        ? 0u
        : blockchain.latestBlock().index();
    const std::vector<node::FinalizedBlockArtifact>& artifacts = load.loadedArtifacts();

    std::ostringstream output;
    output << "Nodo slashing evidence\n"
           << "----------------------\n"
           << "Chain height: " << chainHeight << "\n";

    if (!validatorAddr.empty()) {
        output << "Validator: " << validatorAddr << "\n";

        if (!artifacts.empty()) {
            const node::FinalizedBlockArtifact& tipArtifact = artifacts.back();
            std::size_t evidenceCount = 0;

            for (const auto& rec : tipArtifact.cryptographicSlashingEvidenceRecords()) {
                if (rec.validatorAddress() == validatorAddr) {
                    output << "  Evidence at block " << rec.blockHeight()
                           << " round " << rec.round()
                           << " severity " << rec.severityScore() << "\n";
                    ++evidenceCount;
                }
            }
            for (const auto& pen : tipArtifact.stakePenaltyRecords()) {
                if (pen.validatorAddress() == validatorAddr) {
                    output << "  Stake penalty: before=" << pen.lockedStakeBefore().rawUnits()
                           << " after=" << pen.lockedStakeAfter().rawUnits()
                           << " penalty=" << pen.penaltyAmount().rawUnits() << "\n";
                }
            }
            if (evidenceCount == 0) {
                output << "No slashing evidence found at current tip.\n";
            }
        } else {
            output << "No finalized artifacts loaded.\n";
        }
    } else {
        output << "Use --validator <address> to see slashing evidence for a specific validator.\n";
    }

    return CommandLineResult::success(output.str());
}

} // namespace nodo::app
