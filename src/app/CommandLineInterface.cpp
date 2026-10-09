#include "app/CommandLineInterface.hpp"
#include "app/ProtocolCommandPolicy.hpp"
#include "node/NodeDaemon.hpp"

#include "config/GenesisDocumentCodec.hpp"
#include "config/GenesisRegistry.hpp"
#include "config/NetworkProfileRegistry.hpp"
#include "core/GenesisVerifier.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/Signer.hpp"
#include "economics/EpochTreasuryReport.hpp"
#include "economics/GovernanceLifecycleVerifier.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "node/ChainAuditResult.hpp"
#include "node/ChainAuditor.hpp"
#include "node/EpochTreasuryReportStore.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/FinalizedTreasuryAudit.hpp"
#include "node/GovernanceLifecycleRecordBuilder.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodePruningService.hpp"
#include "node/NodeRuntime.hpp"
#include "node/OperatorDiagnostics.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/ProductionKeySafetyGate.hpp"
#include "node/ReadinessContext.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeMonetaryReportService.hpp"
#include "node/RuntimeStartupService.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "node/TestnetReadinessChecker.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "storage/AtomicFile.hpp"
#include "utils/Amount.hpp"

#include <atomic>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>

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

std::string readPasswordNoEcho(const std::string &prompt) {
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

crypto::KeyStoreLoadResult
loadKeyWithPrompt(const std::filesystem::path &keysDir,
                  const std::string &keyId) {
  const crypto::KeyStoreLoadResult metaLoad =
      crypto::KeyStore::loadKey(keysDir, keyId, "", true);

  if (metaLoad.status() != crypto::KeyStoreStatus::OK) {
    return metaLoad;
  }

  if (metaLoad.metadata().encryptionLevel() ==
      crypto::KeyEncryptionLevel::PLAINTEXT) {
    return crypto::KeyStore::loadKey(keysDir, keyId);
  }

  const char *envVal = std::getenv("NODO_KEY_PASSWORD");
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
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::int64_t parseSignedInt64(const std::string &option,
                              const std::string &value) {
  if (value.empty()) {
    throw std::invalid_argument(option + " value must not be empty.");
  }

  std::size_t pos = 0;
  long long result = 0;

  try {
    result = std::stoll(value, &pos);
  } catch (const std::out_of_range &) {
    throw std::invalid_argument(option + " value out of range: " + value);
  } catch (...) {
    throw std::invalid_argument(option +
                                " value is not a valid integer: " + value);
  }

  if (pos != value.size()) {
    throw std::invalid_argument(
        option + " value contains non-numeric characters: " + value);
  }

  return static_cast<std::int64_t>(result);
}

std::string defaultLocalnetUserKeyId() { return "local-user"; }

std::string defaultLocalnetUserKeySeed() {
  return config::GenesisRegistry::localnetUserKeySeed();
}

bool isLegacyDevelopmentCommand(const std::string &command) {
  return command == "demo" || command == "reload" ||
         command == "submit-demo-transaction" ||
         command == "produce-demo-block";
}

config::NetworkParameters
networkParametersForOptions(const CommandLineOptions &options) {
  return config::NetworkProfileRegistry::get(options.networkName);
}

CommandLineResult validateSelectedNetwork(const CommandLineOptions &options) {
  if (!config::NetworkProfileRegistry::isKnown(options.networkName)) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Unknown network profile: " + options.networkName + "\n");
  }

  const config::NetworkParameters params = networkParametersForOptions(options);

  const node::StartupValidationResult profileCheck =
      node::RuntimeStartupService::validateNetworkProfile(params);

  if (!profileCheck.valid()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      profileCheck.reason() + "\n");
  }

  return CommandLineResult::success("");
}

// Every command resolves its genesis here, so networks that require an
// operator genesis read it from --genesis-file or the data directory.
config::GenesisLookupResult
resolveGenesisForOptions(const CommandLineOptions &options) {
  return node::RuntimeStartupService::resolveAndVerify(
      options.networkName, options.dataDirectory, options.genesisFile);
}

std::optional<std::string>
manifestNetworkMismatch(const node::NodeRuntimeManifest &manifest,
                        const CommandLineOptions &options) {
  const config::GenesisLookupResult lookup =
      node::RuntimeStartupService::resolveGenesis(
          options.networkName, options.dataDirectory, options.genesisFile);

  if (!lookup.found()) {
    return "Cannot resolve genesis for network '" + options.networkName +
           "': " + lookup.reason();
  }

  const node::StartupValidationResult compatCheck =
      node::RuntimeStartupService::validateDataDirectoryCompatibility(
          manifest, lookup.genesis());

  if (!compatCheck.valid()) {
    return compatCheck.reason();
  }

  return std::nullopt;
}

std::string normalizeGovernanceToken(std::string value) {
  for (char &c : value) {
    if (c == '-') {
      c = '_';
    } else {
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
  }
  return value;
}

core::GovernanceProposalType
parseGovernanceProposalTypeOption(const std::string &value) {
  return core::governanceProposalTypeFromString(
      normalizeGovernanceToken(value));
}

core::GovernanceVoteChoice
parseGovernanceVoteChoiceOption(const std::string &value) {
  return core::governanceVoteChoiceFromString(normalizeGovernanceToken(value));
}

CommandLineResult submitSignedTransactionToPersistentMempool(
    const CommandLineOptions &options, const std::string &actionLabel,
    const std::string &defaultKeyId,
    const std::function<core::Transaction(const crypto::Signer &,
                                          const std::string &, std::uint64_t)>
        &buildTransaction) {
  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const config::GenesisConfig genesisConfig = genesisLookup.genesis();
  const config::NetworkParameters networkParameters =
      genesisConfig.networkParameters();

  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);

  if (!manifest.loaded()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "Cannot submit " + actionLabel +
                                          " before init: " + manifest.reason() +
                                          "\n");
  }

  const std::optional<std::string> mismatch =
      manifestNetworkMismatch(manifest.manifest(), options);
  if (mismatch.has_value()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      *mismatch + "\n");
  }

  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(
          networkParameters.networkName());

  if (!cryptoContext.isValid()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit " + actionLabel +
            ": invalid crypto context for network " +
            networkParameters.networkName() + ": " +
            cryptoContext.rejectionReason() + "\n");
  }

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesisConfig,
          CommandLineInterface::localPeerFromOptions(options));

  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit " + actionLabel +
            ": runtime reload failed before mempool admission: " +
            load.reason() + "\n");
  }

  const std::string signingKeyId =
      options.keyIdProvided ? options.keyId : defaultKeyId;

  const crypto::KeyStoreLoadResult key =
      loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), signingKeyId);

  if (!key.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit " + actionLabel + " without local key '" + signingKeyId +
            "': " + key.reason() + "\n");
  }

  const node::KeySafetyCheckResult keySafety =
      node::ProductionKeySafetyGate::check(key.metadata(),
                                           manifest.manifest().networkName());

  if (!keySafety.isApproved()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "Cannot submit " + actionLabel + ": " +
                                          keySafety.reason() + "\n");
  }

  const crypto::Ed25519SignatureProvider provider;
  const crypto::Signer signer(key.keyPair(), provider);

  const core::AccountStateView accountState =
      node::RuntimeAccountStateBuilder::accountStateViewAtTip(
          genesisConfig, load.runtime().blockchain(),
          static_cast<std::int64_t>(networkParameters.minimumFeeRawUnits()));

  if (!accountState.hasAccount(key.metadata().address())) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit " + actionLabel +
            ": signing account does not exist in current state.\n");
  }

  const std::uint64_t nonce =
      options.nonce == 0
          ? accountState.accountOrDefault(key.metadata().address()).nonce() + 1
          : options.nonce;

  std::optional<core::Transaction> transaction;
  try {
    transaction = buildTransaction(signer, networkParameters.chainId(), nonce);
  } catch (const std::exception &error) {
    return CommandLineResult::failure(CommandLineStatus::INVALID_ARGUMENTS,
                                      "Cannot build " + actionLabel + ": " +
                                          error.what() + "\n");
  }

  const core::Transaction &signedTransaction = *transaction;

  const node::TransactionAdmissionContext admissionContext(
      accountState, load.runtime().mempool(), load.runtime().stakingRegistry(),
      load.runtime().validatorRegistry(), load.runtime().governanceExecutor(),
      load.runtime().blockchain().size(),
      load.runtime().blockchain().latestBlock().timestamp());

  const node::TransactionAdmissionResult admission =
      node::TransactionAdmissionValidator::validateRuntimeSubmission(
          signedTransaction, key.metadata(), networkParameters, accountState,
          load.runtime().mempool(), cryptoContext.policy(),
          crypto::SecurityContext::USER_TRANSACTION, provider,
          load.runtime().effectiveMinimumFeeRawUnits(), &admissionContext);

  if (!admission.accepted()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        actionLabel + " rejected before mempool persistence: " +
            admission.reason() + "\n");
  }

  const node::PersistentMempoolWriteResult persisted =
      node::PersistentMempoolStore::persistTransaction(
          directoryConfig, signedTransaction, key.metadata().publicKey(),
          signedTransaction.timestamp() + 1);

  if (!persisted.success()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "Failed to persist " + actionLabel +
                                          ": " + persisted.reason() + "\n");
  }

  std::ostringstream output;

  output << actionLabel << " submitted.\n"
         << "Key id: " << key.keyId() << "\n"
         << "From: " << signedTransaction.fromAddress() << "\n"
         << "To: " << signedTransaction.toAddress() << "\n"
         << "Type: " << core::transactionTypeToString(signedTransaction.type())
         << "\n"
         << "Nonce: " << signedTransaction.nonce() << "\n"
         << "Transaction id: " << persisted.transactionId() << "\n";

  if (signedTransaction.type() == core::TransactionType::GOVERNANCE_PROPOSE) {
    output << "Proposal id: " << signedTransaction.id() << "\n";
  } else if (signedTransaction.type() ==
             core::TransactionType::GOVERNANCE_VOTE) {
    output << "Proposal id: " << signedTransaction.toAddress() << "\n";
  }

  output << "Mempool file: " << persisted.path().string() << "\n";

  return CommandLineResult::success(output.str());
}

} // namespace

