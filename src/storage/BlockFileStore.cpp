#include "storage/BlockFileStore.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::storage {

BlockFileStore::BlockFileStore(std::string rootDirectory)
    : m_rootDirectory(std::move(rootDirectory)) {
    if (m_rootDirectory.empty()) {
        throw std::invalid_argument("BlockFileStore root directory cannot be empty.");
    }
}

const std::string& BlockFileStore::rootDirectory() const {
    return m_rootDirectory;
}

std::string BlockFileStore::blockDirectoryPath() const {
    return blockDirectory().string();
}

void BlockFileStore::ensureStorageDirectory() const {
    std::error_code errorCode;

    std::filesystem::create_directories(blockDirectory(), errorCode);

    if (errorCode) {
        throw std::runtime_error(
            "Failed to create block storage directory: " + errorCode.message()
        );
    }

    if (!std::filesystem::exists(blockDirectory())) {
        throw std::runtime_error("Block storage directory was not created.");
    }

    if (!std::filesystem::is_directory(blockDirectory())) {
        throw std::runtime_error("Block storage path is not a directory.");
    }
}

void BlockFileStore::clearBlockStorage() const {
    std::error_code errorCode;

    if (std::filesystem::exists(blockDirectory())) {
        std::filesystem::remove_all(blockDirectory(), errorCode);

        if (errorCode) {
            throw std::runtime_error(
                "Failed to clear block storage directory: " + errorCode.message()
            );
        }
    }

    ensureStorageDirectory();
}

void BlockFileStore::writeBlock(const core::Block& block) const {
    if (!block.isValid()) {
        throw std::invalid_argument("Invalid block rejected by BlockFileStore.");
    }

    ensureStorageDirectory();

    const std::filesystem::path finalPath = blockFilePath(block);
    const std::string serializedBlock = block.serialize();

    if (std::filesystem::exists(finalPath)) {
        const std::string existingSnapshot = readBlockSnapshot(block);

        if (existingSnapshot != serializedBlock) {
            throw std::logic_error(
                "Stored block snapshot conflict detected for block hash: " +
                block.hash()
            );
        }

        return;
    }

    const std::filesystem::path temporaryPath =
        finalPath.string() + ".tmp";

    {
        std::ofstream output(
            temporaryPath,
            std::ios::out | std::ios::binary | std::ios::trunc
        );

        if (!output.is_open()) {
            throw std::runtime_error("Failed to open temporary block file for writing.");
        }

        output << serializedBlock;

        if (!output.good()) {
            throw std::runtime_error("Failed while writing block snapshot.");
        }
    }

    std::error_code errorCode;

    std::filesystem::rename(temporaryPath, finalPath, errorCode);

    if (errorCode) {
        std::filesystem::remove(temporaryPath);
        throw std::runtime_error(
            "Failed to finalize block snapshot file: " + errorCode.message()
        );
    }

    if (!verifyStoredBlock(block)) {
        throw std::runtime_error("Stored block snapshot verification failed.");
    }
}

void BlockFileStore::writeBlockchain(
    const core::Blockchain& blockchain
) const {
    if (blockchain.empty()) {
        throw std::invalid_argument("Empty Blockchain rejected by BlockFileStore.");
    }

    if (!blockchain.isValid()) {
        throw std::invalid_argument("Invalid Blockchain rejected by BlockFileStore.");
    }

    ensureStorageDirectory();

    for (const auto& block : blockchain.blocks()) {
        writeBlock(block);
    }
}

bool BlockFileStore::hasStoredBlock(
    const core::Block& block
) const {
    if (!block.isValid()) {
        return false;
    }

    return std::filesystem::exists(blockFilePath(block));
}

bool BlockFileStore::verifyStoredBlock(
    const core::Block& block
) const {
    if (!block.isValid()) {
        return false;
    }

    if (!hasStoredBlock(block)) {
        return false;
    }

    try {
        return readBlockSnapshot(block) == block.serialize();
    } catch (...) {
        return false;
    }
}

std::string BlockFileStore::readBlockSnapshot(
    const core::Block& block
) const {
    if (!block.isValid()) {
        throw std::invalid_argument("Invalid block rejected while reading snapshot.");
    }

    const std::filesystem::path path = blockFilePath(block);

    std::ifstream input(path, std::ios::in | std::ios::binary);

    if (!input.is_open()) {
        throw std::runtime_error("Failed to open block snapshot for reading.");
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (!input.good() && !input.eof()) {
        throw std::runtime_error("Failed while reading block snapshot.");
    }

    return buffer.str();
}

std::size_t BlockFileStore::storedBlockFileCount() const {
    if (!std::filesystem::exists(blockDirectory())) {
        return 0;
    }

    if (!std::filesystem::is_directory(blockDirectory())) {
        throw std::runtime_error("Block storage path is not a directory.");
    }

    std::size_t count = 0;

    for (const auto& entry : std::filesystem::directory_iterator(blockDirectory())) {
        if (entry.is_regular_file() && entry.path().extension() == ".nodo") {
            ++count;
        }
    }

    return count;
}

std::string BlockFileStore::serialize() const {
    std::ostringstream oss;

    oss << "BlockFileStore{"
        << "rootDirectory=" << m_rootDirectory
        << ";blockDirectory=" << blockDirectoryPath()
        << ";storedBlockFileCount=" << storedBlockFileCount()
        << "}";

    return oss.str();
}

std::filesystem::path BlockFileStore::rootPath() const {
    return std::filesystem::path(m_rootDirectory);
}

std::filesystem::path BlockFileStore::blockDirectory() const {
    return rootPath() / "blocks";
}

std::filesystem::path BlockFileStore::blockFilePath(
    const core::Block& block
) const {
    return blockDirectory() / blockFileName(block);
}

std::string BlockFileStore::blockFileName(
    const core::Block& block
) {
    if (!block.isValid()) {
        throw std::invalid_argument("Invalid block cannot be converted to file name.");
    }

    validateHashForFileName(block.hash());

    return "block_" +
           std::to_string(block.index()) +
           "_" +
           block.hash() +
           ".nodo";
}

void BlockFileStore::validateHashForFileName(
    const std::string& hash
) {
    if (hash.empty()) {
        throw std::invalid_argument("Block hash cannot be empty.");
    }

    for (const char current : hash) {
        const bool isDigit = current >= '0' && current <= '9';
        const bool isLowerHex = current >= 'a' && current <= 'f';
        const bool isUpperHex = current >= 'A' && current <= 'F';

        if (!isDigit && !isLowerHex && !isUpperHex) {
            throw std::invalid_argument("Block hash contains unsafe file name characters.");
        }
    }
}

} // namespace nodo::storage