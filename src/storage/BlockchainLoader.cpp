#include "storage/BlockchainLoader.hpp"

#include "serialization/BlockCodec.hpp"
#include "storage/BlockchainStorageReader.hpp"
#include "storage/BlockStorageIndex.hpp"
#include "storage/ChainManifest.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::storage {

BlockchainLoadReport::BlockchainLoadReport()
    : m_success(true),
      m_failureReason(""),
      m_storageReaderValid(false),
      m_loadedBlockchainValid(false),
      m_loadedBlockchainMatchesManifest(false),
      m_loadedBlockchainMatchesIndex(false),
      m_loadedBlockCount(0) {}

bool BlockchainLoadReport::success() const {
    return m_success;
}

const std::string& BlockchainLoadReport::failureReason() const {
    return m_failureReason;
}

bool BlockchainLoadReport::storageReaderValid() const {
    return m_storageReaderValid;
}

bool BlockchainLoadReport::loadedBlockchainValid() const {
    return m_loadedBlockchainValid;
}

bool BlockchainLoadReport::loadedBlockchainMatchesManifest() const {
    return m_loadedBlockchainMatchesManifest;
}

bool BlockchainLoadReport::loadedBlockchainMatchesIndex() const {
    return m_loadedBlockchainMatchesIndex;
}

std::size_t BlockchainLoadReport::loadedBlockCount() const {
    return m_loadedBlockCount;
}

void BlockchainLoadReport::markFailure(
    std::string reason
) {
    m_success = false;
    m_failureReason = std::move(reason);
}

void BlockchainLoadReport::setStorageReaderValid(
    bool value
) {
    m_storageReaderValid = value;
}

void BlockchainLoadReport::setLoadedBlockchainValid(
    bool value
) {
    m_loadedBlockchainValid = value;
}

void BlockchainLoadReport::setLoadedBlockchainMatchesManifest(
    bool value
) {
    m_loadedBlockchainMatchesManifest = value;
}

void BlockchainLoadReport::setLoadedBlockchainMatchesIndex(
    bool value
) {
    m_loadedBlockchainMatchesIndex = value;
}

void BlockchainLoadReport::setLoadedBlockCount(
    std::size_t value
) {
    m_loadedBlockCount = value;
}

std::string BlockchainLoadReport::serialize() const {
    std::ostringstream oss;

    oss << "BlockchainLoadReport{"
        << "success=" << (m_success ? "true" : "false")
        << ";failureReason=" << m_failureReason
        << ";storageReaderValid=" << (m_storageReaderValid ? "true" : "false")
        << ";loadedBlockchainValid=" << (m_loadedBlockchainValid ? "true" : "false")
        << ";loadedBlockchainMatchesManifest=" << (m_loadedBlockchainMatchesManifest ? "true" : "false")
        << ";loadedBlockchainMatchesIndex=" << (m_loadedBlockchainMatchesIndex ? "true" : "false")
        << ";loadedBlockCount=" << m_loadedBlockCount
        << "}";

    return oss.str();
}

BlockchainLoadReport BlockchainLoader::auditStorageRoot(
    const std::string& rootDirectory
) {
    BlockchainLoadReport report;

    try {
        const BlockchainStorageReadReport storageReadReport =
            BlockchainStorageReader::auditStorageRoot(rootDirectory);

        report.setStorageReaderValid(storageReadReport.success());

        if (!storageReadReport.success()) {
            report.markFailure(storageReadReport.failureReason());
            return report;
        }

        const ChainManifest manifest =
            ChainManifest::readFromStorageRoot(rootDirectory);

        const BlockStorageIndex index =
            BlockStorageIndex::readFromStorageRoot(rootDirectory);

        const core::Blockchain loadedBlockchain =
            loadFromStorageRoot(rootDirectory);

        report.setLoadedBlockCount(loadedBlockchain.size());
        report.setLoadedBlockchainValid(loadedBlockchain.isValid(false));
        report.setLoadedBlockchainMatchesManifest(
            manifest.matchesBlockchain(loadedBlockchain)
        );
        report.setLoadedBlockchainMatchesIndex(
            index.matchesBlockchainAndManifest(loadedBlockchain, manifest)
        );

        if (!report.loadedBlockchainValid()) {
            report.markFailure("Loaded Blockchain is invalid.");
            return report;
        }

        if (!report.loadedBlockchainMatchesManifest()) {
            report.markFailure("Loaded Blockchain does not match ChainManifest.");
            return report;
        }

        if (!report.loadedBlockchainMatchesIndex()) {
            report.markFailure("Loaded Blockchain does not match BlockStorageIndex.");
            return report;
        }

        return report;
    } catch (const std::exception& error) {
        report.markFailure(error.what());
        return report;
    }
}

core::Blockchain BlockchainLoader::loadFromStorageRoot(
    const std::string& rootDirectory
) {
    if (rootDirectory.empty()) {
        throw std::invalid_argument("BlockchainLoader root directory cannot be empty.");
    }

    const ChainManifest manifest =
        ChainManifest::readFromStorageRoot(rootDirectory);

    if (!manifest.isValid()) {
        throw std::logic_error("Cannot load Blockchain from invalid ChainManifest.");
    }

    const BlockStorageIndex index =
        BlockStorageIndex::readFromStorageRoot(rootDirectory);

    if (!index.isValid()) {
        throw std::logic_error("Cannot load Blockchain from invalid BlockStorageIndex.");
    }

    if (index.chainManifestHash() != manifest.manifestHash()) {
        throw std::logic_error("BlockStorageIndex does not reference ChainManifest.");
    }

    const std::vector<StoredBlockSnapshot> snapshots =
        BlockchainStorageReader::readBlockSnapshots(rootDirectory);

    if (snapshots.size() != manifest.blockCount()) {
        throw std::logic_error("Stored snapshot count does not match ChainManifest.");
    }

    std::vector<core::Block> loadedBlocks;

    for (const auto& snapshot : snapshots) {
        loadedBlocks.push_back(
            serialization::BlockCodec::deserializeFromFile(
                snapshot.filePath()
            )
        );
    }

    core::Blockchain loadedBlockchain =
        buildBlockchainFromBlocks(loadedBlocks);

    if (!loadedBlockchain.isValid(false)) {
        throw std::logic_error("Loaded Blockchain validation failed.");
    }

    if (!manifest.matchesBlockchain(loadedBlockchain)) {
        throw std::logic_error("Loaded Blockchain does not match ChainManifest.");
    }

    if (!index.matchesBlockchainAndManifest(loadedBlockchain, manifest)) {
        throw std::logic_error("Loaded Blockchain does not match BlockStorageIndex.");
    }

    return loadedBlockchain;
}

core::Blockchain BlockchainLoader::buildBlockchainFromBlocks(
    const std::vector<core::Block>& blocks
) {
    if (blocks.empty()) {
        throw std::invalid_argument("Cannot build Blockchain from an empty block list.");
    }

    core::Blockchain blockchain;

    blockchain.addGenesisBlock(blocks.front());

    for (std::size_t i = 1; i < blocks.size(); ++i) {
        blockchain.addBlock(blocks[i]);
    }

    if (!blockchain.isValid(false)) {
        throw std::logic_error("Rebuilt Blockchain is invalid.");
    }

    return blockchain;
}

} // namespace nodo::storage