CommandLineOptions::CommandLineOptions()
    : command("help"), dataDirectory(".nodo"), networkName("localnet"),
      peerId("local-node"), endpoint("127.0.0.1:9000"), listenAddress(""),
      rpcBindAddress("127.0.0.1"), rpcPort(8545), keyId("local-validator"),
      validatorKeyId(""), identityKeyId("local-user"), keyType("both"),
      toAddress("nodo-localnet-recipient"), validatorAddress(""),
      governanceProposalId(""), governanceProposalType("parameter-change"),
      governanceProposalTitle("Governance proposal"),
      governanceProposalBody("Submitted from Nodo CLI."), governanceTarget(""),
      governanceValue(""), governanceVoteChoice("YES"), amountRaw(1000),
      feeRaw(100), nonce(0), timestamp(nowUnixSeconds()),
      governanceEffectiveHeight(0), governanceVotingPeriodBlocks(3),
      showHelp(false), keyIdProvided(false), validatorKeyIdProvided(false),
      outputJson(false), pruningMode("archive"), pruningRetainEpochs(1),
      genesisFile(), genesisValidators(), genesisAccounts(), genesisMemo(""),
      outputPath(), height(0), heightProvided(false), sourceDataDirectory(),
      trustedCheckpoint(""), retainBlocks(0), retainSnapshots(0),
      dryRun(false), segmentIndex(0), segmentProvided(false) {}

