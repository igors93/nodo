#include "app/CommandLineInterface.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::app::CommandLineInterface;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path tempPath(
    const std::string& suffix
) {
    return std::filesystem::temp_directory_path()
        / ("nodo-cli-runtime-block-tests-" + suffix);
}

void clean(
    const std::filesystem::path& path
) {
    std::error_code error;
    std::filesystem::remove_all(
        path,
        error
    );
}

std::string readFile(
    const std::filesystem::path& path
) {
    std::ifstream input(path);
    return std::string(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
}

void writeFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    std::ofstream output(path, std::ios::trunc);
    output << contents;
}

void testProduceBlockRequiresMempoolTransaction() {
    const std::filesystem::path path =
        tempPath("produce-demo");

    clean(path);

    const auto init =
        CommandLineInterface::execute(
            {
                "init",
                "--data-dir",
                path.string(),
                "--peer-id",
                "cli-runtime-block-peer",
                "--endpoint",
                "127.0.0.1:9500",
                "--timestamp",
                std::to_string(kTimestamp)
            }
        );

    requireCondition(
        init.success(),
        "Init should succeed before producing a block."
    );

    const auto produceEmpty =
        CommandLineInterface::execute(
            {
                "block",
                "produce",
                "--data-dir",
                path.string(),
                "--peer-id",
                "cli-runtime-block-peer",
                "--endpoint",
                "127.0.0.1:9500",
                "--timestamp",
                std::to_string(kTimestamp + 100)
            }
        );

    requireCondition(
        !produceEmpty.success() &&
        produceEmpty.message().find("mempool is empty") != std::string::npos,
        "block produce should fail clearly when mempool is empty."
    );

    const auto key =
        CommandLineInterface::execute(
            {
                "keys",
                "create",
                "--data-dir",
                path.string(),
                "--timestamp",
                std::to_string(kTimestamp + 120)
            }
        );

    requireCondition(
        key.success(),
        "Key creation should succeed before submit."
    );

    const auto submit =
        CommandLineInterface::execute(
            {
                "tx",
                "submit",
                "--data-dir",
                path.string(),
                "--timestamp",
                std::to_string(kTimestamp + 130)
            }
        );

    requireCondition(
        submit.success(),
        "tx submit should persist a transaction for block production."
    );

    const auto produce =
        CommandLineInterface::execute(
            {
                "block",
                "produce",
                "--data-dir",
                path.string(),
                "--peer-id",
                "cli-runtime-block-peer",
                "--endpoint",
                "127.0.0.1:9500",
                "--timestamp",
                std::to_string(kTimestamp + 200)
            }
        );

    requireCondition(
        produce.success() &&
        produce.message().find("Block height: 1") != std::string::npos,
        "block produce should finalize and persist block height 1."
    );

    const auto status =
        CommandLineInterface::execute(
            {
                "status",
                "--data-dir",
                path.string()
            }
        );

    requireCondition(
        status.success() &&
        status.message().find("Latest height: 1") != std::string::npos,
        "Status should show updated height after block production."
    );

    clean(path);
}

void testProduceDemoBlockBeforeInitFails() {
    const std::filesystem::path path =
        tempPath("produce-before-init");

    clean(path);

    const auto produce =
        CommandLineInterface::execute(
            {
                "block",
                "produce",
                "--data-dir",
                path.string(),
                "--timestamp",
                std::to_string(kTimestamp + 200)
            }
        );

    requireCondition(
        !produce.success(),
        "block produce should fail before init."
    );

    clean(path);
}

void testChainAuditRejectsTamperedPostStateRoot() {
    const std::filesystem::path path =
        tempPath("audit-state-root");

    clean(path);

    const std::vector<std::string> peerOptions = {
        "--peer-id",
        "cli-audit-state-root-peer",
        "--endpoint",
        "127.0.0.1:9510"
    };

    requireCondition(
        CommandLineInterface::execute(
            {
                "init",
                "--data-dir",
                path.string(),
                peerOptions[0],
                peerOptions[1],
                peerOptions[2],
                peerOptions[3],
                "--timestamp",
                std::to_string(kTimestamp)
            }
        ).success(),
        "Init should succeed."
    );

    requireCondition(
        CommandLineInterface::execute(
            {
                "keys",
                "create",
                "--data-dir",
                path.string(),
                "--timestamp",
                std::to_string(kTimestamp + 10)
            }
        ).success(),
        "Key creation should succeed."
    );

    requireCondition(
        CommandLineInterface::execute(
            {
                "tx",
                "submit",
                "--data-dir",
                path.string(),
                "--timestamp",
                std::to_string(kTimestamp + 20)
            }
        ).success(),
        "Transaction submit should succeed."
    );

    requireCondition(
        CommandLineInterface::execute(
            {
                "block",
                "produce",
                "--data-dir",
                path.string(),
                peerOptions[0],
                peerOptions[1],
                peerOptions[2],
                peerOptions[3],
                "--timestamp",
                std::to_string(kTimestamp + 30)
            }
        ).success(),
        "Block production should succeed before tampering."
    );

    const std::filesystem::path blockPath =
        path / "blocks" / "block_1.nodo";

    std::string contents =
        readFile(blockPath);

    const std::size_t fieldStart =
        contents.find("postStateRoot=");

    requireCondition(
        fieldStart != std::string::npos,
        "Finalized block file should contain postStateRoot."
    );

    const std::size_t fieldEnd =
        contents.find('\n', fieldStart);

    contents.replace(
        fieldStart,
        fieldEnd - fieldStart,
        "postStateRoot=tampered-root"
    );

    writeFile(
        blockPath,
        contents
    );

    const auto audit =
        CommandLineInterface::execute(
            {
                "chain",
                "audit",
                "--data-dir",
                path.string(),
                peerOptions[0],
                peerOptions[1],
                peerOptions[2],
                peerOptions[3],
                "--timestamp",
                std::to_string(kTimestamp + 40)
            }
        );

    requireCondition(
        !audit.success() &&
        audit.message().find("postStateRoot") != std::string::npos,
        "Chain audit should reject tampered postStateRoot."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testProduceBlockRequiresMempoolTransaction();
        testProduceDemoBlockBeforeInitFails();
        testChainAuditRejectsTamperedPostStateRoot();

        std::cout << "Nodo command line runtime block tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo command line runtime block tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
