#include "node/RuntimeStateLoader.hpp"

#include "node/FinalizedBlockStore.hpp"
#include "node/PersistentMempoolStore.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::string readTextFile(
    const std::filesystem::path& path
) {
    std::ifstream input(path);

    if (!input) {
        throw std::runtime_error("Unable to read file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    return buffer.str();
}

std::map<std::string, std::string> parseKeyValueLines(
    const std::string& contents
) {
    std::map<std::string, std::string> fields;
    std::istringstream input(contents);
    std::string line;
    bool first = true;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        if (first &&
            line.rfind("NODO_FINALIZED_BLOCK_", 0) == 0) {
            fields.emplace("version", line);
            first = false;
            continue;
        }

        first = false;

        const std::size_t separator =
            line.find('=');

        if (separator == std::string::npos ||
            separator == 0 ||
            separator + 1 >= line.size()) {
            continue;
        }

        fields.emplace(
            line.substr(0, separator),
            line.substr(separator + 1)
        );
    }

    return fields;
}

std::string requireField(
    const std::map<std::string, std::string>& fields,
    const std::string& key
) {
    const auto found =
        fields.find(key);

    if (found == fields.end()) {
        throw std::invalid_argument("Missing finalized block field: " + key);
    }

    return found->second;
}

std::string extractSimpleField(
    const std::string& serialized,
    const std::string& key
) {
    const std::string prefix =
        key + "=";

    const std::size_t start =
        serialized.find(prefix);

    if (start == std::string::npos) {
        throw std::invalid_argument("Missing serialized field: " + key);
    }

    const std::size_t valueStart =
        start + prefix.size();

    std::size_t valueEnd =
        serialized.find(';', valueStart);

    if (valueEnd == std::string::npos) {
        valueEnd = serialized.find('}', valueStart);
    }

    if (valueEnd == std::string::npos ||
        valueEnd <= valueStart) {
        throw std::invalid_argument("Malformed serialized field: " + key);
    }

    return serialized.substr(
        valueStart,
        valueEnd - valueStart
    );
}

std::string extractLedgerPayload(
    const std::string& serializedLedgerRecord
) {
    const std::string marker =
        ";payload=";

    const std::size_t start =
        serializedLedgerRecord.find(marker);

    if (start == std::string::npos) {
        throw std::invalid_argument("Serialized LedgerRecord missing payload.");
    }

    const std::size_t valueStart =
        start + marker.size();

    const std::size_t valueEnd =
        serializedLedgerRecord.rfind('}');

    if (valueEnd == std::string::npos ||
        valueEnd <= valueStart) {
        throw std::invalid_argument("Serialized LedgerRecord payload is malformed.");
    }

    return serializedLedgerRecord.substr(
        valueStart,
        valueEnd - valueStart
    );
}

core::LedgerRecord decodeLedgerRecord(
    const std::string& serializedLedgerRecord
) {
    if (serializedLedgerRecord.rfind("LedgerRecord{", 0) != 0) {
        throw std::invalid_argument("Serialized data is not a LedgerRecord.");
    }

    return core::LedgerRecord::fromPersistedFields(
        extractSimpleField(serializedLedgerRecord, "id"),
        core::ledgerRecordTypeFromString(
            extractSimpleField(serializedLedgerRecord, "type")
        ),
        extractSimpleField(serializedLedgerRecord, "sourceId"),
        extractLedgerPayload(serializedLedgerRecord),
        extractSimpleField(serializedLedgerRecord, "payloadHash"),
        std::stoll(extractSimpleField(serializedLedgerRecord, "timestamp"))
    );
}

std::vector<std::string> splitSerializedRecords(
    const std::string& recordsPayload
) {
    std::vector<std::string> records;
    std::size_t start = 0;
    int braceDepth = 0;
    int bracketDepth = 0;

    for (std::size_t index = 0; index < recordsPayload.size(); ++index) {
        const char current =
            recordsPayload[index];

        if (current == '{') {
            ++braceDepth;
        } else if (current == '}') {
            --braceDepth;
        } else if (current == '[') {
            ++bracketDepth;
        } else if (current == ']') {
            --bracketDepth;
        } else if (current == ',' &&
                   braceDepth == 0 &&
                   bracketDepth == 0) {
            records.push_back(
                recordsPayload.substr(
                    start,
                    index - start
                )
            );

            start = index + 1;
        }
    }

    if (start < recordsPayload.size()) {
        records.push_back(
            recordsPayload.substr(start)
        );
    }

    return records;
}

std::vector<core::LedgerRecord> decodeRecordsFromBlockSerialization(
    const std::string& serializedBlock
) {
    const std::string marker =
        "records=[";

    const std::size_t recordsStart =
        serializedBlock.find(marker);

    if (recordsStart == std::string::npos) {
        throw std::invalid_argument("Serialized Block does not contain records.");
    }

    const std::size_t contentStart =
        recordsStart + marker.size();

    int bracketDepth = 1;
    int braceDepth = 0;
    std::size_t contentEnd = std::string::npos;

    for (std::size_t index = contentStart; index < serializedBlock.size(); ++index) {
        const char current =
            serializedBlock[index];

        if (current == '{') {
            ++braceDepth;
        } else if (current == '}') {
            --braceDepth;
        } else if (current == '[') {
            ++bracketDepth;
        } else if (current == ']') {
            --bracketDepth;

            if (bracketDepth == 0 &&
                braceDepth == 0) {
                contentEnd = index;
                break;
            }
        }
    }

    if (contentEnd == std::string::npos ||
        contentEnd <= contentStart) {
        throw std::invalid_argument("Serialized Block record list is malformed.");
    }

    const std::string recordsPayload =
        serializedBlock.substr(
            contentStart,
            contentEnd - contentStart
        );

    std::vector<core::LedgerRecord> records;

    for (const std::string& serializedRecord : splitSerializedRecords(recordsPayload)) {
        records.push_back(
            decodeLedgerRecord(serializedRecord)
        );
    }

    return records;
}

core::Block decodeSerializedBlock(
    const std::string& serializedBlock
) {
    if (serializedBlock.rfind("Block{", 0) != 0) {
        throw std::invalid_argument("Serialized data is not a Block.");
    }

    const std::uint64_t index =
        static_cast<std::uint64_t>(
            std::stoull(extractSimpleField(serializedBlock, "index"))
        );

    const std::string previousHash =
        extractSimpleField(serializedBlock, "previousHash");

    const std::string expectedHash =
        extractSimpleField(serializedBlock, "hash");

    const std::int64_t timestamp =
        std::stoll(extractSimpleField(serializedBlock, "timestamp"));

    core::Block block(
        index,
        previousHash,
        decodeRecordsFromBlockSerialization(serializedBlock),
        timestamp
    );

    if (block.hash() != expectedHash) {
        throw std::invalid_argument("Persisted Block hash does not match reconstructed block.");
    }

    return block;
}

} // namespace