std::string commandLineStatusToString(CommandLineStatus status) {
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

CommandLineResult CommandLineResult::success(std::string message) {
  CommandLineResult result;
  result.m_status = CommandLineStatus::SUCCESS;
  result.m_message = std::move(message);
  return result;
}

CommandLineResult CommandLineResult::failure(CommandLineStatus status,
                                             std::string message) {
  CommandLineResult result;
  result.m_status = status;
  result.m_message = std::move(message);
  return result;
}

CommandLineStatus CommandLineResult::status() const { return m_status; }

const std::string &CommandLineResult::message() const { return m_message; }

bool CommandLineResult::success() const {
  return m_status == CommandLineStatus::SUCCESS;
}

int CommandLineInterface::run(int argc, char **argv) {
  std::vector<std::string> args;

  for (int index = 1; index < argc; ++index) {
    args.emplace_back(argv[index]);
  }

  const CommandLineResult result = execute(args);

  if (result.success()) {
    std::cout << result.message();
    return 0;
  }

  std::cerr << result.message();
  return 1;
}

CommandLineResult
CommandLineInterface::execute(const std::vector<std::string> &args) {
  try {
    const CommandLineOptions options = parse(args);

    if (options.showHelp || options.command == "help") {
      return CommandLineResult::success(helpText());
    }

    const CommandLineResult networkValidation =
        validateSelectedNetwork(options);

    if (!networkValidation.success()) {
      return networkValidation;
    }

    if (isLegacyDevelopmentCommand(options.command) &&
        ProtocolCommandPolicy::legacyCommandBlockingEnforced(
            options.networkName)) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          "Legacy development command '" + options.command +
              "' is not permitted on official network '" + options.networkName +
              "'.\n");
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

    if (isHistoryCommand(options.command)) {
      return executeHistoryCommand(options);
    }

    if (options.command == "node prune") {
      return executeNodePrune(options);
    }

    if (options.command == "node pruning-status") {
      return executeNodePruningStatus(options);
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

    if (options.command == "governance propose") {
      return executeGovernancePropose(options);
    }

    if (options.command == "governance vote") {
      return executeGovernanceVote(options);
    }

    if (options.command == "governance execute") {
      return executeGovernanceExecute(options);
    }

    if (options.command == "governance status") {
      return executeGovernanceStatus(options);
    }

    if (options.command == "governance list") {
      return executeGovernanceList(options);
    }

    if (options.command == "governance show") {
      return executeGovernanceShow(options);
    }

    if (options.command == "governance audit") {
      return executeGovernanceAudit(options);
    }

    if (options.command == "keys create") {
      return executeKeysCreate(options);
    }

    if (options.command == "keys list") {
      return executeKeysList(options);
    }

    if (options.command == "genesis create") {
      return executeGenesisCreate(options);
    }

    if (options.command == "genesis inspect") {
      return executeGenesisInspect(options);
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

    if (options.command == "stake lock" || options.command == "stake deposit" ||
        options.command == "stake top-up" ||
        options.command == "stake unlock" ||
        options.command == "stake withdraw") {
      return executeStakeLock(options);
    }

    if (options.command == "stake status") {
      return executeStakeStatus(options);
    }

    if (options.command == "stake positions") {
      return executeStakePositions(options);
    }

    if (options.command == "stake audit") {
      return executeStakeAudit(options);
    }

    if (options.command == "rewards status") {
      return executeRewardsStatus(options);
    }

    if (options.command == "slashing evidence") {
      return executeSlashingEvidence(options);
    }

    return CommandLineResult::failure(CommandLineStatus::INVALID_ARGUMENTS,
                                      "Unknown command: " + options.command +
                                          "\n\n" + helpText());
  } catch (const std::exception &error) {
    return CommandLineResult::failure(CommandLineStatus::INVALID_ARGUMENTS,
                                      std::string("Invalid command line: ") +
                                          error.what() + "\n\n" + helpText());
  }
}

std::string CommandLineInterface::defaultLocalnetKeyId() {
  return "local-validator";
}

std::string CommandLineInterface::defaultLocalnetKeySeed() {
  return "nodo-localnet-validator-seed";
}

p2p::PeerInfo
CommandLineInterface::localPeerFromOptions(const CommandLineOptions &options) {
  const config::NetworkParameters params = networkParametersForOptions(options);

  return p2p::PeerInfo(options.peerId, options.endpoint,
                       params.protocolVersion(), 0, options.timestamp);
}

CommandLineResult
CommandLineInterface::executeInit(const CommandLineOptions &options) {
  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const config::GenesisConfig genesisConfig = genesisLookup.genesis();

  const node::NodeDataDirectoryInitResult result =
      node::NodeDataDirectory::initialize(
          node::NodeDataDirectoryConfig(options.dataDirectory), genesisConfig,
          localPeerFromOptions(options), options.timestamp);

  if (!result.success()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Failed to initialize Nodo data directory: " + result.reason() + "\n");
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
         << "Latest state root: " << result.manifest().latestStateRoot()
         << "\n";

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeStatus(const CommandLineOptions &options) {
  const node::NodeDataDirectoryReadResult result =
      node::NodeDataDirectory::loadManifest(
          node::NodeDataDirectoryConfig(options.dataDirectory));

  if (!result.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Failed to read Nodo status: " + result.reason() + "\n");
  }

  const node::NodeRuntimeManifest &manifest = result.manifest();

  const std::optional<std::string> mismatch =
      manifestNetworkMismatch(manifest, options);

  if (mismatch.has_value()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      *mismatch + "\n");
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

CommandLineResult
CommandLineInterface::executeInspect(const CommandLineOptions &options) {
  const node::NodeDataDirectoryReadResult result =
      node::NodeDataDirectory::loadManifest(
          node::NodeDataDirectoryConfig(options.dataDirectory));

  if (!result.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Failed to inspect Nodo data directory: " + result.reason() + "\n");
  }

  const std::optional<std::string> mismatch =
      manifestNetworkMismatch(result.manifest(), options);

  if (mismatch.has_value()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      *mismatch + "\n");
  }

  std::ostringstream output;

  output << "Nodo inspect\n"
         << "------------\n"
         << "Data directory: " << options.dataDirectory.string() << "\n"
         << result.manifest().serialize() << "\n";

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeReload(const CommandLineOptions &options) {
  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const config::GenesisConfig genesisConfig = genesisLookup.genesis();

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          node::NodeDataDirectoryConfig(options.dataDirectory), genesisConfig,
          localPeerFromOptions(options));

  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Failed to reload Nodo runtime: " + load.reason() + "\n");
  }

  std::ostringstream output;

  output << "Nodo runtime reloaded.\n"
         << "Data directory: " << options.dataDirectory.string() << "\n"
         << "Latest height: " << load.manifest().latestBlockHeight() << "\n"
         << "Latest hash: " << load.manifest().latestBlockHash() << "\n"
         << "Latest state root: " << load.manifest().latestStateRoot() << "\n"
         << "Loaded finalized blocks: " << load.loadedBlockCount() << "\n"
         << "Loaded mempool transactions: "
         << load.loadedMempoolTransactionCount() << "\n";

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeChainAudit(const CommandLineOptions &options) {
  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const config::GenesisConfig genesisConfig = genesisLookup.genesis();

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesisConfig, localPeerFromOptions(options));

  const node::ChainAuditResult audit = node::ChainAuditor::auditLoadedRuntime(
      load, directoryConfig.epochMonetaryReportPath(),
      directoryConfig.epochTreasuryReportPath());

  if (!audit.passed()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      audit.toHumanReadableString());
  }

  std::ostringstream output;

  output << audit.toHumanReadableString()
         << "Data directory: " << directoryConfig.rootPath().string() << "\n"
         << "Genesis id: " << load.manifest().genesisConfigId() << "\n";

  return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeTestnetReadiness(
    const CommandLineOptions &options) {
  if (config::NetworkProfileRegistry::isOfficialNetwork(options.networkName) &&
      !options.keyIdProvided) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Official network readiness requires --key-id. "
        "Do not default to localnet development keys.\n");
  }

  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const config::GenesisConfig genesisConfig = genesisLookup.genesis();

  const config::NetworkParameters params = genesisConfig.networkParameters();

  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);

  if (manifest.loaded()) {
    const std::optional<std::string> mismatch =
        manifestNetworkMismatch(manifest.manifest(), options);

    if (mismatch.has_value()) {
      return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                        *mismatch + "\n");
    }
  }

  const std::string validatorKeyId =
      options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();

  const crypto::KeyStoreLoadResult key =
      loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), validatorKeyId);

  bool keyPolicyPassed = false;
  std::vector<std::string> warnings;

  if (key.loaded()) {
    const node::KeySafetyCheckResult keySafety =
        node::ProductionKeySafetyGate::check(key.metadata(),
                                             params.networkName());
    keyPolicyPassed = keySafety.isApproved();

    if (!keyPolicyPassed) {
      warnings.push_back(keySafety.reason());
    }
  } else {
    warnings.push_back("Validator key '" + validatorKeyId +
                       "' is not loaded: " + key.reason());
  }

  const bool genesisVerified = true; // resolveAndVerify() already verified

  const std::size_t connectedPeers =
      manifest.loaded() ? manifest.manifest().peerCount() : 0;

  const std::uint64_t finalizedHeight =
      manifest.loaded() ? manifest.manifest().latestBlockHeight() : 0;

  const std::size_t validatorCount =
      manifest.loaded() ? manifest.manifest().validatorCount()
                        : genesisConfig.bootstrapValidators().size();

  // Attempt runtime load and chain audit to derive real chain safety facts.
  bool chainAuditPassed = false;
  bool treasuryReportVerified = false;
  if (manifest.loaded() && finalizedHeight > 0) {
    const node::RuntimeStateLoadResult rtLoad =
        node::RuntimeStateLoader::loadFromDataDirectory(
            directoryConfig, genesisConfig, localPeerFromOptions(options));
    if (rtLoad.loaded()) {
      const node::ChainAuditResult auditResult =
          node::ChainAuditor::auditLoadedRuntime(
              rtLoad, directoryConfig.epochMonetaryReportPath(),
              directoryConfig.epochTreasuryReportPath());
      chainAuditPassed = auditResult.passed();
      treasuryReportVerified = auditResult.passed();
    }
  } else {
    // No finalized blocks yet: chain audit and treasury report are vacuously
    // valid.
    chainAuditPassed = true;
    treasuryReportVerified = true;
  }

  // Build the readiness context from real runtime inspection.
  const crypto::StoredKeyMetadata keyMetadata =
      key.loaded() ? key.metadata() : crypto::StoredKeyMetadata();
  node::ReadinessContextBuilder ctxBuilder(directoryConfig, params,
                                           keyMetadata);
  ctxBuilder.withManifest(manifest)
      .withGenesisFacts(
          genesisVerified,
          // The genesis is either built in or the operator document pinned
          // at init; resolveGenesisForOptions() returned early otherwise.
          config::GenesisRegistry::hasRegisteredGenesis(options.networkName) ||
              config::GenesisRegistry::requiresOperatorGenesis(
                  options.networkName),
          genesisConfig.deterministicId(),
          config::networkClassToString(params.networkClass()))
      .withChainAuditResult(chainAuditPassed, treasuryReportVerified)
      .withSafetyState()
      .withKeyPolicyResult(keyPolicyPassed);

  for (const auto &w : warnings) {
    ctxBuilder.addWarning(w);
  }

  const node::ReadinessContext readinessCtx = ctxBuilder.build();

  // Propagate any safety state warnings back to warnings list.
  for (const auto &w : readinessCtx.warnings) {
    bool alreadyPresent = false;
    for (const auto &existing : warnings) {
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
        readinessCtx.connectedPeers, readinessCtx.genesisVerified,
        readinessCtx.finalizedHeight,
        readinessCtx.governanceLifecycleIntegrated,
        readinessCtx.defenseModeInactive, readinessCtx.legacyCommandsBlocked,
        readinessCtx.treasuryReportVerified,
        readinessCtx.evidenceCaptureHealthy, readinessCtx.chainAuditPassed,
        readinessCtx.genesisRegistered, readinessCtx.networkClass);
    checks = node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key.metadata(), readinessConfig);
  } else {
    checks.emplace_back("validator_key_loaded", false,
                        "Validator key '" + validatorKeyId +
                            "' is missing or unreadable.");
    checks.emplace_back("network_parameters_valid", params.isValid(),
                        params.isValid() ? "Network parameters are valid for " +
                                               params.networkName()
                                         : "Network parameters are invalid.");
    checks.emplace_back("genesis_verified", genesisVerified,
                        genesisVerified ? "Genesis has been verified."
                                        : "Genesis verification failed.");
    checks.emplace_back("peers_connected", connectedPeers > 0,
                        std::to_string(connectedPeers) + " peer(s) connected.");
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
         << "Genesis verified: "
         << (readinessCtx.genesisVerified ? "yes" : "no") << "\n"
         << "Key policy passed: "
         << (readinessCtx.keyPolicyPassed ? "yes" : "no") << "\n"
         << "Defense mode: "
         << (readinessCtx.defenseModeInactive ? "INACTIVE"
                                              : "ACTIVE or UNKNOWN")
         << "\n"
         << "Peers: " << readinessCtx.connectedPeers << "\n"
         << "Validators: " << validatorCount << "\n"
         << "Finalized height: " << readinessCtx.finalizedHeight << "\n"
         << "Readiness: " << node::readinessStatusToString(status) << "\n";

  for (const node::ReadinessDiagnostic &check : checks) {
    output << check.serialize() << "\n";
  }

  for (const std::string &warning : warnings) {
    output << "NOT_READY: " << warning;
    if (warning.empty() || warning.back() != '\n') {
      output << "\n";
    }
  }

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeDiagnostics(const CommandLineOptions &options) {
  if (config::NetworkProfileRegistry::isOfficialNetwork(options.networkName) &&
      !options.keyIdProvided) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Official network diagnostics requires --key-id. "
        "Do not default to localnet development keys.\n");
  }

  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const config::GenesisConfig genesisConfig = genesisLookup.genesis();

  const config::NetworkParameters params = genesisConfig.networkParameters();

  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);

  if (manifest.loaded()) {
    const std::optional<std::string> mismatch =
        manifestNetworkMismatch(manifest.manifest(), options);

    if (mismatch.has_value()) {
      return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                        *mismatch + "\n");
    }
  }

  const std::string validatorKeyId =
      options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();

  const crypto::KeyStoreLoadResult key =
      loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), validatorKeyId);

  bool keyPolicyPassed = false;
  std::vector<std::string> warnings;

  if (key.loaded()) {
    const node::KeySafetyCheckResult keySafety =
        node::ProductionKeySafetyGate::check(key.metadata(),
                                             params.networkName());
    keyPolicyPassed = keySafety.isApproved();
    if (!keyPolicyPassed) {
      warnings.push_back(keySafety.reason());
    }
  } else {
    warnings.push_back("Validator key '" + validatorKeyId +
                       "' is not loaded: " + key.reason());
  }

  if (!manifest.loaded()) {
    warnings.push_back("Node data directory is not initialized: " +
                       manifest.reason());
  }

  const std::string registeredGenesisId = genesisConfig.deterministicId();
  const std::string manifestGenesisId =
      manifest.loaded() ? manifest.manifest().genesisConfigId() : "";
  const bool genesisCompatible =
      manifestGenesisId.empty() || manifestGenesisId == registeredGenesisId;

  const node::OperatorDiagnosticsReport report =
      node::OperatorDiagnostics::collect(
          params, registeredGenesisId, manifestGenesisId,
          config::networkClassToString(params.networkClass()),
          manifest.loaded() ? manifest.manifest().latestBlockHeight() : 0,
          manifest.loaded() ? manifest.manifest().latestBlockHash() : "",
          manifest.loaded() ? manifest.manifest().validatorCount()
                            : genesisConfig.bootstrapValidators().size(),
          manifest.loaded() ? manifest.manifest().peerCount() : 0,
          0,     // inboundPeers not tracked at CLI level
          0,     // outboundPeers not tracked at CLI level
          false, // discoveryActive not tracked at CLI level
          true,  // resolveAndVerify() already verified genesis
          genesisCompatible, keyPolicyPassed,
          "",    // latestImportStatus not tracked at CLI level
          "",    // latestImportRejectionReason
          false, // defenseRestrictionsActive requires runtime
          warnings);

  return CommandLineResult::success(report.serialize() + "\n");
}

