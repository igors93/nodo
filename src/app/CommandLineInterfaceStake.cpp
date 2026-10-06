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

std::string defaultLocalnetUserKeyId() { return "local-user"; }

config::GenesisLookupResult
resolveGenesisForOptions(const CommandLineOptions &options) {
  return node::RuntimeStartupService::resolveAndVerify(
      options.networkName, options.dataDirectory, options.genesisFile);
}

} // namespace

CommandLineResult
CommandLineInterface::executeStakeLock(const CommandLineOptions &options) {
  std::string validatorAddr = options.validatorAddress;

  if (validatorAddr.empty() && !options.validatorKeyIdProvided) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Provide --validator <address> or --validator-key <id> for the stake "
        "operation.\n");
  }

  if (options.amountRaw <= 0) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Provide --amount or --stake (positive integer raw units).\n");
  }

  const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);

  if (options.validatorKeyIdProvided) {
    const crypto::KeyStoreLoadResult validatorKey = crypto::KeyStore::loadKey(
        directoryConfig.keysDirectoryPath(), options.validatorKeyId, "", true);
    if (!validatorKey.loaded() || validatorKey.metadata().keyType() !=
                                      crypto::KeyStoreKeyType::VALIDATOR) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          "Cannot resolve validator key '" + options.validatorKeyId + "': " +
              (validatorKey.loaded() ? "key is not a validator identity"
                                     : validatorKey.reason()) +
              "\n");
    }
    if (!validatorAddr.empty() &&
        validatorAddr != validatorKey.metadata().address()) {
      return CommandLineResult::failure(
          CommandLineStatus::INVALID_ARGUMENTS,
          "--validator and --validator-key resolve to different validator "
          "addresses.\n");
    }
    validatorAddr = validatorKey.metadata().address();
  }

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
      options.keyIdProvided ? options.keyId : defaultLocalnetUserKeyId();
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

  const core::TransactionBuildRequest stakeRequest(
      validatorAddr, utils::Amount::fromRawUnits(options.amountRaw),
      utils::Amount::fromRawUnits(options.feeRaw), nextNonce,
      options.timestamp + 10);
  core::Transaction tx =
      options.command == "stake top-up"
          ? core::TransactionBuilder::buildSignedStakeTopUp(
                stakeRequest, signer, networkParameters.chainId())
      : options.command == "stake unlock"
          ? core::TransactionBuilder::buildSignedStakeUnlock(
                stakeRequest, signer, networkParameters.chainId())
      : options.command == "stake withdraw"
          ? core::TransactionBuilder::buildSignedStakeWithdraw(
                stakeRequest, signer, networkParameters.chainId())
          : core::TransactionBuilder::buildSignedStakeDeposit(
                stakeRequest, signer, networkParameters.chainId());

  const node::TransactionAdmissionContext admissionContext(
      accountState, load.runtime().mempool(), load.runtime().stakingRegistry(),
      load.runtime().validatorRegistry(), load.runtime().governanceExecutor(),
      load.runtime().blockchain().size());

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
  const std::string label = options.command == "stake top-up"   ? "Stake top-up"
                            : options.command == "stake unlock" ? "Stake unlock"
                            : options.command == "stake withdraw"
                                ? "Stake withdraw"
                            : options.command == "stake lock" ? "Stake lock"
                                                              : "Stake deposit";
  if (options.outputJson) {
    out << "{\n"
        << "  \"command\": \"" << label << "\",\n"
        << "  \"validator\": \"" << validatorAddr << "\",\n"
        << "  \"amount\": " << options.amountRaw << ",\n"
        << "  \"type\": \"" << core::transactionTypeToString(tx.type())
        << "\",\n"
        << "  \"transactionId\": \"" << persisted.transactionId() << "\"\n"
        << "}\n";
  } else {
    out << label << " submitted.\n"
        << "Validator: " << validatorAddr << "\n"
        << "Amount: " << options.amountRaw << " raw units\n"
        << "Type: " << core::transactionTypeToString(tx.type()) << "\n"
        << "Transaction id: " << persisted.transactionId() << "\n";
  }
  return CommandLineResult::success(out.str());
}