std::string runtimeStateLoadStatusToString(
    RuntimeStateLoadStatus status
) {
    switch (status) {
        case RuntimeStateLoadStatus::LOADED:
            return "LOADED";
        case RuntimeStateLoadStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case RuntimeStateLoadStatus::NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        case RuntimeStateLoadStatus::GENESIS_MISMATCH:
            return "GENESIS_MISMATCH";
        case RuntimeStateLoadStatus::RUNTIME_START_FAILED:
            return "RUNTIME_START_FAILED";
        case RuntimeStateLoadStatus::BLOCK_FILE_MISSING:
            return "BLOCK_FILE_MISSING";
        case RuntimeStateLoadStatus::BLOCK_FILE_INVALID:
            return "BLOCK_FILE_INVALID";
        case RuntimeStateLoadStatus::BLOCK_APPEND_FAILED:
            return "BLOCK_APPEND_FAILED";
        case RuntimeStateLoadStatus::MANIFEST_MISMATCH:
            return "MANIFEST_MISMATCH";
        case RuntimeStateLoadStatus::MEMPOOL_LOAD_FAILED:
            return "MEMPOOL_LOAD_FAILED";
        default:
            return "BLOCK_FILE_INVALID";
    }
}

RuntimeStateLoadResult::RuntimeStateLoadResult()
    : m_status(RuntimeStateLoadStatus::INVALID_CONFIG),
      m_reason("Uninitialized runtime state load result."),
      m_runtime(),
      m_manifest(),
      m_loadedBlockCount(0),
      m_loadedMempoolTransactionCount(0) {}

RuntimeStateLoadResult RuntimeStateLoadResult::loaded(
    NodeRuntime runtime,
    NodeRuntimeManifest manifest,
    std::size_t loadedBlockCount,
    std::size_t loadedMempoolTransactionCount
) {
    RuntimeStateLoadResult result;
    result.m_status = RuntimeStateLoadStatus::LOADED;
    result.m_reason = "";
    result.m_runtime = std::move(runtime);
    result.m_manifest = std::move(manifest);
    result.m_loadedBlockCount = loadedBlockCount;
    result.m_loadedMempoolTransactionCount = loadedMempoolTransactionCount;
    return result;
}