CommandLineResult
CommandLineInterface::executeKeysCreate(const CommandLineOptions &options) {
  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);

  // A genesis ceremony needs validator and owner public keys before the
  // genesis exists, so networks that require an operator genesis may create
  // keys in a data directory that has not been initialized yet.
  if (!manifest.loaded() &&
      !config::GenesisRegistry::requiresOperatorGenesis(options.networkName)) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot create key before init: " + manifest.reason() + "\n");
  }

  if (manifest.loaded()) {
    const std::optional<std::string> mismatch =
        manifestNetworkMismatch(manifest.manifest(), options);

    if (mismatch.has_value()) {
      return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                        *mismatch + "\n");
    }
  }

  if (crypto::KeyEncryptionPolicy::isMainnetBlocked(options.networkName)) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot create a key for mainnet: no audited key provider is "
        "available. This will remain blocked until a production HSM/KMS "
        "integration is complete.\n");
  }

  if (options.keyType == "both" && options.keyIdProvided) {
    throw std::invalid_argument(
        "--key-id requires --type user or --type validator.");
  }

  const auto seedFor = [&](const std::string &keyId,
                           crypto::KeyStoreKeyType keyType) {
    if (keyType == crypto::KeyStoreKeyType::VALIDATOR &&
        keyId == defaultLocalnetKeyId()) {
      return defaultLocalnetKeySeed();
    }

    if (keyType == crypto::KeyStoreKeyType::USER &&
        keyId == defaultLocalnetUserKeyId()) {
      return defaultLocalnetUserKeySeed();
    }

    return manifest.manifest().genesisConfigId() + "#" +
           crypto::keyStoreKeyTypeToString(keyType) + "#" + keyId;
  };

  std::string password;
  if (crypto::KeyEncryptionPolicy::isOfficialNetwork(options.networkName)) {
    const char *envVal = std::getenv("NODO_KEY_PASSWORD");
    if (envVal && std::strlen(envVal) > 0) {
      password = envVal;
    } else {
      password = readPasswordNoEcho("Enter password to encrypt private key: ");
      if (password.size() < 8) {
        return CommandLineResult::failure(
            CommandLineStatus::COMMAND_FAILED,
            "Error: Password must be at least 8 characters for official "
            "networks.\n");
      }
      std::string confirm = readPasswordNoEcho("Confirm password: ");
      if (password != confirm) {
        return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                          "Error: Passwords do not match.\n");
      }
    }
  }

  // Official network keys come from the OS CSPRNG. Seed-derived keys are
  // reproducible from public data (the genesis id and key id, or the
  // localnet seeds), so they stay confined to development networks.
  const bool randomKeys =
      crypto::KeyEncryptionPolicy::isOfficialNetwork(options.networkName);

  const auto createKey = [&](const std::string &keyId,
                             crypto::KeyStoreKeyType keyType) {
    if (randomKeys) {
      return crypto::KeyStore::createRandomKey(
          directoryConfig.keysDirectoryPath(), keyId, keyType,
          options.timestamp, password, options.networkName);
    }

    return crypto::KeyStore::createLocalKey(
        directoryConfig.keysDirectoryPath(), keyId, keyType,
        seedFor(keyId, keyType), options.timestamp, password,
        options.networkName);
  };

  std::vector<crypto::KeyStoreCreateResult> createdKeys;

  if (options.keyType == "both") {
    createdKeys.push_back(
        createKey(defaultLocalnetUserKeyId(), crypto::KeyStoreKeyType::USER));
    createdKeys.push_back(
        createKey(defaultLocalnetKeyId(), crypto::KeyStoreKeyType::VALIDATOR));
  } else {
    const crypto::KeyStoreKeyType keyType =
        crypto::keyStoreKeyTypeFromString(options.keyType);

    const std::string keyId = options.keyIdProvided
                                  ? options.keyId
                                  : (keyType == crypto::KeyStoreKeyType::USER
                                         ? defaultLocalnetUserKeyId()
                                         : defaultLocalnetKeyId());

    createdKeys.push_back(createKey(keyId, keyType));
  }

  for (const crypto::KeyStoreCreateResult &created : createdKeys) {
    if (!created.success()) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          "Failed to create key: " + created.reason() + "\n");
    }
  }

  std::ostringstream output;

  if (randomKeys) {
    output << "Nodo key created from the operating system CSPRNG.\n"
           << "Keep the key file and its password private; share only the "
              "public key and address.\n";
  } else {
    output << "Nodo development key created.\n"
           << "WARNING: this key is deterministically derived from the "
              "genesis config id and key id, not real randomness. Do not use "
              "it for custody, production validators, treasury, or mainnet.\n";
  }

  for (const crypto::KeyStoreCreateResult &created : createdKeys) {
    output << "Key id: " << created.metadata().keyId() << "\n"
           << "Key type: "
           << crypto::keyStoreKeyTypeToString(created.metadata().keyType())
           << "\n"
           << "Public key: " << created.metadata().publicKey().keyMaterial()
           << "\n"
           << "Address: " << created.metadata().address() << "\n"
           << "Algorithm: "
           << crypto::cryptoAlgorithmToString(created.metadata().algorithm())
           << "\n"
           << "Provider: " << created.metadata().provider() << "\n"
           << "Network profile: " << created.metadata().networkProfile() << "\n"
           << "Key file: " << created.path().string() << "\n";
  }

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeKeysList(const CommandLineOptions &options) {
  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);

  if (!manifest.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot list keys before init: " + manifest.reason() + "\n");
  }

  const std::optional<std::string> mismatch =
      manifestNetworkMismatch(manifest.manifest(), options);

  if (mismatch.has_value()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      *mismatch + "\n");
  }

  const crypto::KeyStoreListResult listed =
      crypto::KeyStore::listKeys(directoryConfig.keysDirectoryPath());

  if (!listed.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Failed to list keys: " + listed.reason() + "\n");
  }

  std::ostringstream output;

  output << "Nodo keys\n"
         << "---------\n"
         << "Key count: " << listed.keys().size() << "\n";

  for (const crypto::StoredKeyMetadata &key : listed.keys()) {
    output << "Key id: " << key.keyId()
           << " | Type: " << crypto::keyStoreKeyTypeToString(key.keyType())
           << " | Address: " << key.address() << " | Algorithm: "
           << crypto::cryptoAlgorithmToString(key.algorithm())
           << " | Provider: " << key.provider()
           << " | Network profile: " << key.networkProfile() << "\n";
  }

  return CommandLineResult::success(output.str());
}

namespace {

// Splits "LEFT:RIGHT" at the first ':'; both sides must be non-empty.
std::pair<std::string, std::string> splitColonPair(const std::string &option,
                                                   const std::string &value,
                                                   const std::string &shape) {
  const std::size_t separator = value.find(':');
  if (separator == std::string::npos || separator == 0 ||
      separator + 1 >= value.size()) {
    throw std::invalid_argument(option + " must have the form " + shape +
                                ": " + value);
  }
  return {value.substr(0, separator), value.substr(separator + 1)};
}

void describeGenesis(std::ostringstream &output,
                     const config::GenesisConfig &genesis) {
  const config::NetworkParameters &params = genesis.networkParameters();

  output << "Network: " << params.networkName() << "\n"
         << "Chain id: " << params.chainId() << "\n"
         << "Genesis id: " << genesis.deterministicId() << "\n"
         << "Genesis timestamp: " << genesis.genesisTimestamp() << "\n"
         << "Genesis memo: " << genesis.genesisMemo() << "\n"
         << "Bootstrap validators: " << genesis.bootstrapValidators().size()
         << "\n";

  for (std::size_t index = 0; index < genesis.bootstrapValidators().size();
       ++index) {
    const config::BootstrapValidatorConfig &validator =
        genesis.bootstrapValidators()[index];
    output << "  [" << index << "] validator="
           << validator.validatorAddress()
           << " owner=" << validator.effectiveOwnerAddress() << "\n";
  }

  output << "Genesis accounts: " << genesis.genesisAccounts().size() << "\n";

  for (std::size_t index = 0; index < genesis.genesisAccounts().size();
       ++index) {
    const config::GenesisAccountConfig &account =
        genesis.genesisAccounts()[index];
    output << "  [" << index << "] address=" << account.address()
           << " balanceRaw=" << account.balance().rawUnits() << "\n";
  }
}

} // namespace

