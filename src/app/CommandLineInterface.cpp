#include "app/CommandLineInterface.hpp"

#include "app/DemoScenario.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signer.hpp"
#include "crypto/SignatureBundle.hpp"
#include "node/ChainAuditor.hpp"
#include "node/ChainAuditResult.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "utils/Amount.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace nodo::app {

namespace {

std::int64_t nowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

crypto::PublicKey developmentValidatorKey(
    const std::string& seed
) {
    return crypto::KeyPair::createDeterministicBls12381KeyPair(seed).publicKey();
}

crypto::PublicKey developmentUserKey(
    const std::string& seed
) {
    return crypto::KeyPair::createDeterministicEd25519KeyPair(seed).publicKey();
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
           value == "validator";
}

} // namespace

CommandLineOptions::CommandLineOptions()
    : command("help"),
      dataDirectory(".nodo"),
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

        if (options.command == "demo") {
            const int demoStatus =
                runBlockchainFoundationDemo();

            if (demoStatus == 0) {
                return CommandLineResult::success(
                    "Nodo demo completed successfully.\n"
                );
            }

            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                "Nodo demo failed.\n"
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

        if (options.command == "reload" ||
            options.command == "node reload") {
            return executeReload(options);
        }

        if (options.command == "chain audit") {
            return executeChainAudit(options);
        }

        if (options.command == "produce-demo-block" ||
            options.command == "block produce") {
            return executeProduceDemoBlock(options);
        }

        if (options.command == "submit-demo-transaction" ||
            options.command == "tx submit") {
            return executeSubmitDemoTransaction(options);
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
        "  nodo init [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
        "  nodo status [--data-dir PATH]\n"
        "  nodo inspect [--data-dir PATH]\n"
        "  nodo node reload [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
        "  nodo keys create [--data-dir PATH] [--type user|validator|both] [--key-id ID]\n"
        "  nodo keys list [--data-dir PATH]\n"
        "  nodo tx submit [--data-dir PATH] [--from KEY_ID] [--to ADDRESS] [--amount RAW_UNITS] [--fee RAW_UNITS] [--nonce VALUE]\n"
        "  nodo block produce [--data-dir PATH]\n"
        "  nodo chain audit [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
        "  nodo validator list [--data-dir PATH]\n"
        "\n"
        "Compatibility commands:\n"
        "  nodo demo\n"
        "  nodo reload [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
        "  nodo submit-demo-transaction [--data-dir PATH]\n"
        "  nodo produce-demo-block [--data-dir PATH]\n"
        "\n"
        "Options:\n"
        "  --data-dir PATH      Node data directory. Default: .nodo\n"
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
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        1900000000,
        {
            config::BootstrapValidatorConfig(
                developmentValidatorKey(defaultLocalnetKeySeed()),
                1,
                1,
                "cli-localnet-validator"
            )
        },
        {
            config::GenesisAccountConfig(
                crypto::AddressDerivation::deriveFromPublicKey(
                    developmentUserKey(defaultLocalnetUserKeySeed())
                ).value(),
                utils::Amount::fromRawUnits(1000000000000),
                0
            )
        },
        "nodo-cli-localnet-genesis"
    );
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
    return p2p::PeerInfo(
        options.peerId,
        options.endpoint,
        config::NetworkParameters::developmentLocal().protocolVersion(),
        0,
        options.timestamp
    );
}

CommandLineResult CommandLineInterface::executeInit(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryInitResult result =
        node::NodeDataDirectory::initialize(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            developmentGenesisConfig(),
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
    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            developmentGenesisConfig(),
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

    if (options.command == "reload") {
        output << "Deprecated command: use nodo node reload.\n";
    }

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

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            developmentGenesisConfig(),
            localPeerFromOptions(options)
        );

    const node::ChainAuditResult audit =
        node::ChainAuditor::auditLoadedRuntime(
            load
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

    output << "Nodo key created.\n";

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
    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            node::NodeDataDirectoryConfig(options.dataDirectory),
            developmentGenesisConfig(),
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

CommandLineResult CommandLineInterface::executeSubmitDemoTransaction(
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
            "Cannot submit transaction before init: "
            + manifest.reason()
            + "\n"
        );
    }

    const config::NetworkParameters networkParameters =
        developmentGenesisConfig().networkParameters();

    if (manifest.manifest().networkName() != networkParameters.networkName()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Cannot submit transaction: data directory network does not match localnet parameters.\n"
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
            developmentGenesisConfig(),
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

    const crypto::Ed25519SignatureProvider provider;
    const crypto::Signer signer(
        key.keyPair(),
        provider
    );

    const core::AccountStateView accountState =
        node::RuntimeAccountStateBuilder::accountStateViewAtTip(
            developmentGenesisConfig(),
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

    if (options.command == "submit-demo-transaction") {
        output << "Deprecated command: use nodo tx submit.\n";
    }

    output << "Nodo transaction submitted.\n"
           << "Key id: " << key.keyId() << "\n"
           << "From: " << transaction.fromAddress() << "\n"
           << "To: " << transaction.toAddress() << "\n"
           << "Nonce: " << transaction.nonce() << "\n"
           << "Transaction id: " << persisted.transactionId() << "\n"
           << "Mempool file: " << persisted.path().string() << "\n";

    return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeProduceDemoBlock(
    const CommandLineOptions& options
) {
    const node::NodeDataDirectoryConfig directoryConfig(
        options.dataDirectory
    );

    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig,
            developmentGenesisConfig(),
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

    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(
        key.keyPair(),
        provider
    );

    const node::RuntimeBlockPipelineResult pipeline =
        node::RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            node::RuntimeBlockPipelineConfig(
                100,
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

    const std::size_t removedPendingTransactions =
        node::PersistentMempoolStore::removeTransactions(
            directoryConfig,
            pipeline.finalizedTransactionIds()
        );

    std::ostringstream output;

    if (options.command == "produce-demo-block") {
        output << "Deprecated command: use nodo block produce.\n";
    }

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
