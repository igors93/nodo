// End-to-end coverage for the testnet-candidate genesis ceremony through the
// CLI: operators create random keys before any genesis exists, a coordinator
// builds the genesis document from their public keys only, and every
// operator initializes from that document. No testnet-candidate key or
// genesis is derivable from the source code.

#include "app/CommandLineInterface.hpp"

#include "node/NodeDataDirectory.hpp"
#include "storage/AtomicFile.hpp"

#include "../common/TestnetCandidateGenesisFixture.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::app::CommandLineInterface;
using nodo::app::CommandLineResult;
using nodo::app::CommandLineStatus;

constexpr std::int64_t kTimestamp = 1900000000;
constexpr const char *kPassword = "genesis-ceremony-tests-password";

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::filesystem::path tempPath(const std::string &suffix) {
  return std::filesystem::temp_directory_path() /
         ("nodo-genesis-ceremony-tests-" + suffix);
}

void clean(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove_all(path, error);
}

void setTestEnvVar(const char *name, const char *value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void unsetTestEnvVar(const char *name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

// Returns the value printed after "label: " on the first matching line.
std::string printedValue(const std::string &output, const std::string &label) {
  const std::string prefix = label + ": ";
  const std::size_t start = output.find(prefix);
  requireCondition(start != std::string::npos,
                   "Output has no '" + label + "' line: " + output);
  const std::size_t valueStart = start + prefix.size();
  return output.substr(valueStart, output.find('\n', valueStart) - valueStart);
}

struct OperatorKeys {
  std::string validatorPublicKey;
  std::string ownerAddress;
};

OperatorKeys createOperatorKeys(const std::filesystem::path &dataDir) {
  setTestEnvVar("NODO_KEY_PASSWORD", kPassword);
  const CommandLineResult validator = CommandLineInterface::execute(
      {"keys", "create", "--network", "testnet-candidate", "--data-dir",
       dataDir.string(), "--type", "validator", "--key-id", "local-validator",
       "--timestamp", std::to_string(kTimestamp)});
  const CommandLineResult owner = CommandLineInterface::execute(
      {"keys", "create", "--network", "testnet-candidate", "--data-dir",
       dataDir.string(), "--type", "user", "--key-id", "validator-owner",
       "--timestamp", std::to_string(kTimestamp)});
  unsetTestEnvVar("NODO_KEY_PASSWORD");

  requireCondition(validator.success() && owner.success(),
                   "Operators must be able to create testnet-candidate keys "
                   "before init: " +
                       validator.message() + owner.message());
  requireCondition(
      validator.message().find("operating system CSPRNG") !=
              std::string::npos &&
          validator.message().find("development key") == std::string::npos,
      "testnet-candidate keys must come from the CSPRNG: " +
          validator.message());

  return {printedValue(validator.message(), "Public key"),
          printedValue(owner.message(), "Address")};
}

std::vector<std::string>
genesisCreateArgs(const std::vector<OperatorKeys> &operators,
                  const std::filesystem::path &output,
                  const std::string &network = "testnet-candidate") {
  std::vector<std::string> args{"genesis",  "create",
                                "--network", network,
                                "--output", output.string(),
                                "--timestamp", std::to_string(kTimestamp)};
  for (const OperatorKeys &keys : operators) {
    args.push_back("--genesis-validator");
    args.push_back(keys.validatorPublicKey + ":" + keys.ownerAddress);
  }
  args.push_back("--genesis-account");
  args.push_back(operators.front().ownerAddress + ":1000000000");
  return args;
}

void testCeremonyProducesInitializableGenesis() {
  const std::filesystem::path root = tempPath("ceremony");
  clean(root);

  std::vector<OperatorKeys> operators;
  std::set<std::string> distinctKeys;
  for (int index = 0; index < 4; ++index) {
    operators.push_back(
        createOperatorKeys(root / ("operator-" + std::to_string(index))));
    distinctKeys.insert(operators.back().validatorPublicKey);
  }
  requireCondition(distinctKeys.size() == 4,
                   "Every operator must get a distinct validator key.");

  const std::filesystem::path genesisFile = root / "genesis.nodo";
  const CommandLineResult created =
      CommandLineInterface::execute(genesisCreateArgs(operators, genesisFile));
  requireCondition(created.success(),
                   "genesis create should succeed: " + created.message());

  const std::string genesisId = printedValue(created.message(), "Genesis id");
  requireCondition(
      nodo::storage::AtomicFile::readTextFile(genesisFile).find("privateKey") ==
          std::string::npos,
      "The genesis document must carry public material only.");

  const CommandLineResult inspected = CommandLineInterface::execute(
      {"genesis", "inspect", "--genesis-file", genesisFile.string()});
  requireCondition(inspected.success() &&
                       printedValue(inspected.message(), "Genesis id") ==
                           genesisId,
                   "genesis inspect must report the same genesis id: " +
                       inspected.message());

  const std::filesystem::path operatorDir = root / "operator-0";
  const CommandLineResult init = CommandLineInterface::execute(
      {"init", "--network", "testnet-candidate", "--data-dir",
       operatorDir.string(), "--genesis-file", genesisFile.string(),
       "--timestamp", std::to_string(kTimestamp)});
  requireCondition(init.success() &&
                       printedValue(init.message(), "Genesis id") == genesisId,
                   "init must pin the ceremony genesis: " + init.message());

  // Later commands read the genesis pinned in the data directory.
  const CommandLineResult status = CommandLineInterface::execute(
      {"status", "--network", "testnet-candidate", "--data-dir",
       operatorDir.string()});
  requireCondition(status.success() &&
                       status.message().find("Validators: 4") !=
                           std::string::npos,
                   "status should load the pinned genesis: " +
                       status.message());

  setTestEnvVar("NODO_KEY_PASSWORD", kPassword);
  const CommandLineResult readiness = CommandLineInterface::execute(
      {"testnet", "readiness", "--network", "testnet-candidate", "--data-dir",
       operatorDir.string(), "--key-id", "local-validator"});
  unsetTestEnvVar("NODO_KEY_PASSWORD");
  requireCondition(
      readiness.success() &&
          readiness.message().find("Genesis verified: yes") !=
              std::string::npos &&
          readiness.message().find("Key policy passed: yes") !=
              std::string::npos,
      "A ceremony key must pass the testnet-candidate key policy: " +
          readiness.message());

  clean(root);
}

void testGenesisCreateRejections() {
  const std::filesystem::path root = tempPath("rejections");
  clean(root);

  std::vector<OperatorKeys> operators;
  for (int index = 0; index < 4; ++index) {
    operators.push_back(
        createOperatorKeys(root / ("operator-" + std::to_string(index))));
  }

  const CommandLineResult localnet = CommandLineInterface::execute(
      genesisCreateArgs(operators, root / "localnet.nodo", "localnet"));
  requireCondition(!localnet.success() &&
                       localnet.message().find("built-in genesis") !=
                           std::string::npos,
                   "genesis create must refuse built-in networks: " +
                       localnet.message());

  std::vector<OperatorKeys> tooFew(operators.begin(), operators.end() - 1);
  const CommandLineResult few = CommandLineInterface::execute(
      genesisCreateArgs(tooFew, root / "few.nodo"));
  requireCondition(!few.success() &&
                       few.message().find("at least 4") != std::string::npos,
                   "genesis create must enforce the minimum validator count: " +
                       few.message());
  requireCondition(!std::filesystem::exists(root / "few.nodo"),
                   "A rejected genesis must not be written.");

  std::vector<OperatorKeys> duplicated = operators;
  duplicated.back() = duplicated.front();
  const CommandLineResult duplicate = CommandLineInterface::execute(
      genesisCreateArgs(duplicated, root / "duplicate.nodo"));
  requireCondition(!duplicate.success() &&
                       duplicate.message().find("duplicates") !=
                           std::string::npos,
                   "genesis create must reject duplicate validators: " +
                       duplicate.message());

  const CommandLineResult malformed = CommandLineInterface::execute(
      {"genesis", "create", "--network", "testnet-candidate", "--output",
       (root / "malformed.nodo").string(), "--genesis-validator",
       "no-separator"});
  requireCondition(!malformed.success() &&
                       malformed.status() ==
                           CommandLineStatus::INVALID_ARGUMENTS &&
                       malformed.message().find("must have the form") !=
                           std::string::npos,
                   "Malformed --genesis-validator must be rejected: " +
                       malformed.message());

  const std::filesystem::path existing = root / "existing.nodo";
  requireCondition(
      CommandLineInterface::execute(genesisCreateArgs(operators, existing))
          .success(),
      "First genesis create should succeed.");
  const CommandLineResult overwrite =
      CommandLineInterface::execute(genesisCreateArgs(operators, existing));
  requireCondition(!overwrite.success() &&
                       overwrite.message().find("Refusing to overwrite") !=
                           std::string::npos,
                   "genesis create must never overwrite a file: " +
                       overwrite.message());

  clean(root);
}

void testInitRequiresOperatorGenesisOnlyWhereNeeded() {
  const std::filesystem::path root = tempPath("init");
  clean(root);

  const CommandLineResult withoutFile = CommandLineInterface::execute(
      {"init", "--network", "testnet-candidate", "--data-dir",
       (root / "no-file").string(), "--timestamp",
       std::to_string(kTimestamp)});
  requireCondition(!withoutFile.success() &&
                       withoutFile.message().find("--genesis-file") !=
                           std::string::npos,
                   "testnet-candidate init must require a genesis file: " +
                       withoutFile.message());

  const std::filesystem::path genesisFile =
      nodo::tests::writeTestnetCandidateTestGenesis(root / "genesis.nodo");
  const CommandLineResult localnet = CommandLineInterface::execute(
      {"init", "--network", "localnet", "--data-dir",
       (root / "localnet").string(), "--genesis-file", genesisFile.string(),
       "--timestamp", std::to_string(kTimestamp)});
  requireCondition(!localnet.success() &&
                       localnet.message().find("built-in genesis") !=
                           std::string::npos,
                   "localnet must reject an operator genesis file: " +
                       localnet.message());

  const CommandLineResult localnetKeys = CommandLineInterface::execute(
      {"keys", "create", "--network", "localnet", "--data-dir",
       (root / "localnet-keys").string(), "--timestamp",
       std::to_string(kTimestamp)});
  requireCondition(!localnetKeys.success() &&
                       localnetKeys.message().find(
                           "Cannot create key before init") !=
                           std::string::npos,
                   "localnet key creation must still require init: " +
                       localnetKeys.message());

  clean(root);
}

// The manifest pins the genesis id, so editing the pinned document is caught.
void testTamperedPinnedGenesisRejected() {
  const std::filesystem::path root = tempPath("tampered");
  clean(root);

  const std::filesystem::path dataDir = root / "node";
  const std::filesystem::path genesisFile =
      nodo::tests::writeTestnetCandidateTestGenesis(root / "genesis.nodo");
  requireCondition(
      CommandLineInterface::execute(
          {"init", "--network", "testnet-candidate", "--data-dir",
           dataDir.string(), "--genesis-file", genesisFile.string(),
           "--timestamp", std::to_string(kTimestamp)})
          .success(),
      "init should succeed before tampering.");

  const std::filesystem::path pinned =
      nodo::node::NodeDataDirectoryConfig(dataDir).genesisConfigPath();
  std::string contents = nodo::storage::AtomicFile::readTextFile(pinned);
  const std::string memo = "genesisMemo=nodo-testnet-candidate-test-genesis";
  const std::size_t position = contents.find(memo);
  requireCondition(position != std::string::npos,
                   "Pinned genesis should be a genesis document.");
  contents.replace(position, memo.size(), "genesisMemo=forged");
  nodo::storage::AtomicFile::writeTextFile(pinned, contents);

  const CommandLineResult status = CommandLineInterface::execute(
      {"status", "--network", "testnet-candidate", "--data-dir",
       dataDir.string()});
  requireCondition(!status.success() &&
                       status.message().find("does not match registered "
                                             "genesis id") != std::string::npos,
                   "A tampered pinned genesis must be rejected: " +
                       status.message());

  clean(root);
}

} // namespace

int main() {
  try {
    testCeremonyProducesInitializableGenesis();
    testGenesisCreateRejections();
    testInitRequiresOperatorGenesisOnlyWhereNeeded();
    testTamperedPinnedGenesisRejected();

    std::cout << "Nodo genesis ceremony CLI tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo genesis ceremony CLI tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
