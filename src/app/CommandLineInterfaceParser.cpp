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

std::uint64_t parseUnsignedInt64(const std::string &option,
                                 const std::string &value) {
  if (value.empty()) {
    throw std::invalid_argument(option + " value must not be empty.");
  }

  if (!value.empty() && value[0] == '-') {
    throw std::invalid_argument(
        option + " requires a non-negative integer, got: " + value);
  }

  std::size_t pos = 0;
  unsigned long long result = 0;

  try {
    result = std::stoull(value, &pos);
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

  return static_cast<std::uint64_t>(result);
}

struct ParsedHostPort {
  std::string host;
  std::uint16_t port;
};

ParsedHostPort parseHostPort(const std::string &option,
                             const std::string &value) {
  const std::size_t separator = value.rfind(':');
  if (separator == std::string::npos || separator == 0 ||
      separator + 1 >= value.size()) {
    throw std::invalid_argument(
        option + " requires HOST:PORT with a non-empty host and port.");
  }

  std::string host = value.substr(0, separator);
  // IPv6 literals are written in brackets, as in [::1]:8545.
  if (host.size() > 2 && host.front() == '[' && host.back() == ']') {
    host = host.substr(1, host.size() - 2);
  }
  const std::string portText = value.substr(separator + 1);
  for (const char character : portText) {
    if (character < '0' || character > '9') {
      throw std::invalid_argument(
          option + " port must be an integer between 1 and 65535.");
    }
  }

  const std::uint64_t parsedPort = parseUnsignedInt64(option, portText);
  if (parsedPort == 0 || parsedPort > 65535) {
    throw std::invalid_argument(option + " port must be between 1 and 65535.");
  }

  return ParsedHostPort{host, static_cast<std::uint16_t>(parsedPort)};
}

bool isOption(const std::string &value) { return value.rfind("--", 0) == 0; }

bool isCommandGroup(const std::string &value) {
  return value == "tx" || value == "block" || value == "node" ||
         value == "chain" || value == "keys" || value == "validator" ||
         value == "stake" || value == "rewards" || value == "slashing" ||
         value == "governance" || value == "testnet" || value == "genesis" ||
         value == "checkpoint" || value == "storage" || value == "pruning" ||
         value == "archive";
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

} // namespace

CommandLineOptions
CommandLineInterface::parse(const std::vector<std::string> &args) {
  CommandLineOptions options;

  std::size_t index = 0;

  if (!args.empty() && !isOption(args.front())) {
    options.command = args.front();
    index = 1;

    if (isCommandGroup(options.command) && index < args.size() &&
        !isOption(args[index])) {
      options.command += " " + args[index];
      ++index;
    }
  }

  while (index < args.size()) {
    const std::string &option = args[index];

    if (option == "--help" || option == "-h") {
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

    if (option == "--from" || option == "--key-id") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(option + " requires a value.");
      }

      const std::string value = args[index + 1];
      if (options.keyIdProvided && options.keyId != value) {
        throw std::invalid_argument("Conflicting signing key options.");
      }
      options.keyId = value;
      options.keyIdProvided = true;
      index += 2;
      continue;
    }

    if (option == "--type") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--type requires a value.");
      }

      options.keyType = args[index + 1];

      if (options.keyType != "user" && options.keyType != "validator" &&
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

    if (option == "--address") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--address requires a value.");
      }

      options.toAddress = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--amount") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--amount requires a value.");
      }

      options.amountRaw = parseSignedInt64("--amount", args[index + 1]);

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

      options.feeRaw = parseSignedInt64("--fee", args[index + 1]);

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

      options.nonce = parseUnsignedInt64("--nonce", args[index + 1]);
      index += 2;
      continue;
    }

    if (option == "--timestamp") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--timestamp requires a value.");
      }

      options.timestamp = parseSignedInt64("--timestamp", args[index + 1]);
      index += 2;
      continue;
    }

    if (option == "--proposal-id") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--proposal-id requires a value.");
      }

      options.governanceProposalId = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--proposal-type") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--proposal-type requires a value.");
      }

      options.governanceProposalType = args[index + 1];
      (void)parseGovernanceProposalTypeOption(options.governanceProposalType);
      index += 2;
      continue;
    }

    if (option == "--title") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--title requires a value.");
      }

      options.governanceProposalTitle = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--body") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--body requires a value.");
      }

      options.governanceProposalBody = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--target") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--target requires a value.");
      }

      options.governanceTarget = normalizeGovernanceToken(args[index + 1]);
      index += 2;
      continue;
    }

    if (option == "--value") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--value requires a value.");
      }

      options.governanceValue = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--effective-height") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--effective-height requires a value.");
      }

      options.governanceEffectiveHeight =
          parseUnsignedInt64("--effective-height", args[index + 1]);
      index += 2;
      continue;
    }

    if (option == "--voting-period") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--voting-period requires a value.");
      }

      options.governanceVotingPeriodBlocks =
          parseUnsignedInt64("--voting-period", args[index + 1]);
      if (options.governanceVotingPeriodBlocks == 0) {
        throw std::invalid_argument("--voting-period must be positive.");
      }
      index += 2;
      continue;
    }

    if (option == "--vote") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--vote requires a value.");
      }

      options.governanceVoteChoice = args[index + 1];
      (void)parseGovernanceVoteChoiceOption(options.governanceVoteChoice);
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

    if (option == "--rpc-listen") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(
            "--rpc-listen requires a value (HOST:PORT).");
      }

      const ParsedHostPort rpc = parseHostPort("--rpc-listen", args[index + 1]);
      options.rpcBindAddress = rpc.host;
      options.rpcPort = rpc.port;
      index += 2;
      continue;
    }

    if (option == "--peer") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(
            "--peer requires a value (NAME@HOST:PORT).");
      }

      options.peers.push_back(args[index + 1]);
      index += 2;
      continue;
    }

    if (option == "--validator-key") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--validator-key requires a value.");
      }

      const std::string value = args[index + 1];
      if (options.validatorKeyIdProvided && options.validatorKeyId != value) {
        throw std::invalid_argument("Conflicting --validator-key values.");
      }
      options.validatorKeyId = value;
      options.validatorKeyIdProvided = true;
      // node run historically exposes this option through keyId. Keep
      // that public parse contract while stake commands retain a
      // separate owner signing key.
      if (options.command == "node run") {
        options.keyId = value;
        options.keyIdProvided = true;
      }
      index += 2;
      continue;
    }

    if (option == "--identity-key") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--identity-key requires a value.");
      }
      options.identityKeyId = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--validator") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(
            "--validator requires a value (validator address).");
      }

      const std::string value = args[index + 1];
      if (!options.validatorAddress.empty() &&
          options.validatorAddress != value) {
        throw std::invalid_argument("Conflicting --validator values.");
      }
      options.validatorAddress = value;
      index += 2;
      continue;
    }

    if (option == "--owner") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(
            "--owner requires a value (key-id of validator owner).");
      }

      const std::string value = args[index + 1];
      if (options.keyIdProvided && options.keyId != value) {
        throw std::invalid_argument("Conflicting signing key options.");
      }
      options.keyId = value;
      options.keyIdProvided = true;
      index += 2;
      continue;
    }

    if (option == "--stake") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(
            "--stake requires a value (amount in raw units).");
      }

      options.amountRaw = parseSignedInt64("--stake", args[index + 1]);
      if (options.amountRaw < 0) {
        throw std::invalid_argument("--stake must be non-negative.");
      }
      index += 2;
      continue;
    }

    if (option == "--pruning-mode" || option == "--mode") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(option + " requires archive, full, or light.");
      }
      options.pruningMode = args[index + 1];
      if (options.pruningMode != "archive" && options.pruningMode != "full" &&
          options.pruningMode != "light" && options.pruningMode != "ARCHIVE" &&
          options.pruningMode != "FULL" && options.pruningMode != "LIGHT" &&
          options.pruningMode != "normal" && options.pruningMode != "NORMAL") {
        throw std::invalid_argument(option +
                                    " must be archive, normal, full, or light.");
      }
      index += 2;
      continue;
    }

    if (option == "--retain-epochs") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--retain-epochs requires a positive integer.");
      }
      options.pruningRetainEpochs =
          parseUnsignedInt64("--retain-epochs", args[index + 1]);
      if (options.pruningRetainEpochs == 0) {
        throw std::invalid_argument("--retain-epochs must be positive.");
      }
      index += 2;
      continue;
    }

    if (option == "--height") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--height requires a value.");
      }
      options.height = parseUnsignedInt64("--height", args[index + 1]);
      options.heightProvided = true;
      index += 2;
      continue;
    }

    if (option == "--segment") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--segment requires a value.");
      }
      options.segmentIndex = parseUnsignedInt64("--segment", args[index + 1]);
      options.segmentProvided = true;
      index += 2;
      continue;
    }

    if (option == "--source-dir") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--source-dir requires a value.");
      }
      options.sourceDataDirectory = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--trusted-checkpoint") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(
            "--trusted-checkpoint requires HEIGHT:CHECKPOINT_ID.");
      }
      options.trustedCheckpoint = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--retain-blocks") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--retain-blocks requires a value.");
      }
      options.retainBlocks =
          parseUnsignedInt64("--retain-blocks", args[index + 1]);
      index += 2;
      continue;
    }

    if (option == "--retain-snapshots") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--retain-snapshots requires a value.");
      }
      const std::uint64_t value =
          parseUnsignedInt64("--retain-snapshots", args[index + 1]);
      if (value > 1000000) {
        throw std::invalid_argument("--retain-snapshots is out of range.");
      }
      options.retainSnapshots = static_cast<std::uint32_t>(value);
      index += 2;
      continue;
    }

    if (option == "--dry-run") {
      options.dryRun = true;
      ++index;
      continue;
    }

    if (option == "--genesis-file") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--genesis-file requires a value.");
      }
      options.genesisFile = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--genesis-validator") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--genesis-validator requires a value "
                                    "(BLS_PUBLIC_KEY_HEX:OWNER_ADDRESS).");
      }
      options.genesisValidators.push_back(args[index + 1]);
      index += 2;
      continue;
    }

    if (option == "--genesis-account") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(
            "--genesis-account requires a value (ADDRESS:BALANCE_RAW).");
      }
      options.genesisAccounts.push_back(args[index + 1]);
      index += 2;
      continue;
    }

    if (option == "--memo") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--memo requires a value.");
      }
      options.genesisMemo = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--output") {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument("--output requires a value.");
      }
      options.outputPath = args[index + 1];
      index += 2;
      continue;
    }

    if (option == "--json") {
      options.outputJson = true;
      index++;
      continue;
    }

    throw std::invalid_argument("Unknown option: " + option);
  }

  return options;
}