CommandLineResult
CommandLineInterface::executeGenesisCreate(const CommandLineOptions &options) {
  if (!config::GenesisRegistry::requiresOperatorGenesis(options.networkName)) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "genesis create applies only to networks that require an operator "
        "genesis. Network '" +
            options.networkName + "' uses its built-in genesis.\n");
  }

  if (options.outputPath.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "genesis create requires --output PATH.\n");
  }

  if (options.genesisValidators.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "genesis create requires --genesis-validator "
        "BLS_PUBLIC_KEY_HEX:OWNER_ADDRESS for each bootstrap validator.\n");
  }

  std::error_code existsError;
  if (std::filesystem::exists(options.outputPath, existsError)) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Refusing to overwrite existing file '" + options.outputPath.string() +
            "'.\n");
  }

  const config::NetworkParameters params = networkParametersForOptions(options);

  std::vector<config::BootstrapValidatorConfig> validators;
  for (std::size_t index = 0; index < options.genesisValidators.size();
       ++index) {
    const auto [publicKeyHex, ownerAddress] =
        splitColonPair("--genesis-validator", options.genesisValidators[index],
                       "BLS_PUBLIC_KEY_HEX:OWNER_ADDRESS");
    validators.emplace_back(
        crypto::PublicKey(crypto::CryptoAlgorithm::BLS12_381, publicKeyHex), 1,
        1, params.networkName() + "-genesis-validator-" + std::to_string(index),
        ownerAddress);
  }

  std::vector<config::GenesisAccountConfig> accounts;
  for (const std::string &entry : options.genesisAccounts) {
    const auto [address, balanceText] = splitColonPair(
        "--genesis-account", entry, "ADDRESS:BALANCE_RAW");
    const std::int64_t balanceRaw =
        parseSignedInt64("--genesis-account balance", balanceText);
    if (balanceRaw < 0) {
      throw std::invalid_argument(
          "--genesis-account balance must be non-negative.");
    }
    accounts.emplace_back(address, utils::Amount::fromRawUnits(balanceRaw), 0);
  }

  const std::string memo = options.genesisMemo.empty()
                               ? "nodo-" + params.networkName() + "-genesis"
                               : options.genesisMemo;

  const std::string contents = config::GenesisDocumentCodec::encode(
      config::GenesisConfig(params, options.timestamp, std::move(validators),
                            std::move(accounts), memo));

  // Decoding what we are about to write applies every rule a node enforces
  // when it loads the document: canonical hex, address checksums,
  // duplicates, and the network's minimum validator count.
  config::GenesisConfig genesis;
  try {
    genesis = config::GenesisDocumentCodec::decode(contents);
  } catch (const std::exception &error) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        std::string("Genesis is invalid: ") + error.what() + "\n");
  }

  const core::GenesisVerificationResult verified =
      core::GenesisVerifier::verify(genesis);
  if (!verified.isValid()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Genesis verification failed: " +
            core::genesisVerificationStatusToString(verified.status()) + ": " +
            verified.reason() + "\n");
  }

  storage::AtomicFile::writeTextFile(options.outputPath, contents);

  std::ostringstream output;
  output << "Genesis document written.\n"
         << "File: " << options.outputPath.string() << "\n";
  describeGenesis(output, genesis);
  output << "Next: every operator checks the genesis id with 'nodo genesis "
            "inspect --genesis-file PATH', then runs 'nodo init --network "
         << params.networkName() << " --genesis-file PATH'.\n";

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeGenesisInspect(const CommandLineOptions &options) {
  if (options.genesisFile.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "genesis inspect requires --genesis-file PATH.\n");
  }

  config::GenesisConfig genesis;
  try {
    genesis = config::GenesisDocumentCodec::loadFile(options.genesisFile);
  } catch (const std::exception &error) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Genesis document '" + options.genesisFile.string() +
            "' is invalid: " + error.what() + "\n");
  }

  const core::GenesisVerificationResult verified =
      core::GenesisVerifier::verify(genesis);
  if (!verified.isValid()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Genesis verification failed: " +
            core::genesisVerificationStatusToString(verified.status()) + ": " +
            verified.reason() + "\n");
  }

  std::ostringstream output;
  output << "Genesis document: " << options.genesisFile.string() << "\n";
  describeGenesis(output, genesis);

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeValidatorList(const CommandLineOptions &options) {
  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const config::GenesisConfig genesisConfig = genesisLookup.genesis();

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          node::NodeDataDirectoryConfig(options.dataDirectory), genesisConfig,
          localPeerFromOptions(options));

  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot list validators: " + load.reason() + "\n");
  }

  const std::vector<std::string> validators =
      load.runtime().validatorRegistry().activeValidatorAddresses();

  std::ostringstream output;

  output << "Nodo validators\n"
         << "---------------\n"
         << "Active validators: " << validators.size() << "\n"
         << "Total consensus weight: "
         << load.runtime().validatorRegistry().totalConsensusWeight() << "\n"
         << "Validator set root: "
         << load.runtime().validatorRegistry().validatorSetRoot() << "\n";

  for (const std::string &validator : validators) {
    const core::ValidatorRegistryEntry *entry =
        load.runtime().validatorRegistry().entryForAddress(validator);
    output << "Validator: " << validator;
    if (entry != nullptr) {
      output << " | status="
             << core::validatorRegistrationStatusToString(entry->status())
             << " | stakeRaw=" << entry->stakeAmount()
             << " | weight=" << entry->consensusWeight();
    }
    output << "\n";
  }

  return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeSubmitTransaction(
    const CommandLineOptions &options) {
  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const config::GenesisConfig genesisConfig = genesisLookup.genesis();

  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);

  if (!manifest.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit transaction before init: " + manifest.reason() + "\n");
  }

  const config::NetworkParameters networkParameters =
      genesisConfig.networkParameters();

  if (manifest.manifest().networkName() != networkParameters.networkName()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit transaction: data directory network does not match "
        "selected network parameters.\n");
  }

  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(
          networkParameters.networkName());

  if (!cryptoContext.isValid()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit transaction: invalid crypto context for network " +
            networkParameters.networkName() + ": " +
            cryptoContext.rejectionReason() + "\n");
  }

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesisConfig, localPeerFromOptions(options));

  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit transaction: runtime reload failed before mempool "
        "admission: " +
            load.reason() + "\n");
  }

  const std::string signingKeyId =
      options.keyIdProvided ? options.keyId : defaultLocalnetUserKeyId();

  const crypto::KeyStoreLoadResult key =
      loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), signingKeyId);

  if (!key.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit transaction without local key '" + signingKeyId +
            "': " + key.reason() + "\n");
  }

  const node::KeySafetyCheckResult keySafety =
      node::ProductionKeySafetyGate::check(key.metadata(),
                                           manifest.manifest().networkName());

  if (!keySafety.isApproved()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit transaction: " + keySafety.reason() + "\n");
  }

  const crypto::Ed25519SignatureProvider provider;
  const crypto::Signer signer(key.keyPair(), provider);

  const core::AccountStateView accountState =
      node::RuntimeAccountStateBuilder::accountStateViewAtTip(
          genesisConfig, load.runtime().blockchain(),
          static_cast<std::int64_t>(networkParameters.minimumFeeRawUnits()));

  if (!accountState.hasAccount(key.metadata().address())) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit transaction: signing account does not exist in current "
        "state.\n");
  }

  const std::uint64_t nonce =
      options.nonce == 0
          ? accountState.accountOrDefault(key.metadata().address()).nonce() + 1
          : options.nonce;

  const core::Transaction transaction =
      core::TransactionBuilder::buildSignedTransfer(
          core::TransactionBuildRequest(
              options.toAddress, utils::Amount::fromRawUnits(options.amountRaw),
              utils::Amount::fromRawUnits(options.feeRaw), nonce,
              options.timestamp + 10),
          signer, networkParameters.chainId());

  const node::TransactionAdmissionContext admissionContext(
      accountState, load.runtime().mempool(), load.runtime().stakingRegistry(),
      load.runtime().validatorRegistry(), load.runtime().governanceExecutor(),
      load.runtime().blockchain().size(),
      load.runtime().blockchain().latestBlock().timestamp());

  const node::TransactionAdmissionResult admission =
      node::TransactionAdmissionValidator::validateRuntimeSubmission(
          transaction, key.metadata(), networkParameters, accountState,
          load.runtime().mempool(), cryptoContext.policy(),
          crypto::SecurityContext::USER_TRANSACTION, provider,
          load.runtime().effectiveMinimumFeeRawUnits(), &admissionContext);

  if (!admission.accepted()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Transaction rejected before mempool persistence: " +
            admission.reason() + "\n");
  }

  const node::PersistentMempoolWriteResult persisted =
      node::PersistentMempoolStore::persistTransaction(
          directoryConfig, transaction, key.metadata().publicKey(),
          options.timestamp + 11);

  if (!persisted.success()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Failed to persist transaction: " + persisted.reason() + "\n");
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

CommandLineResult CommandLineInterface::executeGovernancePropose(
    const CommandLineOptions &options) {
  core::GovernanceProposalPayload payload;

  try {
    const core::GovernanceProposalType type =
        parseGovernanceProposalTypeOption(options.governanceProposalType);

    switch (type) {
    case core::GovernanceProposalType::PARAMETER_CHANGE:
      if (options.governanceTarget.empty() || options.governanceValue.empty() ||
          options.governanceEffectiveHeight == 0) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Parameter governance proposal requires --target, --value, and "
            "--effective-height.\n");
      }
      payload = core::GovernanceProposalPayload::parameterChange(
          options.governanceProposalTitle, options.governanceProposalBody,
          options.governanceTarget, options.governanceValue,
          options.governanceEffectiveHeight, 1,
          options.governanceVotingPeriodBlocks);
      break;
    case core::GovernanceProposalType::TREASURY_SPEND:
      if (options.toAddress.empty() || options.amountRaw <= 0) {
        return CommandLineResult::failure(
            CommandLineStatus::INVALID_ARGUMENTS,
            "Treasury spend proposal requires --to and positive --amount.\n");
      }
      payload = core::GovernanceProposalPayload::treasurySpend(
          options.governanceProposalTitle, options.governanceProposalBody,
          options.toAddress, options.amountRaw, 1,
          options.governanceVotingPeriodBlocks);
      break;
    case core::GovernanceProposalType::TEXT:
      payload = core::GovernanceProposalPayload::text(
          options.governanceProposalTitle, options.governanceProposalBody, 1,
          options.governanceVotingPeriodBlocks);
      break;
    }
  } catch (const std::exception &error) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        std::string("Invalid governance proposal options: ") + error.what() +
            "\n");
  }

  std::string serializedPayload;
  try {
    serializedPayload = payload.serialize();
  } catch (const std::exception &error) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        std::string("Invalid governance proposal payload: ") + error.what() +
            "\n");
  }

  std::string reason;
  if (!node::GovernanceExecutor::validateProposalPayload(serializedPayload,
                                                         reason)) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Invalid governance proposal payload: " + reason + "\n");
  }

  return submitSignedTransactionToPersistentMempool(
      options, "Governance proposal", defaultLocalnetUserKeyId(),
      [serializedPayload, feeRaw = options.feeRaw,
       timestamp = options.timestamp](const crypto::Signer &signer,
                                      const std::string &chainId,
                                      std::uint64_t nonce) {
        return core::TransactionBuilder::buildSignedGovernanceProposal(
            serializedPayload, utils::Amount::fromRawUnits(feeRaw), nonce,
            timestamp + 10, signer, chainId);
      });
}

CommandLineResult
CommandLineInterface::executeGovernanceVote(const CommandLineOptions &options) {
  if (options.governanceProposalId.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Governance vote requires --proposal-id.\n");
  }

  if (options.validatorAddress.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Governance vote requires --validator <address>.\n");
  }

  core::GovernanceVoteChoice choice;
  try {
    choice = parseGovernanceVoteChoiceOption(options.governanceVoteChoice);
  } catch (const std::exception &error) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        std::string("Invalid governance vote choice: ") + error.what() + "\n");
  }

  const core::GovernanceVotePayload vote(options.governanceProposalId,
                                         options.validatorAddress, choice);

  if (!vote.isValid()) {
    return CommandLineResult::failure(CommandLineStatus::INVALID_ARGUMENTS,
                                      "Governance vote payload is invalid.\n");
  }

  return submitSignedTransactionToPersistentMempool(
      options, "Governance vote", defaultLocalnetKeyId(),
      [proposalId = options.governanceProposalId, vote, feeRaw = options.feeRaw,
       timestamp = options.timestamp](const crypto::Signer &signer,
                                      const std::string &chainId,
                                      std::uint64_t nonce) {
        return core::TransactionBuilder::buildSignedGovernanceVote(
            proposalId, vote, utils::Amount::fromRawUnits(feeRaw), nonce,
            timestamp + 10, signer, chainId);
      });
}

