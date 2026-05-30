#include "node/FinalizedBlockStore.hpp"

#include <fstream>
#include <sstream>
#include <utility>

namespace nodo::node {

std::string finalizedBlockStoreStatusToString(
    FinalizedBlockStoreStatus status
) {
    switch (status) {
        case FinalizedBlockStoreStatus::STORED:
            return "STORED";
        case FinalizedBlockStoreStatus::ALREADY_STORED:
            return "ALREADY_STORED";
        case FinalizedBlockStoreStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case FinalizedBlockStoreStatus::INVALID_RUNTIME:
            return "INVALID_RUNTIME";
        case FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT:
            return "INVALID_PIPELINE_RESULT";
        case FinalizedBlockStoreStatus::NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        case FinalizedBlockStoreStatus::CONFLICTING_BLOCK_FILE:
            return "CONFLICTING_BLOCK_FILE";
        case FinalizedBlockStoreStatus::IO_ERROR:
            return "IO_ERROR";
        default:
            return "IO_ERROR";
    }
}

FinalizedBlockStoreResult::FinalizedBlockStoreResult()
    : m_status(FinalizedBlockStoreStatus::IO_ERROR),
      m_reason("Uninitialized finalized block store result."),
      m_manifest(),
      m_blockPath() {}

FinalizedBlockStoreResult FinalizedBlockStoreResult::stored(
    NodeRuntimeManifest manifest,
    std::filesystem::path blockPath
) {
    FinalizedBlockStoreResult result;
    result.m_status = FinalizedBlockStoreStatus::STORED;
    result.m_reason = "";
    result.m_manifest = std::move(manifest);
    result.m_blockPath = std::move(blockPath);
    return result;
}

FinalizedBlockStoreResult FinalizedBlockStoreResult::alreadyStored(
    NodeRuntimeManifest manifest,
    std::filesystem::path blockPath
) {
    FinalizedBlockStoreResult result;
    result.m_status = FinalizedBlockStoreStatus::ALREADY_STORED;
    result.m_reason = "Finalized block artifact is already stored.";
    result.m_manifest = std::move(manifest);
    result.m_blockPath = std::move(blockPath);
    return result;
}

FinalizedBlockStoreResult FinalizedBlockStoreResult::rejected(
    FinalizedBlockStoreStatus status,
    std::string reason
) {
    FinalizedBlockStoreResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

FinalizedBlockStoreStatus FinalizedBlockStoreResult::status() const {
    return m_status;
}

const std::string& FinalizedBlockStoreResult::reason() const {
    return m_reason;
}

const NodeRuntimeManifest& FinalizedBlockStoreResult::manifest() const {
    return m_manifest;
}

const std::filesystem::path& FinalizedBlockStoreResult::blockPath() const {
    return m_blockPath;
}

bool FinalizedBlockStoreResult::stored() const {
    return m_status == FinalizedBlockStoreStatus::STORED &&
           m_manifest.isValid();
}

bool FinalizedBlockStoreResult::alreadyStored() const {
    return m_status == FinalizedBlockStoreStatus::ALREADY_STORED &&
           m_manifest.isValid();
}

bool FinalizedBlockStoreResult::success() const {
    return stored() || alreadyStored();
}

std::string FinalizedBlockStoreResult::serialize() const {
    std::ostringstream oss;

    oss << "FinalizedBlockStoreResult{"
        << "status=" << finalizedBlockStoreStatusToString(m_status)
        << ";reason=" << m_reason
        << ";blockPath=" << m_blockPath.string()
        << ";manifest=" << (m_manifest.isValid() ? m_manifest.serialize() : "NONE")
        << "}";

    return oss.str();
}

FinalizedBlockStoreResult FinalizedBlockStore::persist(
    const NodeDataDirectoryConfig& directoryConfig,
    const NodeRuntime& runtime,
    const RuntimeBlockPipelineResult& pipelineResult,
    std::int64_t updatedAt
) {
    if (!directoryConfig.isValid()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_CONFIG,
            "Node data directory config is invalid."
        );
    }

    if (!runtime.isValid()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_RUNTIME,
            "Runtime is invalid."
        );
    }

