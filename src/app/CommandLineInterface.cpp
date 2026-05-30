#include "app/CommandLineInterface.hpp"

#include "app/DemoScenario.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
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
    const std::string& suffix
) {
    return crypto::PublicKey(
        crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "nodo-cli-bootstrap-validator-public-key-" + suffix
    );
}

crypto::PublicKey developmentTransactionPublicKey() {
    return crypto::PublicKey(
        crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "nodo-cli-demo-transaction-public-key"
    );
}

crypto::PrivateKey developmentTransactionPrivateKey() {
    return crypto::PrivateKey(
        crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "nodo-cli-demo-transaction-private-key"
    );
}

core::Transaction createDemoTransaction(
    std::int64_t timestamp
) {
    core::Transaction transaction(
        core::TransactionType::TRANSFER,
        "nodo-cli-demo-sender",
        "nodo-cli-demo-recipient",
        utils::Amount::fromRawUnits(1000),
        utils::Amount::fromRawUnits(100),
        static_cast<std::uint64_t>(timestamp),
        timestamp
    );

    transaction.attachSignatureBundle(
        crypto::SignatureBundle::createDevelopmentSignature(
            transaction.signingPayload(),
            developmentTransactionPublicKey(),
            developmentTransactionPrivateKey(),
            timestamp
        )
    );

    return transaction;
}

bool isOption(
    const std::string& value
) {
    return value.rfind("--", 0) == 0;
}

} // namespace

CommandLineOptions::CommandLineOptions()
    : command("help"),
      dataDirectory(".nodo"),
      peerId("local-node"),
      endpoint("127.0.0.1:9000"),
      timestamp(nowUnixSeconds()),
      showHelp(false) {}

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

        if (options.command == "reload") {
            return executeReload(options);
        }

        if (options.command == "produce-demo-block") {
            return executeProduceDemoBlock(options);
        }

        if (options.command == "submit-demo-transaction") {
            return executeSubmitDemoTransaction(options);
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
        "  nodo demo\n"
        "  nodo init [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
        "  nodo status [--data-dir PATH]\n"
        "  nodo inspect [--data-dir PATH]\n"
        "  nodo reload [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
        "  nodo submit-demo-transaction [--data-dir PATH]\n"
        "  nodo produce-demo-block [--data-dir PATH]\n"
        "\n"
        "Options:\n"
        "  --data-dir PATH      Node data directory. Default: .nodo\n"
        "  --peer-id ID         Local peer id for init/load. Default: local-node\n"
        "  --endpoint HOST:PORT Local endpoint for init/load. Default: 127.0.0.1:9000\n"
        "  --timestamp SECONDS  Deterministic timestamp override for tests.\n";
}

config::GenesisConfig CommandLineInterface::developmentGenesisConfig() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        1900000000,
        {
            config::BootstrapValidatorConfig(
                developmentValidatorKey("a"),
                1,
                1,
                "cli-bootstrap-validator-a"
            ),
            config::BootstrapValidatorConfig(
                developmentValidatorKey("b"),
                1,
                1,
                "cli-bootstrap-validator-b"
            )
        },
        "nodo-cli-devnet-genesis"
    );
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
           << "Latest height: " << result.manifest().latestBlockHeight() << "\n";

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

    output << "Nodo runtime reloaded.\n"
           << "Data directory: " << options.dataDirectory.string() << "\n"
           << "Latest height: " << load.manifest().latestBlockHeight() << "\n"
           << "Latest hash: " << load.manifest().latestBlockHash() << "\n"
           << "Loaded finalized blocks: " << load.loadedBlockCount() << "\n"
           << "Loaded mempool transactions: " << load.loadedMempoolTransactionCount() << "\n";

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

    const core::Transaction transaction =
        createDemoTransaction(options.timestamp + 10);

    const node::PersistentMempoolWriteResult persisted =
        node::PersistentMempoolStore::persistTransaction(
            directoryConfig,
            transaction,
            developmentTransactionPublicKey(),
            options.timestamp + 11
        );

    if (!persisted.success()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to persist demo transaction: "
            + persisted.reason()
            + "\n"
        );
    }

    std::ostringstream output;

    output << "Nodo demo transaction submitted.\n"
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

    /*
     * Backward-compatible operator convenience:
     * If no pending transaction was submitted yet, create one demo transaction
     * and persist it before producing the demo block.
     */
    if (runtime.mempool().empty()) {
        const core::Transaction transaction =
            createDemoTransaction(options.timestamp + 10);

        const node::PersistentMempoolWriteResult persisted =
            node::PersistentMempoolStore::persistTransaction(
                directoryConfig,
                transaction,
                developmentTransactionPublicKey(),
                options.timestamp + 11
            );

        if (!persisted.success()) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                "Failed to create fallback demo transaction: "
                + persisted.reason()
                + "\n"
            );
        }

        const auto admission =
            runtime.mutableMempool().admitTransaction(
                transaction,
                crypto::CryptoPolicy::developmentPolicy(),
                crypto::SecurityContext::USER_TRANSACTION,
                options.timestamp + 11
            );

        if (!admission.success()) {
            return CommandLineResult::failure(
                CommandLineStatus::COMMAND_FAILED,
                "Failed to admit fallback demo transaction: "
                + admission.reason()
                + "\n"
            );
        }
    }

    const node::RuntimeBlockPipelineResult pipeline =
        node::RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            node::RuntimeBlockPipelineConfig(
                100,
                1,
                1,
                options.timestamp + 20
            )
        );

    if (!pipeline.finalized()) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Failed to produce finalized demo block: "
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
            "Failed to persist finalized demo block: "
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

    output << "Nodo demo block finalized and persisted.\n"
           << "Block height: " << pipeline.block().index() << "\n"
           << "Block hash: " << pipeline.block().hash() << "\n"
           << "Transactions finalized: " << pipeline.finalizedTransactionIds().size() << "\n"
           << "Pending transactions removed: " << removedPendingTransactions << "\n"
           << "Block file: " << persistedBlock.blockPath().string() << "\n"
           << "Manifest latest height: " << persistedBlock.manifest().latestBlockHeight() << "\n";

    return CommandLineResult::success(output.str());
}

} // namespace nodo::app