CommandLineResult CommandLineInterface::executeGovernanceExecute(
    const CommandLineOptions &options) {
  if (options.governanceProposalId.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Governance execute requires --proposal-id.\n");
  }

  // Permissionless by design: execution is a timelock-gated action anyone can
  // trigger once a treasury spend has been approved, mirroring propose (see
  // GovernanceExecutor::proposalReadyForExplicitExecution). It is not a
  // validator-only action like vote, so it defaults to the local user key.
  return submitSignedTransactionToPersistentMempool(
      options, "Governance execute", defaultLocalnetUserKeyId(),
      [proposalId = options.governanceProposalId, feeRaw = options.feeRaw,
       timestamp = options.timestamp](const crypto::Signer &signer,
                                      const std::string &chainId,
                                      std::uint64_t nonce) {
        return core::TransactionBuilder::buildSignedGovernanceExecute(
            proposalId, utils::Amount::fromRawUnits(feeRaw), nonce,
            timestamp + 10, signer, chainId);
      });
}

CommandLineResult CommandLineInterface::executeGovernanceStatus(
    const CommandLineOptions &options) {
  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);
  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          node::NodeDataDirectoryConfig(options.dataDirectory),
          genesisLookup.genesis(), localPeerFromOptions(options));

  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot load governance state: " + load.reason() + "\n");
  }

  const node::GovernanceExecutor &governance =
      load.runtime().governanceExecutor();
  const std::uint64_t nextHeight = load.runtime().blockchain().size();

  std::ostringstream output;
  output << "Nodo governance status\n"
         << "----------------------\n"
         << "Latest height: " << load.manifest().latestBlockHeight() << "\n"
         << "Active proposals: " << governance.activeProposalCount() << "\n"
         << "Approved proposals: " << governance.approvedProposalCount() << "\n"
         << "Executable proposals: "
         << governance.executableProposalCount(nextHeight) << "\n"
         << "Executed proposals: " << governance.executedProposalCount() << "\n"
         << "Effective minimum fee: "
         << load.runtime().effectiveMinimumFeeRawUnits() << "\n";

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeGovernanceList(const CommandLineOptions &options) {
  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);
  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          node::NodeDataDirectoryConfig(options.dataDirectory),
          genesisLookup.genesis(), localPeerFromOptions(options));

  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot load governance state: " + load.reason() + "\n");
  }

  const node::GovernanceExecutor &governance =
      load.runtime().governanceExecutor();
  const std::vector<std::string> ids = governance.proposalIds();

  std::ostringstream output;
  output << "Nodo governance proposals\n"
         << "-------------------------\n"
         << "Count: " << ids.size() << "\n";

  for (const std::string &id : ids) {
    output << "Proposal: " << id << " | Status: "
           << node::governanceProposalStatusToString(
                  governance.proposalStatus(id))
           << " | Voting: " << governance.proposalVotingStartHeight(id) << "-"
           << governance.proposalVotingEndHeight(id) << "\n";
  }

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeGovernanceShow(const CommandLineOptions &options) {
  if (options.governanceProposalId.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Governance show requires --proposal-id.\n");
  }

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);
  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          node::NodeDataDirectoryConfig(options.dataDirectory),
          genesisLookup.genesis(), localPeerFromOptions(options));

  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot load governance state: " + load.reason() + "\n");
  }

  const node::GovernanceExecutor &governance =
      load.runtime().governanceExecutor();

  if (!governance.hasProposal(options.governanceProposalId)) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "Governance proposal not found: " +
                                          options.governanceProposalId + "\n");
  }

  std::ostringstream output;
  output << "Nodo governance proposal\n"
         << "------------------------\n"
         << governance.proposalDetail(options.governanceProposalId) << "\n"
         << governance.proposalVotes(options.governanceProposalId) << "\n"
         << governance.proposalExecutionDetail(options.governanceProposalId)
         << "\n";

  return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeGovernanceAudit(
    const CommandLineOptions &options) {
  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);
  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesisLookup.genesis(),
          localPeerFromOptions(options));

  const node::ChainAuditResult audit = node::ChainAuditor::auditLoadedRuntime(
      load, directoryConfig.epochMonetaryReportPath(),
      directoryConfig.epochTreasuryReportPath());

  if (!audit.passed()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      audit.toHumanReadableString());
  }

  const node::GovernanceExecutor &governance =
      load.runtime().governanceExecutor();
  const std::vector<std::string> ids = governance.proposalIds();
  std::size_t lifecycleRecordsVerified = 0;

  for (const std::string &id : ids) {
    if (governance.proposalDetail(id).empty() ||
        governance.tallyForProposal(id).proposalId() != id) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          "Governance audit failed for proposal: " + id + "\n");
    }

    // Treasury-spend proposals that reached a decision carry a lifecycle
    // record in the economics:: evidence model. It is not persisted
    // separately: it is deterministically rebuilt here from
    // GovernanceExecutor's own replayed state (identical on a node that
    // synced from scratch as on one that decided it live) and independently
    // re-verified with GovernanceLifecycleVerifier, the same verification
    // FinalizedTreasurySectionValidator relies on when a spend executes.
    const node::GovernanceExecutor::GovernanceProposalSnapshot snapshot =
        governance.proposalSnapshot(id);
    const std::optional<economics::GovernanceLifecycleRecord> decided =
        node::GovernanceLifecycleRecordBuilder::buildDecided(snapshot);
    if (!decided.has_value()) {
      continue;
    }

    const economics::GovernanceLifecycleRecord lifecycle =
        snapshot.status == node::GovernanceProposalStatus::EXECUTED
            ? node::GovernanceLifecycleRecordBuilder::buildExecuted(
                  *decided, snapshot.executedAtHeight)
            : *decided;

    const economics::GovernanceLifecycleVerificationResult verification =
        economics::GovernanceLifecycleVerifier::verify(lifecycle);
    if (!verification.verified()) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          "Governance audit failed to verify lifecycle for proposal " + id +
              ": " + verification.reason() + "\n");
    }
    ++lifecycleRecordsVerified;
  }

  std::ostringstream output;
  output << "Nodo governance audit passed.\n"
         << "Data directory: " << directoryConfig.rootPath().string() << "\n"
         << "Latest height: " << load.manifest().latestBlockHeight() << "\n"
         << "Proposal count: " << ids.size() << "\n"
         << "Treasury lifecycle records verified: " << lifecycleRecordsVerified
         << "\n"
         << "Governance state bytes: " << governance.serialize().size() << "\n";

  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeProduceBlock(const CommandLineOptions &options) {
  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const config::GenesisConfig genesisConfig = genesisLookup.genesis();

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesisConfig, localPeerFromOptions(options));

  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot load runtime from data directory: " + load.reason() + "\n");
  }

  node::NodeRuntime runtime = load.runtime();

  if (runtime.mempool().empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot produce block: mempool is empty and the current localnet rule "
        "requires at least one transaction.\n");
  }

  const std::string validatorKeyId =
      options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();

  const crypto::KeyStoreLoadResult key =
      loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), validatorKeyId);

  if (!key.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot sign validator vote without local key '" + validatorKeyId +
            "': " + key.reason() + "\n");
  }

  const node::KeySafetyCheckResult keySafety =
      node::ProductionKeySafetyGate::check(key.metadata(),
                                           load.manifest().networkName());

  if (!keySafety.isApproved()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot sign validator vote: " + keySafety.reason() + "\n");
  }

  const crypto::Bls12381SignatureProvider provider;
  const crypto::Signer signer(key.keyPair(), provider);

  const node::RuntimeBlockPipelineResult pipeline =
      node::RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
          runtime,
          node::RuntimeBlockPipelineConfig(
              static_cast<std::size_t>(
                  genesisConfig.networkParameters().maxTransactionsPerBlock()),
              1, 1, options.timestamp + 20),
          signer, &directoryConfig);

  if (!pipeline.finalized()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Failed to produce finalized block: " + pipeline.reason() + "\n");
  }

  // Persist epoch monetary report after successful finalization.
  // Failure here is a hard error: a persisted block without a verifiable
  // monetary report is not acceptable in the normal production path.
  {
    utils::Amount genesisSupply;
    try {
      genesisSupply = node::MonetaryFirewall::genesisSupply(genesisConfig);
    } catch (const std::exception &e) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          std::string("Block persisted but genesis supply unavailable for "
                      "monetary report: ") +
              e.what() + "\n");
    }

    const economics::MonetaryPolicy reportPolicy =
        economics::MonetaryPolicy::localnetDefault(
            genesisConfig.networkParameters().chainId(), genesisSupply);

    const auto reportResult =
        node::RuntimeMonetaryReportService::buildAndPersist(
            reportPolicy, runtime.supplyState().finalizedDeltas(), 0,
            directoryConfig.epochMonetaryReportPath());

    if (!reportResult.succeeded()) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          "Block persisted but monetary report persistence failed: " +
              reportResult.reason() + "\n");
    }
  }

  // Persist epoch treasury report derived from the just-persisted artifact.
  // The artifact is reloaded from disk to validate the round-trip and derive
  // the treasury report from its actual treasury section (not a placeholder).
  {
    node::FinalizedBlockArtifact persistedArtifact;
    try {
      persistedArtifact =
          node::FinalizedBlockArtifactCodec::readBlockArtifactFile(
              node::FinalizedBlockStore::blockFilePath(
                  directoryConfig, pipeline.block().index()));
    } catch (const std::exception &e) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          std::string("Block persisted but artifact reload failed for treasury "
                      "report: ") +
              e.what() + "\n");
    }

    const node::FinalizedTreasuryAuditResult treasuryAudit =
        node::FinalizedTreasuryAudit::auditArtifacts(0, {persistedArtifact});

    if (!treasuryAudit.passed()) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          "Block persisted but treasury audit failed: " +
              treasuryAudit.reason() + "\n");
    }

    try {
      node::EpochTreasuryReportStore::write(
          directoryConfig.epochTreasuryReportPath(),
          treasuryAudit.rebuiltReport());
    } catch (const std::exception &e) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          std::string(
              "Block persisted but treasury report persistence failed: ") +
              e.what() + "\n");
    }
  }

  const node::NodeRuntimeManifest manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig).manifest();

  std::ostringstream output;

  output << "Nodo block finalized and persisted.\n"
         << "Block height: " << pipeline.block().index() << "\n"
         << "Block hash: " << pipeline.block().hash() << "\n"
         << "Transactions finalized: "
         << pipeline.finalizedTransactionIds().size() << "\n"
         << "Persistent mempool updated by finalization commit.\n"
         << "Block file: "
         << node::FinalizedBlockStore::blockFilePath(directoryConfig,
                                                     pipeline.block().index())
                .string()
         << "\n"
         << "Manifest latest height: " << manifest.latestBlockHeight() << "\n"
         << "Manifest latest state root: " << manifest.latestStateRoot()
         << "\n";

  return CommandLineResult::success(output.str());
}