std::string CommandLineInterface::helpText() {
  return "Nodo command line\n"
         "-----------------\n"
         "\n"
         "Usage:\n"
         "  nodo help\n"
         "  nodo init [--network localnet|testnet-candidate] [--data-dir PATH] "
         "[--genesis-file PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
         "  nodo genesis create --network testnet-candidate --output PATH "
         "--genesis-validator BLS_PUBLIC_KEY_HEX:OWNER_ADDRESS... "
         "[--genesis-account ADDRESS:BALANCE_RAW]... [--memo TEXT] "
         "[--timestamp SECONDS]\n"
         "  nodo genesis inspect --genesis-file PATH\n"
         "  nodo status [--network localnet|testnet-candidate] [--data-dir "
         "PATH]\n"
         "  nodo inspect [--network localnet|testnet-candidate] [--data-dir "
         "PATH]\n"
         "  nodo node run [--network localnet|localnet-soak|testnet-candidate] "
         "[--data-dir PATH] [--listen HOST:PORT] [--rpc-listen HOST:PORT] "
         "[--peer NAME@HOST:PORT]... [--validator-key ID] [--identity-key ID]\n"
         "  nodo node reload [--network localnet|testnet-candidate] "
         "[--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]\n"
         "  nodo keys create [--network localnet|testnet-candidate] "
         "[--data-dir PATH] [--type user|validator|both] [--key-id ID]\n"
         "  nodo keys list [--data-dir PATH]\n"
         "  nodo tx submit [--network localnet|testnet-candidate] [--data-dir "
         "PATH] [--from KEY_ID] [--to ADDRESS] "
         "[--amount RAW_UNITS] [--fee RAW_UNITS] [--nonce VALUE]\n"
         "  nodo governance propose [--network localnet|testnet-candidate] "
         "[--data-dir PATH] [--from KEY_ID] "
         "[--proposal-type parameter-change|treasury-spend|text] [--title "
         "TEXT] [--body TEXT] [--target PARAM] [--value VALUE] "
         "[--effective-height HEIGHT] [--to ADDRESS] [--amount RAW_UNITS] "
         "[--fee RAW_UNITS] [--voting-period BLOCKS]\n"
         "  nodo governance vote [--network localnet|testnet-candidate] "
         "[--data-dir PATH] [--owner KEY_ID] "
         "--proposal-id ID --validator ADDRESS [--vote YES|NO|ABSTAIN] [--fee "
         "RAW_UNITS]\n"
         "  nodo governance execute [--network localnet|testnet-candidate] "
         "[--data-dir PATH] [--from KEY_ID] "
         "--proposal-id ID [--fee RAW_UNITS]\n"
         "  nodo governance status|list|show|audit [--data-dir PATH] "
         "[--proposal-id ID]\n"
         "  nodo stake lock|deposit|top-up|unlock|withdraw [--network "
         "localnet|testnet-candidate] [--data-dir PATH] "
         "(--validator ADDRESS | --validator-key ID) --amount RAW_UNITS "
         "[--owner KEY_ID] [--fee RAW_UNITS]\n"
         "  nodo stake status [--data-dir PATH] (--validator ADDRESS | "
         "--validator-key ID)\n"
         "  nodo stake positions|audit [--data-dir PATH] [--validator ADDRESS] "
         "[--address ADDRESS]\n"
         "  nodo validator exit [--network localnet|testnet-candidate] "
         "[--data-dir PATH] --validator ADDRESS [--key-id ID]\n"
         "  nodo validator unjail [--network localnet|testnet-candidate] "
         "[--data-dir PATH] --validator ADDRESS [--key-id ID]\n"
         "  nodo block produce [--data-dir PATH]\n"
         "  nodo chain audit [--data-dir PATH] [--peer-id ID] [--endpoint "
         "HOST:PORT]\n"
         "  nodo validator list [--data-dir PATH]\n"
         "  nodo testnet readiness [--network localnet|testnet-candidate] "
         "[--data-dir PATH] [--key-id ID]\n"
         "  nodo diagnostics [--network localnet|testnet-candidate] "
         "[--data-dir PATH] [--key-id ID]\n"
         "  nodo checkpoint status|list|backfill [--data-dir PATH] [--json]\n"
         "  nodo checkpoint show|verify [--data-dir PATH] [--height HEIGHT] "
         "[--json]\n"
         "  nodo checkpoint verify-bootstrap --source-dir PATH "
         "--trusted-checkpoint HEIGHT:CHECKPOINT_ID [--data-dir PATH]\n"
         "  nodo storage status|migrate [--data-dir PATH] [--json]\n"
         "  nodo pruning status|run [--data-dir PATH] "
         "[--mode archive|normal|light] [--retain-blocks N] "
         "[--retain-snapshots N] [--dry-run]\n"
         "  nodo archive status|segments|enable [--data-dir PATH] [--json]\n"
         "  nodo archive self-audit [--data-dir PATH] [--segment INDEX]\n"
         "\n"
         "Options:\n"
         "  --data-dir PATH      Node data directory. Default: .nodo\n"
         "  --network NAME       Network profile: localnet, localnet-soak, or "
         "testnet-candidate. mainnet is blocked.\n"
         "  --genesis-file PATH  Operator genesis document. Required by init "
         "on testnet-candidate, which has no built-in genesis; later "
         "commands read the copy pinned in the data directory.\n"
         "  --genesis-validator PUBKEY:OWNER Bootstrap validator for genesis "
         "create: BLS public key hex and owner address (repeatable).\n"
         "  --genesis-account ADDRESS:BALANCE_RAW Funded account for genesis "
         "create (repeatable).\n"
         "  --memo TEXT          Genesis memo for genesis create.\n"
         "  --output PATH        Output file for genesis create. Never "
         "overwritten.\n"
         "  --peer-id ID         Local peer id for init/load. Default: "
         "local-node\n"
         "  --endpoint HOST:PORT Local endpoint for init/load. Default: "
         "127.0.0.1:9000\n"
         "  --listen HOST:PORT   Bind address for node run daemon. Overrides "
         "--endpoint.\n"
         "  --rpc-listen HOST:PORT RPC bind address for node run. Default: "
         "127.0.0.1:8545\n"
         "  --peer NAME@HOST:PORT Static peer for node run daemon "
         "(repeatable).\n"
         "  --validator-key ID   Validator identity key for node run, or a "
         "stake target resolved to its validator address.\n"
         "  --identity-key ID    Ed25519 peer identity key for node run. "
         "Default: local-user\n"
         "  --validator ADDRESS Validator address for lifecycle, stake, and "
         "governance vote commands.\n"
         "  --address ADDRESS   Owner/delegator address for stake positions.\n"
         "  --owner KEY_ID       Alias for --key-id when signing "
         "validator-owned operations.\n"
         "  --key-id ID          Key id for keys create or signing. Defaults "
         "depend on command.\n"
         "  --type TYPE          Key type for keys create. Default: both\n"
         "  --from KEY_ID        Alias for --key-id in tx submit.\n"
         "  --to ADDRESS         Recipient address for tx submit.\n"
         "  --amount RAW_UNITS   Transfer amount for tx submit. Default: 1000\n"
         "  --fee RAW_UNITS      Transfer fee for tx submit. Default: 100\n"
         "  --nonce VALUE        Transaction nonce for tx submit. Default: "
         "next account nonce\n"
         "  --timestamp SECONDS  Deterministic timestamp override for tests.\n"
         "  --proposal-id ID     Governance proposal id for "
         "vote/show/tally/decision/execution.\n"
         "  --proposal-type TYPE Governance proposal type: parameter-change, "
         "treasury-spend, or text.\n"
         "  --target PARAM       Governance parameter target, for example "
         "MINIMUM_FEE_RAW.\n"
         "  --value VALUE        Governance parameter value.\n"
         "  --effective-height H Block height where a parameter proposal may "
         "execute.\n"
         "  --voting-period N    Governance voting period in blocks. Default: "
         "3\n"
         "  --vote CHOICE        Governance vote choice: YES, NO, or ABSTAIN. "
         "Default: YES\n";
}

} // namespace nodo::app