CommandLineResult
CommandLineInterface::executeStakeStatus(const CommandLineOptions &options) {
  std::string addr = options.validatorAddress;

  if (addr.empty() && !options.validatorKeyIdProvided) {
    return CommandLineResult::failure(
        CommandLineStatus::INVALID_ARGUMENTS,
        "Provide --validator <address> or --validator-key <id> to inspect "
        "stake for.\n");
  }

  if (options.validatorKeyIdProvided) {
    const node::NodeDataDirectoryConfig directoryConfig(options.dataDirectory);
    const crypto::KeyStoreLoadResult validatorKey = crypto::KeyStore::loadKey(
        directoryConfig.keysDirectoryPath(), options.validatorKeyId, "", true);
    if (!validatorKey.loaded() || validatorKey.metadata().keyType() !=
                                      crypto::KeyStoreKeyType::VALIDATOR) {
      return CommandLineResult::failure(
          CommandLineStatus::COMMAND_FAILED,
          "Cannot resolve validator key '" + options.validatorKeyId + "': " +
              (validatorKey.loaded() ? "key is not a validator identity"
                                     : validatorKey.reason()) +
              "\n");
    }
    if (!addr.empty() && addr != validatorKey.metadata().address()) {
      return CommandLineResult::failure(
          CommandLineStatus::INVALID_ARGUMENTS,
          "--validator and --validator-key resolve to different validator "
          "addresses.\n");
    }
    addr = validatorKey.metadata().address();
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
  if (options.outputJson) {
    out << "{\n"
        << "  \"validator\": \"" << addr << "\",\n";
    if (!entry) {
      out << "  \"found\": false\n"
          << "}\n";
    } else {
      const auto stakeAccount =
          load.runtime().stakingRegistry().accountOrDefault(addr);
      out << "  \"found\": true,\n"
          << "  \"registryStatus\": \""
          << core::validatorRegistrationStatusToString(entry->status())
          << "\",\n"
          << "  \"registryStake\": " << entry->stakeAmount() << ",\n"
          << "  \"consensusWeight\": " << entry->consensusWeight() << ",\n"
          << "  \"bondedStake\": " << stakeAccount.bondedAmount().rawUnits()
          << ",\n"
          << "  \"activeStake\": "
          << load.runtime().stakingRegistry().activeStakeFor(addr).rawUnits()
          << ",\n"
          << "  \"slashedStake\": " << stakeAccount.slashedAmount().rawUnits()
          << ",\n"
          << "  \"jailed\": " << (stakeAccount.jailed() ? "true" : "false")
          << ",\n"
          << "  \"tombstoned\": "
          << (stakeAccount.tombstoned() ? "true" : "false") << "\n"
          << "}\n";
    }
    return CommandLineResult::success(out.str());
  }

  out << "Stake status\n"
      << "------------\n"
      << "Validator: " << addr << "\n";

  if (!entry) {
    out << "No validator registration found for this address.\n";
  } else {
    const auto stakeAccount =
        load.runtime().stakingRegistry().accountOrDefault(addr);
    out << "Registry status: "
        << core::validatorRegistrationStatusToString(entry->status()) << "\n"
        << "Registry active stake (raw units): " << entry->stakeAmount() << "\n"
        << "Consensus weight: " << entry->consensusWeight() << "\n"
        << "Bonded stake (raw units): "
        << stakeAccount.bondedAmount().rawUnits() << "\n"
        << "Active stake (raw units): "
        << load.runtime().stakingRegistry().activeStakeFor(addr).rawUnits()
        << "\n"
        << "Slashed stake (raw units): "
        << stakeAccount.slashedAmount().rawUnits() << "\n"
        << "Jailed: " << (stakeAccount.jailed() ? "yes" : "no") << "\n"
        << "Tombstoned: " << (stakeAccount.tombstoned() ? "yes" : "no") << "\n";
    const auto positions = load.runtime().stakingRegistry().positions();
    for (const auto &position : positions) {
      if (position.validatorAddress != addr)
        continue;
      out << "Position: " << position.positionId << "\n"
          << "  Owner: " << position.ownerAddress << "\n"
          << "  Status: " << node::stakePositionStatusToString(position.status)
          << "\n"
          << "  Pending activation: "
          << position.pendingActivationAmount.rawUnits() << "\n"
          << "  Pending unbonding: "
          << position.pendingUnbondingAmount.rawUnits() << "\n"
          << "  Withdrawable height: " << position.withdrawableHeight << "\n"
          << "  Withdrawn: " << position.withdrawnAmount.rawUnits() << "\n";
    }
  }

  return CommandLineResult::success(out.str());
}

CommandLineResult
CommandLineInterface::executeStakePositions(const CommandLineOptions &options) {
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

  const std::string owner =
      options.toAddress == "nodo-localnet-recipient" ? "" : options.toAddress;
  const std::string validator = options.validatorAddress;
  const auto positions =
      owner.empty() ? load.runtime().stakingRegistry().positions()
                    : load.runtime().stakingRegistry().positionsForOwner(owner);

  std::ostringstream out;
  if (options.outputJson) {
    out << "{\n"
        << "  \"positions\": [\n";
    bool first = true;
    for (const auto &position : positions) {
      if (!validator.empty() && position.validatorAddress != validator)
        continue;
      if (!first)
        out << ",\n";
      out << "    {\n"
          << "      \"positionId\": \"" << position.positionId << "\",\n"
          << "      \"owner\": \"" << position.ownerAddress << "\",\n"
          << "      \"validator\": \"" << position.validatorAddress << "\",\n"
          << "      \"status\": \""
          << node::stakePositionStatusToString(position.status) << "\",\n"
          << "      \"active\": " << position.activeAmount.rawUnits() << ",\n"
          << "      \"pendingActivation\": "
          << position.pendingActivationAmount.rawUnits() << ",\n"
          << "      \"pendingUnbonding\": "
          << position.pendingUnbondingAmount.rawUnits() << ",\n"
          << "      \"withdrawn\": " << position.withdrawnAmount.rawUnits()
          << ",\n"
          << "      \"slashed\": " << position.slashedAmount.rawUnits() << ",\n"
          << "      \"activationHeight\": " << position.activationHeight
          << ",\n"
          << "      \"withdrawableHeight\": " << position.withdrawableHeight
          << "\n"
          << "    }";
      first = false;
    }
    out << "\n  ]\n}\n";
    return CommandLineResult::success(out.str());
  }

  out << "Stake positions\n"
      << "---------------\n";
  std::size_t count = 0;
  for (const auto &position : positions) {
    if (!validator.empty() && position.validatorAddress != validator)
      continue;
    ++count;
    out << "Position: " << position.positionId << "\n"
        << "Owner: " << position.ownerAddress << "\n"
        << "Validator: " << position.validatorAddress << "\n"
        << "Status: " << node::stakePositionStatusToString(position.status)
        << "\n"
        << "Active: " << position.activeAmount.rawUnits() << "\n"
        << "Pending activation: " << position.pendingActivationAmount.rawUnits()
        << "\n"
        << "Pending unbonding: " << position.pendingUnbondingAmount.rawUnits()
        << "\n"
        << "Withdrawn: " << position.withdrawnAmount.rawUnits() << "\n"
        << "Slashed: " << position.slashedAmount.rawUnits() << "\n"
        << "Activation height: " << position.activationHeight << "\n"
        << "Withdrawable height: " << position.withdrawableHeight << "\n";
  }
  out << "Count: " << count << "\n";
  return CommandLineResult::success(out.str());
}

CommandLineResult
CommandLineInterface::executeStakeAudit(const CommandLineOptions &options) {
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
  if (!load.runtime().stakingRegistry().isValid()) {
    return CommandLineResult::failure(
        CommandLineStatus::COMMAND_FAILED,
        "Staking audit failed: staking registry invariants are invalid.\n");
  }
  std::ostringstream out;
  out << "Staking audit passed.\n"
      << "Stake accounts: "
      << load.runtime().stakingRegistry().accounts().size() << "\n"
      << "Stake positions: "
      << load.runtime().stakingRegistry().positions().size() << "\n"
      << "Lifecycle records: "
      << load.runtime().stakingRegistry().lifecycleRecords().size() << "\n";
  return CommandLineResult::success(out.str());
}

CommandLineResult
CommandLineInterface::executeRewardsStatus(const CommandLineOptions &options) {
  const std::string validatorAddr = options.validatorAddress.empty()
                                        ? options.toAddress
                                        : options.validatorAddress;

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

  const core::Blockchain &blockchain = load.runtime().blockchain();
  const std::uint64_t chainHeight =
      blockchain.empty() ? 0u : blockchain.latestBlock().index();
  const std::vector<node::FinalizedBlockArtifact> &artifacts =
      load.loadedArtifacts();

  std::ostringstream output;
  output << "Nodo rewards status\n"
         << "-------------------\n"
         << "Chain height: " << chainHeight << "\n";

  if (!validatorAddr.empty()) {
    output << "Validator: " << validatorAddr << "\n";

    if (!artifacts.empty()) {
      const node::FinalizedBlockArtifact &tipArtifact = artifacts.back();
      std::int64_t totalReward = 0;

      for (const auto &dist : tipArtifact.rewardDistributions()) {
        if (dist.validatorAddress() == validatorAddr) {
          totalReward += dist.liquidReward().rawUnits();
          output << "  Block " << dist.blockHeight()
                 << " liquid reward: " << dist.liquidReward().rawUnits()
                 << "\n";
        }
      }
      output << "Total liquid rewards at tip: " << totalReward
             << " raw units\n";
    } else {
      output << "No finalized artifacts loaded.\n";
    }
  } else {
    output << "Use --validator <address> to see rewards for a specific "
              "validator.\n";
  }

  return CommandLineResult::success(output.str());
}

CommandLineResult CommandLineInterface::executeSlashingEvidence(
    const CommandLineOptions &options) {
  const std::string validatorAddr = options.validatorAddress.empty()
                                        ? options.toAddress
                                        : options.validatorAddress;

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

  const core::Blockchain &blockchain = load.runtime().blockchain();
  const std::uint64_t chainHeight =
      blockchain.empty() ? 0u : blockchain.latestBlock().index();
  const std::vector<node::FinalizedBlockArtifact> &artifacts =
      load.loadedArtifacts();

  std::ostringstream output;
  output << "Nodo slashing evidence\n"
         << "----------------------\n"
         << "Chain height: " << chainHeight << "\n";

  if (!validatorAddr.empty()) {
    output << "Validator: " << validatorAddr << "\n";

    if (!artifacts.empty()) {
      const node::FinalizedBlockArtifact &tipArtifact = artifacts.back();
      std::size_t evidenceCount = 0;

      for (const auto &rec :
           tipArtifact.cryptographicSlashingEvidenceRecords()) {
        if (rec.validatorAddress() == validatorAddr) {
          output << "  Evidence at block " << rec.blockHeight() << " round "
                 << rec.round() << " severity " << rec.severityScore() << "\n";
          ++evidenceCount;
        }
      }
      for (const auto &pen : tipArtifact.stakePenaltyRecords()) {
        if (pen.validatorAddress() == validatorAddr) {
          output << "  Stake penalty: before="
                 << pen.lockedStakeBefore().rawUnits()
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
    output << "Use --validator <address> to see slashing evidence for a "
              "specific validator.\n";
  }

  return CommandLineResult::success(output.str());
}

} // namespace nodo::app