// ---------------------------------------------------------------------------
// node run — long-running daemon
// ---------------------------------------------------------------------------

namespace {

// SIGINT/SIGTERM flag — set from signal handler, polled by runBlocking loop.
std::atomic<bool> g_daemonShutdownRequested{false};

node::NodeDaemonPeerEntry parsePeerEntry(const std::string &raw) {
  // Format: NAME@HOST:PORT
  const auto atPos = raw.find('@');
  if (atPos == std::string::npos) {
    throw std::invalid_argument("Invalid --peer value '" + raw +
                                "': expected NAME@HOST:PORT");
  }
  const std::string nodeId = raw.substr(0, atPos);
  const std::string hostPort = raw.substr(atPos + 1);
  const auto colonPos = hostPort.rfind(':');
  if (colonPos == std::string::npos) {
    throw std::invalid_argument("Invalid --peer value '" + raw +
                                "': missing port in HOST:PORT");
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
    throw std::invalid_argument("Invalid --peer port in '" + raw + "'");
  }

  node::NodeDaemonPeerEntry entry;
  entry.nodeId = nodeId;
  entry.host = host;
  entry.port = port;
  return entry;
}

} // namespace


namespace {

node::NodePruningConfig pruningConfigFromOptions(
    const CommandLineOptions& options
) {
  std::string mode = options.pruningMode;
  for (char& c : mode) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }

  if (mode == "archive") {
    return node::NodePruningConfig::archiveMode();
  }
  if (mode == "full") {
    return node::NodePruningConfig::fullMode(
        static_cast<std::size_t>(options.pruningRetainEpochs));
  }
  if (mode == "light") {
    return node::NodePruningConfig::lightMode();
  }
  throw std::invalid_argument("unsupported pruning mode: " + options.pruningMode);
}

} // namespace

CommandLineResult CommandLineInterface::executeNodePrune(
    const CommandLineOptions &options) {
  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);
  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);

  if (!manifest.loaded()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      manifest.reason() + "\n");
  }

  const node::NodePruningConfig pruningConfig = pruningConfigFromOptions(options);
  const node::NodePruningResult result = node::NodePruningService::apply(
      directoryConfig, manifest.manifest(), pruningConfig, nowUnixSeconds());

  if (!result.success()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      result.reason() + "\n");
  }

  std::ostringstream output;
  output << "Pruning: " << node::nodePruningStatusToString(result.status()) << "\n"
         << "Message: " << result.reason() << "\n";
  if (result.manifest().has_value()) {
    output << "Manifest: " << result.manifest()->serialize() << "\n";
  }
  if (result.plan().has_value()) {
    output << "Plan: " << result.plan()->serialize() << "\n";
  }
  return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeNodePruningStatus(
    const CommandLineOptions &options) {
  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);
  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);

  if (!manifest.loaded()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      manifest.reason() + "\n");
  }

  const std::optional<node::NodePruningManifest> pruningManifest =
      node::NodePruningService::loadManifest(directoryConfig);

  std::ostringstream output;
  output << "Data directory: " << directoryConfig.rootPath().string() << "\n"
         << "Chain height: " << manifest.manifest().latestBlockHeight() << "\n";

  if (!pruningManifest.has_value()) {
    const node::NodePruningPlan plan = node::NodePruningService::buildPlan(
        directoryConfig, manifest.manifest(), node::NodePruningConfig::archiveMode());
    output << "Pruning mode: ARCHIVE (implicit)\n"
           << "Plan: " << plan.serialize() << "\n";
    return CommandLineResult::success(output.str());
  }

  output << "Pruning manifest: " << pruningManifest->serialize() << "\n";
  const node::NodePruningPlan plan = node::NodePruningService::buildPlan(
      directoryConfig, manifest.manifest(), pruningManifest->config());
  output << "Current plan: " << plan.serialize() << "\n";
  return CommandLineResult::success(output.str());
}

CommandLineResult
CommandLineInterface::executeNodeRun(const CommandLineOptions &options) {
  // Security gate: reject mainnet.
  const config::NetworkParameters params = networkParametersForOptions(options);

  if (params.networkClass() == config::NetworkClass::LOCKED_PRODUCTION) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "node run: mainnet (locked production) "
                                      "is not supported in this build.\n");
  }

  // Resolve and verify genesis — refuse to start with an unknown genesis.
  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);

  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "node run: " + genesisLookup.reason() +
                                          "\n");
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
            "node run: data directory belongs to a different genesis: " +
                *mismatch + "\n");
      }
    }
  }

  // Route through the same crypto policy gate every other signing command
  // uses, instead of hardcoding providers/policy directly.
  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(params.networkName());

  if (!cryptoContext.isValid()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "node run: invalid crypto context for network " +
            params.networkName() + ": " + cryptoContext.rejectionReason() +
            "\n");
  }

  // cryptoContext (and therefore blsProvider) must outlive the Signer and
  // the daemon.
  const crypto::SignatureProvider &blsProvider =
      cryptoContext.validatorSignatureProvider();
  const crypto::CryptoPolicy policy = cryptoContext.policy();

  // Load optional validator key (for signing blocks / votes).
  std::optional<crypto::Signer> localSigner;
  std::string localValidatorAddress;

  const std::string validatorKeyId =
      options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();

  const crypto::KeyStoreLoadResult key =
      loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), validatorKeyId);

  if (key.loaded()) {
    const node::KeySafetyCheckResult keySafety =
        node::ProductionKeySafetyGate::check(key.metadata(),
                                             params.networkName());

    if (!keySafety.isApproved()) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          "node run: key safety check failed: " + keySafety.reason() + "\n");
    }

    localSigner.emplace(key.keyPair(), blsProvider);
    localValidatorAddress = localSigner->address();
  }

  const crypto::KeyStoreLoadResult nodeIdentityKey = loadKeyWithPrompt(
      directoryConfig.keysDirectoryPath(), options.identityKeyId);
  if (!nodeIdentityKey.loaded() ||
      nodeIdentityKey.metadata().keyType() != crypto::KeyStoreKeyType::USER ||
      nodeIdentityKey.keyPair().algorithm() !=
          crypto::CryptoAlgorithm::CLASSIC_ED25519) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "node run: an Ed25519 local-user key is required for signed peer "
        "identity. "
        "Run 'nodo keys create --type both' for this data directory.\n");
  }
  const node::KeySafetyCheckResult nodeKeySafety =
      node::ProductionKeySafetyGate::check(nodeIdentityKey.metadata(),
                                           params.networkName());
  if (!nodeKeySafety.isApproved()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "node run: node identity key safety check failed: " +
            nodeKeySafety.reason() + "\n");
  }

  // Parse static peers.
  std::vector<node::NodeDaemonPeerEntry> staticPeers;
  for (const auto &rawPeer : options.peers) {
    try {
      staticPeers.push_back(parsePeerEntry(rawPeer));
    } catch (const std::exception &e) {
      return CommandLineResult::failure(CommandLineStatus::INVALID_ARGUMENTS,
                                        std::string("node run: ") + e.what() +
                                            "\n");
    }
  }

  // Build daemon config.
  const node::NodeOrchestratorConfig orchestratorConfig(
      genesisConfig, directoryConfig, localPeerFromOptions(options),
      localValidatorAddress, options.rpcPort, options.rpcBindAddress, 100,
      static_cast<std::size_t>(
          genesisConfig.networkParameters().maxTransactionsPerBlock()));

  node::NodeDaemonConfig daemonConfig;
  daemonConfig.orchestratorConfig = orchestratorConfig;
  daemonConfig.staticPeers = std::move(staticPeers);

  node::NodeDaemon daemon(daemonConfig, policy, blsProvider);

  daemon.setLocalNodeIdentity(nodeIdentityKey.keyPair());

  if (localSigner.has_value()) {
    daemon.setLocalSigner(std::move(*localSigner));
  }

  // Set up SIGINT/SIGTERM handler.
  g_daemonShutdownRequested.store(false);
  std::signal(SIGINT, [](int) { g_daemonShutdownRequested.store(true); });
  std::signal(SIGTERM, [](int) { g_daemonShutdownRequested.store(true); });

  const auto startResult = daemon.start();
  if (!startResult.running()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "node run: failed to start daemon: " + startResult.reason + "\n");
  }

  std::cout << "Nodo daemon running on " << options.endpoint
            << " (network: " << options.networkName << ")\n"
            << "RPC listening on " << options.rpcBindAddress << ":"
            << options.rpcPort << "\n"
            << "Press Ctrl+C to stop.\n";
  std::cout.flush();

  // Poll g_daemonShutdownRequested in the tick loop instead of runBlocking().
  while (!g_daemonShutdownRequested.load() && daemon.isRunning()) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    daemon.tick(static_cast<std::int64_t>(now));

    std::this_thread::sleep_for(
        std::chrono::milliseconds(node::NodeDaemon::DEFAULT_TICK_INTERVAL_MS));
  }

  daemon.stop();

  return CommandLineResult::success("Nodo daemon stopped.\n");
}