RuntimeStateLoadResult RuntimeStateLoadResult::rejected(
    RuntimeStateLoadStatus status,
    std::string reason
) {
    RuntimeStateLoadResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

RuntimeStateLoadStatus RuntimeStateLoadResult::status() const {
    return m_status;
}

const std::string& RuntimeStateLoadResult::reason() const {
    return m_reason;
}

bool RuntimeStateLoadResult::loaded() const {
    return m_status == RuntimeStateLoadStatus::LOADED &&
           m_runtime.isValid() &&
           m_manifest.isValid();
}

const NodeRuntime& RuntimeStateLoadResult::runtime() const {
    return m_runtime;
}

const NodeRuntimeManifest& RuntimeStateLoadResult::manifest() const {
    return m_manifest;
}

std::size_t RuntimeStateLoadResult::loadedBlockCount() const {
    return m_loadedBlockCount;
}

std::size_t RuntimeStateLoadResult::loadedMempoolTransactionCount() const {
    return m_loadedMempoolTransactionCount;
}

std::string RuntimeStateLoadResult::serialize() const {
    std::ostringstream oss;

    oss << "RuntimeStateLoadResult{"
        << "status=" << runtimeStateLoadStatusToString(m_status)
        << ";reason=" << m_reason
        << ";loadedBlockCount=" << m_loadedBlockCount
        << ";loadedMempoolTransactionCount=" << m_loadedMempoolTransactionCount
        << ";manifest=" << (m_manifest.isValid() ? m_manifest.serialize() : "NONE")
        << "}";

    return oss.str();
}

core::Block FinalizedBlockFileCodec::readBlockFile(
    const std::filesystem::path& path
) {
    return decodeBlockFileContents(
        readTextFile(path)
    );
}

core::Block FinalizedBlockFileCodec::decodeBlockFileContents(
    const std::string& contents
) {
    const std::map<std::string, std::string> fields =
        parseKeyValueLines(contents);

    /*
     * V2 stores explicit block fields and record lines, but the canonical block
     * serialization is still kept as an integrity anchor.
     */
    const std::string serializedBlock =
        requireField(fields, "block");

    core::Block block =
        decodeSerializedBlock(serializedBlock);

    const std::uint64_t index =
        static_cast<std::uint64_t>(
            std::stoull(requireField(fields, "blockIndex"))
        );

    const std::string blockHash =
        requireField(fields, "blockHash");

    const std::string previousHash =
        requireField(fields, "previousHash");

    if (block.index() != index ||
        block.hash() != blockHash ||
        block.previousHash() != previousHash) {
        throw std::invalid_argument("Finalized block file header does not match block payload.");
    }

    return block;
}

RuntimeStateLoadResult RuntimeStateLoader::loadFromDataDirectory(
    const NodeDataDirectoryConfig& directoryConfig,
    const config::GenesisConfig& genesisConfig,
    const p2p::PeerInfo& localPeer
) {
    if (!directoryConfig.isValid() ||
        !genesisConfig.isValid() ||
        !localPeer.isValid()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::INVALID_CONFIG,
            "Runtime loader config is invalid."
        );
    }

    const NodeDataDirectoryReadResult manifestResult =
        NodeDataDirectory::loadManifest(directoryConfig);

    if (!manifestResult.loaded()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::NOT_INITIALIZED,
            manifestResult.reason()
        );
    }

    const NodeRuntimeManifest manifest =
        manifestResult.manifest();

    if (manifest.genesisConfigId() != genesisConfig.deterministicId()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::GENESIS_MISMATCH,
            "Data directory genesis does not match loader genesis config."
        );
    }

    const NodeRuntimeConfig runtimeConfig(
        genesisConfig,
        localPeer,
        genesisConfig.networkParameters().maxPeerCount()
    );

    const NodeRuntimeStartResult start =
        NodeRuntimeFactory::startFromGenesis(runtimeConfig);

    if (!start.started()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::RUNTIME_START_FAILED,
            start.reason()
        );
    }

    NodeRuntime runtime =
        start.runtime();

    std::size_t loadedBlockCount = 0;

    for (std::uint64_t height = 1; height <= manifest.latestBlockHeight(); ++height) {
        const std::filesystem::path blockPath =
            FinalizedBlockStore::blockFilePath(
                directoryConfig,
                height
            );

        if (!std::filesystem::exists(blockPath)) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_FILE_MISSING,
                "Missing finalized block file: " + blockPath.string()
            );
        }

        core::Block block(
            0,
            "GENESIS",
            runtime.blockchain().genesisBlock().records(),
            runtime.blockchain().genesisBlock().timestamp()
        );

        try {
            block = FinalizedBlockFileCodec::readBlockFile(blockPath);
        } catch (const std::exception& error) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
                error.what()
            );
        }

        if (!runtime.mutableBlockchain().canAppendBlock(block)) {
            return RuntimeStateLoadResult::rejected(
                RuntimeStateLoadStatus::BLOCK_APPEND_FAILED,
                "Persisted block cannot append to rebuilt runtime chain."
            );
        }

        runtime.mutableBlockchain().addBlock(block);
        ++loadedBlockCount;
    }

    if (runtime.blockchain().latestBlock().index() != manifest.latestBlockHeight() ||
        runtime.blockchain().latestBlock().hash() != manifest.latestBlockHash()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            "Rebuilt chain latest block does not match manifest."
        );
    }

    const PersistentMempoolLoadResult mempoolLoad =
        PersistentMempoolStore::loadIntoMempool(
            directoryConfig,
            runtime.mutableMempool(),
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION
        );

    if (!mempoolLoad.loaded()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::MEMPOOL_LOAD_FAILED,
            mempoolLoad.reason()
        );
    }

    if (!runtime.isValid()) {
        return RuntimeStateLoadResult::rejected(
            RuntimeStateLoadStatus::RUNTIME_START_FAILED,
            "Rebuilt runtime failed final audit."
        );
    }

    return RuntimeStateLoadResult::loaded(
        runtime,
        manifest,
        loadedBlockCount,
        mempoolLoad.loadedTransactionCount()
    );
}

} // namespace nodo::node