    if (!pipelineResult.finalized()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_PIPELINE_RESULT,
            "Runtime block pipeline result is not finalized."
        );
    }

    const NodeDataDirectoryReadResult existingManifest =
        NodeDataDirectory::loadManifest(directoryConfig);

    if (!existingManifest.loaded()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::NOT_INITIALIZED,
            existingManifest.reason()
        );
    }

    if (existingManifest.manifest().genesisConfigId() !=
        runtime.config().genesisConfig().deterministicId()) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::INVALID_RUNTIME,
            "Runtime genesis does not match data directory manifest."
        );
    }

    const std::filesystem::path path =
        blockFilePath(
            directoryConfig,
            pipelineResult.block().index()
        );

    const std::string contents =
        finalizedBlockFileContents(pipelineResult);

    try {
        std::filesystem::create_directories(
            directoryConfig.blocksDirectoryPath()
        );

        if (std::filesystem::exists(path)) {
            const std::string existingContents =
                readTextFile(path);

            if (existingContents != contents) {
                return FinalizedBlockStoreResult::rejected(
                    FinalizedBlockStoreStatus::CONFLICTING_BLOCK_FILE,
                    "A different finalized block artifact already exists at this height."
                );
            }

            const NodeDataDirectoryReadResult snapshot =
                NodeDataDirectory::writeRuntimeSnapshot(
                    directoryConfig,
                    runtime,
                    updatedAt
                );

            if (!snapshot.loaded()) {
                return FinalizedBlockStoreResult::rejected(
                    FinalizedBlockStoreStatus::IO_ERROR,
                    snapshot.reason()
                );
            }

            return FinalizedBlockStoreResult::alreadyStored(
                snapshot.manifest(),
                path
            );
        }

        writeTextFile(
            path,
            contents
        );

        const NodeDataDirectoryReadResult snapshot =
            NodeDataDirectory::writeRuntimeSnapshot(
                directoryConfig,
                runtime,
                updatedAt
            );

        if (!snapshot.loaded()) {
            return FinalizedBlockStoreResult::rejected(
                FinalizedBlockStoreStatus::IO_ERROR,
                snapshot.reason()
            );
        }

        return FinalizedBlockStoreResult::stored(
            snapshot.manifest(),
            path
        );
    } catch (const std::exception& error) {
        return FinalizedBlockStoreResult::rejected(
            FinalizedBlockStoreStatus::IO_ERROR,
            error.what()
        );
    }
}

std::filesystem::path FinalizedBlockStore::blockFilePath(
    const NodeDataDirectoryConfig& directoryConfig,
    std::uint64_t blockIndex
) {
    std::ostringstream filename;
    filename << "block_" << blockIndex << ".nodo";

    return directoryConfig.blocksDirectoryPath() / filename.str();
}

std::string FinalizedBlockStore::finalizedBlockFileContents(
    const RuntimeBlockPipelineResult& pipelineResult
) {
    std::ostringstream oss;

    oss << "NODO_FINALIZED_BLOCK_V2\n"
        << "blockIndex=" << pipelineResult.block().index() << "\n"
        << "blockHash=" << pipelineResult.block().hash() << "\n"
        << "previousHash=" << pipelineResult.block().previousHash() << "\n"
        << "timestamp=" << pipelineResult.block().timestamp() << "\n"
        << "recordCount=" << pipelineResult.block().records().size() << "\n";

    const auto& records =
        pipelineResult.block().records();

    for (std::size_t index = 0; index < records.size(); ++index) {
        oss << "record." << index << "=" << records[index].serialize() << "\n";
    }

    oss << "block=" << pipelineResult.block().serialize() << "\n"
        << "quorumCertificate=" << pipelineResult.certificate().serialize() << "\n"
        << "finalizedRecord=" << pipelineResult.finalizedRecord().serialize() << "\n";

    return oss.str();
}

void FinalizedBlockStore::writeTextFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    std::ofstream output(
        path,
        std::ios::out | std::ios::trunc
    );

    if (!output) {
        throw std::runtime_error("Unable to open finalized block file for writing: " + path.string());
    }

    output << contents;

    if (!output) {
        throw std::runtime_error("Unable to write finalized block file: " + path.string());
    }
}

std::string FinalizedBlockStore::readTextFile(
    const std::filesystem::path& path
) {
    std::ifstream input(path);

    if (!input) {
        throw std::runtime_error("Unable to open finalized block file for reading: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    return buffer.str();
}

} // namespace nodo::node