// ---------------------------------------------------------------------------
// Validator lifecycle + staking CLI commands
// ---------------------------------------------------------------------------

CommandLineResult CommandLineInterface::executeValidatorStatus(
    const CommandLineOptions &options) {
  const std::string addr =
      options.validatorAddress.empty()
          ? (options.toAddress == "nodo-localnet-recipient" ? ""
                                                            : options.toAddress)
          : options.validatorAddress;

  if (addr.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Provide --validator <address> to inspect a validator.\n");
  }

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);
  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          node::NodeDataDirectoryConfig(options.dataDirectory),
          genesisLookup.genesis(), localPeerFromOptions(options));

  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot load runtime state: " + load.reason() + "\n");
  }

  const core::ValidatorRegistryEntry *entry =
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
        << "Consensus weight: " << entry->consensusWeight() << "\n"
        << "Activation epoch: " << entry->registrationRecord().activationEpoch()
        << "\n"
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

CommandLineResult
CommandLineInterface::executeValidatorExit(const CommandLineOptions &options) {
  const std::string validatorAddr = options.validatorAddress.empty()
                                        ? options.toAddress
                                        : options.validatorAddress;

  if (validatorAddr.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Provide --validator <address> to request exit for.\n");
  }

  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);
  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }
  const config::GenesisConfig genesisConfig = genesisLookup.genesis();
  const config::NetworkParameters networkParameters =
      genesisConfig.networkParameters();

  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);
  if (!manifest.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit before init: " + manifest.reason() + "\n");
  }

  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(
          networkParameters.networkName());
  if (!cryptoContext.isValid()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Invalid crypto context: " + cryptoContext.rejectionReason() + "\n");
  }

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesisConfig, localPeerFromOptions(options));
  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Runtime reload failed: " + load.reason() + "\n");
  }

  const std::string keyId =
      options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();
  const crypto::KeyStoreLoadResult key =
      loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), keyId);
  if (!key.loaded()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "Cannot load key '" + keyId +
                                          "': " + key.reason() + "\n");
  }

  const node::KeySafetyCheckResult keySafety =
      node::ProductionKeySafetyGate::check(key.metadata(),
                                           manifest.manifest().networkName());
  if (!keySafety.isApproved()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "Key safety: " + keySafety.reason() +
                                          "\n");
  }

  const crypto::Ed25519SignatureProvider provider;
  const crypto::Signer signer(key.keyPair(), provider);

  const core::AccountStateView accountState =
      node::RuntimeAccountStateBuilder::accountStateViewAtTip(
          genesisConfig, load.runtime().blockchain(),
          static_cast<std::int64_t>(networkParameters.minimumFeeRawUnits()));
  if (!accountState.hasAccount(key.metadata().address())) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Signing account does not exist in current state.\n");
  }

  const std::uint64_t nextNonce =
      options.nonce == 0
          ? accountState.accountOrDefault(key.metadata().address()).nonce() + 1
          : options.nonce;

  const core::Transaction tx =
      core::TransactionBuilder::buildSignedValidatorExitRequest(
          core::TransactionBuildRequest(
              validatorAddr, utils::Amount(),
              utils::Amount::fromRawUnits(options.feeRaw), nextNonce,
              options.timestamp + 10),
          signer, networkParameters.chainId());

  const node::TransactionAdmissionContext admissionContext(
      accountState, load.runtime().mempool(), load.runtime().stakingRegistry(),
      load.runtime().validatorRegistry(), load.runtime().governanceExecutor(),
      load.runtime().blockchain().size(),
      load.runtime().blockchain().latestBlock().timestamp());

  const node::TransactionAdmissionResult admission =
      node::TransactionAdmissionValidator::validateRuntimeSubmission(
          tx, key.metadata(), networkParameters, accountState,
          load.runtime().mempool(), cryptoContext.policy(),
          crypto::SecurityContext::USER_TRANSACTION, provider,
          load.runtime().effectiveMinimumFeeRawUnits(), &admissionContext);
  if (!admission.accepted()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Transaction rejected: " + admission.reason() + "\n");
  }

  const node::PersistentMempoolWriteResult persisted =
      node::PersistentMempoolStore::persistTransaction(
          directoryConfig, tx, key.metadata().publicKey(), tx.timestamp() + 1);
  if (!persisted.success()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Failed to persist: " + persisted.reason() + "\n");
  }

  std::ostringstream out;
  out << "Validator exit request submitted.\n"
      << "Validator: " << validatorAddr << "\n"
      << "Transaction id: " << persisted.transactionId() << "\n";
  return CommandLineResult::success(out.str());
}

CommandLineResult CommandLineInterface::executeValidatorUnjail(
    const CommandLineOptions &options) {
  const std::string validatorAddr = options.validatorAddress.empty()
                                        ? options.toAddress
                                        : options.validatorAddress;

  if (validatorAddr.empty()) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Provide --validator <address> to unjail.\n");
  }

  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  const config::GenesisLookupResult genesisLookup =
      resolveGenesisForOptions(options);
  if (!genesisLookup.found()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      genesisLookup.reason() + "\n");
  }
  const config::GenesisConfig genesisConfig = genesisLookup.genesis();
  const config::NetworkParameters networkParameters =
      genesisConfig.networkParameters();

  const node::NodeDataDirectoryReadResult manifest =
      node::NodeDataDirectory::loadManifest(directoryConfig);
  if (!manifest.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Cannot submit before init: " + manifest.reason() + "\n");
  }

  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(
          networkParameters.networkName());
  if (!cryptoContext.isValid()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Invalid crypto context: " + cryptoContext.rejectionReason() + "\n");
  }

  const node::RuntimeStateLoadResult load =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesisConfig, localPeerFromOptions(options));
  if (!load.loaded()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Runtime reload failed: " + load.reason() + "\n");
  }

  const std::string keyId =
      options.keyIdProvided ? options.keyId : defaultLocalnetKeyId();
  const crypto::KeyStoreLoadResult key =
      loadKeyWithPrompt(directoryConfig.keysDirectoryPath(), keyId);
  if (!key.loaded()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "Cannot load key '" + keyId +
                                          "': " + key.reason() + "\n");
  }

  const node::KeySafetyCheckResult keySafety =
      node::ProductionKeySafetyGate::check(key.metadata(),
                                           manifest.manifest().networkName());
  if (!keySafety.isApproved()) {
    return CommandLineResult::failure(CommandLineStatus::COMMAND_FAILED,
                                      "Key safety: " + keySafety.reason() +
                                          "\n");
  }

  const crypto::Ed25519SignatureProvider provider;
  const crypto::Signer signer(key.keyPair(), provider);

  const core::AccountStateView accountState =
      node::RuntimeAccountStateBuilder::accountStateViewAtTip(
          genesisConfig, load.runtime().blockchain(),
          static_cast<std::int64_t>(networkParameters.minimumFeeRawUnits()));
  if (!accountState.hasAccount(key.metadata().address())) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Signing account does not exist in current state.\n");
  }

  const std::uint64_t nextNonce =
      options.nonce == 0
          ? accountState.accountOrDefault(key.metadata().address()).nonce() + 1
          : options.nonce;

  const core::Transaction tx =
      core::TransactionBuilder::buildSignedValidatorUnjailRequest(
          core::TransactionBuildRequest(
              validatorAddr, utils::Amount(),
              utils::Amount::fromRawUnits(options.feeRaw), nextNonce,
              options.timestamp + 10),
          signer, networkParameters.chainId());

  const node::TransactionAdmissionContext admissionContext(
      accountState, load.runtime().mempool(), load.runtime().stakingRegistry(),
      load.runtime().validatorRegistry(), load.runtime().governanceExecutor(),
      load.runtime().blockchain().size(),
      load.runtime().blockchain().latestBlock().timestamp());

  const node::TransactionAdmissionResult admission =
      node::TransactionAdmissionValidator::validateRuntimeSubmission(
          tx, key.metadata(), networkParameters, accountState,
          load.runtime().mempool(), cryptoContext.policy(),
          crypto::SecurityContext::USER_TRANSACTION, provider,
          load.runtime().effectiveMinimumFeeRawUnits(), &admissionContext);
  if (!admission.accepted()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Transaction rejected: " + admission.reason() + "\n");
  }

  const node::PersistentMempoolWriteResult persisted =
      node::PersistentMempoolStore::persistTransaction(
          directoryConfig, tx, key.metadata().publicKey(), tx.timestamp() + 1);
  if (!persisted.success()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Failed to persist: " + persisted.reason() + "\n");
  }

  std::ostringstream out;
  out << "Validator unjail request submitted.\n"
      << "Validator: " << validatorAddr << "\n"
      << "Transaction id: " << persisted.transactionId() << "\n";
  return CommandLineResult::success(out.str());
}

} // namespace nodo::app
