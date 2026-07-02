#include "app/CommandLineInterface.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::app::CommandLineInterface;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path tempPath() {
    return std::filesystem::temp_directory_path() / "nodo-cli-stake-tests";
}

void clean(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

void testStakeOptionSemantics() {
    const auto lock = CommandLineInterface::parse({
        "stake", "lock", "--validator-key", "validator-one",
        "--owner", "owner-one", "--amount", "123"
    });
    requireCondition(lock.command == "stake lock", "Stake lock command was not parsed.");
    requireCondition(lock.validatorKeyIdProvided && lock.validatorKeyId == "validator-one",
        "--validator-key must identify the validator target.");
    requireCondition(lock.keyIdProvided && lock.keyId == "owner-one",
        "--owner must independently identify the transaction signer.");
    requireCondition(lock.amountRaw == 123, "Stake amount was not parsed.");

    const auto status = CommandLineInterface::parse({
        "stake", "status", "--validator-key", "validator-one"
    });
    requireCondition(status.validatorKeyIdProvided && !status.keyIdProvided,
        "Stake status must not reinterpret the validator identity as an owner signer.");
}

void testStakeLockAndStatusFlow() {
    const std::filesystem::path path = tempPath();
    clean(path);

    requireCondition(CommandLineInterface::execute({
        "init", "--data-dir", path.string(),
        "--timestamp", std::to_string(kTimestamp)
    }).success(), "Init should succeed before stake operations.");

    requireCondition(CommandLineInterface::execute({
        "keys", "create", "--data-dir", path.string(),
        "--timestamp", std::to_string(kTimestamp + 10)
    }).success(), "User and validator keys should be created.");

    const auto lock = CommandLineInterface::execute({
        "stake", "lock", "--data-dir", path.string(),
        "--validator-key", "local-validator",
        "--amount", "1000", "--timestamp", std::to_string(kTimestamp + 20)
    });
    requireCondition(lock.success() &&
        lock.message().find("Stake lock submitted.") != std::string::npos,
        "Stake lock should be signed by the default owner and persisted.");

    requireCondition(CommandLineInterface::execute({
        "block", "produce", "--data-dir", path.string(),
        "--timestamp", std::to_string(kTimestamp + 30)
    }).success(), "Stake lock should finalize through block production.");

    const auto status = CommandLineInterface::execute({
        "stake", "status", "--data-dir", path.string(),
        "--validator-key", "local-validator"
    });
    requireCondition(status.success() &&
        status.message().find("Stake status") != std::string::npos &&
        status.message().find("Bonded stake (raw units): 1001000") != std::string::npos &&
        status.message().find("Active stake (raw units): 1001000") != std::string::npos,
        "Stake status should resolve the validator key and expose finalized stake.");

    clean(path);
}

} // namespace

int main() {
    try {
        testStakeOptionSemantics();
        testStakeLockAndStatusFlow();
        std::cout << "Command line stake tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Command line stake tests failed: " << error.what() << '\n';
        return 1;
    }
}
