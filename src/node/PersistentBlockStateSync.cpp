#include "node/PersistentBlockStateSync.hpp"
#include "node/FastSyncSnapshotService.hpp"
#include "node/FastSyncSnapshotStore.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "core/ProtocolLimits.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/FinalizedSlashingEvidenceAudit.hpp"
#include "node/CanonicalSlashingTransition.hpp"
#include "storage/SlashingEvidenceStore.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "serialization/BlockCodec.hpp"
#include "serialization/CanonicalHash.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

constexpr std::size_t MAX_SERIALIZED_BLOCK_BYTES =
    core::ProtocolLimits::MAX_SERIALIZED_BLOCK_BYTES;
constexpr std::size_t MAX_SERIALIZED_FINALIZED_RECORD_BYTES =
    core::ProtocolLimits::MAX_SERIALIZED_BLOCK_BYTES;
constexpr std::size_t MAX_SYNC_CODEC_FIELD_BYTES = 16 * 1024 * 1024;
constexpr const char* CODEC_VERSION = "NODO_PERSISTENT_BLOCK_STATE_SYNC_CODEC_V1";

bool isSafeScalar(const std::string& value, std::size_t maxSize = 240) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::map<std::string, std::string> parseCheckpointFields(const std::string& serialized) {
    std::map<std::string, std::string> fields;
    std::istringstream input(serialized);
    std::string line;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 >= line.size()) {
            throw std::invalid_argument("Malformed checkpoint line.");
        }

        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);

        if (!fields.emplace(key, value).second) {
            throw std::invalid_argument("Duplicate checkpoint field: " + key);
        }
    }

    return fields;
}


std::vector<std::string> finalizedSlashingEvidenceIds(const core::Block& block) {
    std::vector<std::string> ids;
    for (const consensus::DoubleVoteEvidence& evidence :
         CanonicalSlashingTransition::doubleVoteEvidenceFromBlock(block)) {
        ids.push_back(evidence.evidenceId());
    }
    for (const consensus::ProposerEquivocationEvidence& evidence :
         CanonicalSlashingTransition::proposerEquivocationEvidenceFromBlock(block)) {
        ids.push_back(evidence.evidenceId());
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

bool removeFinalizedSlashingEvidenceFromPendingStores(
    const std::vector<core::Block>& finalizedBlocks,
    consensus::EvidencePool* pendingEvidencePool,
    storage::SlashingEvidenceStore* pendingEvidenceStore,
    std::string& reason
) {
    try {
        for (const core::Block& block : finalizedBlocks) {
            for (const std::string& evidenceId : finalizedSlashingEvidenceIds(block)) {
                bool removedFromPool = true;
                if (pendingEvidencePool != nullptr &&
                    pendingEvidencePool->contains(evidenceId)) {
                    removedFromPool = pendingEvidencePool->removeEvidence(evidenceId);
                }
                if (!removedFromPool) {
                    reason = "Finalized slashing evidence could not be removed from pending pool: " + evidenceId;
                    return false;
                }

                if (pendingEvidenceStore != nullptr &&
                    pendingEvidenceStore->contains(evidenceId) &&
                    !pendingEvidenceStore->erase(evidenceId)) {
                    reason = "Finalized slashing evidence could not be removed from pending store: " + evidenceId;
                    return false;
                }
            }
        }
    } catch (const std::exception& error) {
        reason = std::string("Finalized slashing evidence cleanup failed: ") + error.what();
        return false;
    }
    return true;
}

std::string requireField(
    const std::map<std::string, std::string>& fields,
    const std::string& key
) {
    const auto found = fields.find(key);
    if (found == fields.end()) {
        throw std::invalid_argument("Missing checkpoint field: " + key);
    }
    return found->second;
}

std::uint64_t parseU64Strict(const std::string& value, const std::string& fieldName) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (const char character : value) {
        if (character < '0' || character > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const auto parsed = static_cast<std::uint64_t>(std::stoull(value, &parsedSize));
    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }
    return parsed;
}

std::int64_t parseI64Strict(const std::string& value, const std::string& fieldName) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char character = value[index];
        if (character == '-' && index == 0 && value.size() > 1) {
            continue;
        }
        if (character < '0' || character > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const auto parsed = static_cast<std::int64_t>(std::stoll(value, &parsedSize));
    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }
    return parsed;
}

void writeHeader(serialization::CanonicalWriter& writer, const std::string& typeName) {
    writer.writeString(CODEC_VERSION);
    writer.writeString(typeName);
}

void readHeader(serialization::CanonicalReader& reader, const std::string& expectedType) {
    const std::string version = reader.readString();
    const std::string type = reader.readString();

    if (version != CODEC_VERSION) {
        throw std::runtime_error("Unsupported persistent sync codec version.");
    }

    if (type != expectedType) {
        throw std::runtime_error("Unexpected persistent sync codec type.");
    }
}

std::uint32_t encodeStatus(PersistentSyncStatus status) {
    return static_cast<std::uint32_t>(status);
}

PersistentSyncStatus decodeStatus(std::uint32_t value) {
    if (value > static_cast<std::uint32_t>(PersistentSyncStatus::FAILED)) {
        return PersistentSyncStatus::UNKNOWN;
    }
    return static_cast<PersistentSyncStatus>(value);
}

} // namespace

std::string persistentSyncStatusToString(PersistentSyncStatus status) {
    switch (status) {
        case PersistentSyncStatus::IDLE:
            return "IDLE";
        case PersistentSyncStatus::SYNCING:
            return "SYNCING";
        case PersistentSyncStatus::COMPLETE:
            return "COMPLETE";
        case PersistentSyncStatus::FAILED:
            return "FAILED";
        case PersistentSyncStatus::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

PersistentSyncStatus persistentSyncStatusFromString(const std::string& value) {
    if (value == "IDLE") return PersistentSyncStatus::IDLE;
    if (value == "SYNCING") return PersistentSyncStatus::SYNCING;
    if (value == "COMPLETE") return PersistentSyncStatus::COMPLETE;
    if (value == "FAILED") return PersistentSyncStatus::FAILED;
    return PersistentSyncStatus::UNKNOWN;
}

PersistentSyncCheckpoint::PersistentSyncCheckpoint()
    : m_schemaVersion(),
      m_finalizedHeight(0),
      m_finalizedBlockHash(),
      m_finalizedStateRoot(),
      m_status(PersistentSyncStatus::UNKNOWN),
      m_sourcePeerId(),
      m_updatedAt(0) {}

PersistentSyncCheckpoint::PersistentSyncCheckpoint(
    std::string schemaVersion,
    std::uint64_t finalizedHeight,
    std::string finalizedBlockHash,
    std::string finalizedStateRoot,
    PersistentSyncStatus status,
    std::string sourcePeerId,
    std::int64_t updatedAt
) : m_schemaVersion(std::move(schemaVersion)),
    m_finalizedHeight(finalizedHeight),
    m_finalizedBlockHash(std::move(finalizedBlockHash)),
    m_finalizedStateRoot(std::move(finalizedStateRoot)),
    m_status(status),
    m_sourcePeerId(std::move(sourcePeerId)),
    m_updatedAt(updatedAt) {}

const std::string& PersistentSyncCheckpoint::schemaVersion() const { return m_schemaVersion; }
std::uint64_t PersistentSyncCheckpoint::finalizedHeight() const { return m_finalizedHeight; }
const std::string& PersistentSyncCheckpoint::finalizedBlockHash() const { return m_finalizedBlockHash; }
const std::string& PersistentSyncCheckpoint::finalizedStateRoot() const { return m_finalizedStateRoot; }
PersistentSyncStatus PersistentSyncCheckpoint::status() const { return m_status; }
const std::string& PersistentSyncCheckpoint::sourcePeerId() const { return m_sourcePeerId; }
std::int64_t PersistentSyncCheckpoint::updatedAt() const { return m_updatedAt; }

bool PersistentSyncCheckpoint::isValid() const {
    return m_schemaVersion == SCHEMA_VERSION &&
           isSafeScalar(m_finalizedBlockHash) &&
           isSafeScalar(m_finalizedStateRoot) &&
           isSafeScalar(m_sourcePeerId) &&
           m_status != PersistentSyncStatus::UNKNOWN &&
           m_updatedAt > 0;
}

std::string PersistentSyncCheckpoint::serialize() const {
    std::ostringstream output;
    output << "schemaVersion=" << m_schemaVersion << '\n'
           << "finalizedHeight=" << m_finalizedHeight << '\n'
           << "finalizedBlockHash=" << m_finalizedBlockHash << '\n'
           << "finalizedStateRoot=" << m_finalizedStateRoot << '\n'
           << "status=" << persistentSyncStatusToString(m_status) << '\n'
           << "sourcePeerId=" << m_sourcePeerId << '\n'
           << "updatedAt=" << m_updatedAt << '\n';
    return output.str();
}

PersistentSyncCheckpoint PersistentSyncCheckpoint::genesis(
    const std::string& genesisBlockHash,
    const std::string& genesisStateRoot,
    std::int64_t now
) {
    return PersistentSyncCheckpoint(
        SCHEMA_VERSION,
        0,
        genesisBlockHash,
        genesisStateRoot,
        PersistentSyncStatus::COMPLETE,
        "LOCAL_GENESIS",
        now
    );
}

PersistentSyncCheckpoint PersistentSyncCheckpoint::deserialize(
    const std::string& serialized
) {
    const auto fields = parseCheckpointFields(serialized);

    PersistentSyncCheckpoint checkpoint(
        requireField(fields, "schemaVersion"),
        parseU64Strict(requireField(fields, "finalizedHeight"), "finalizedHeight"),
        requireField(fields, "finalizedBlockHash"),
        requireField(fields, "finalizedStateRoot"),
        persistentSyncStatusFromString(requireField(fields, "status")),
        requireField(fields, "sourcePeerId"),
        parseI64Strict(requireField(fields, "updatedAt"), "updatedAt")
    );

    if (!checkpoint.isValid()) {
        throw std::invalid_argument("Persistent sync checkpoint is invalid.");
    }

    return checkpoint;
}

PersistentSyncCheckpointStore::PersistentSyncCheckpointStore(
    std::filesystem::path dataDirectory
) : m_dataDirectory(std::move(dataDirectory)) {}

const std::filesystem::path& PersistentSyncCheckpointStore::dataDirectory() const {
    return m_dataDirectory;
}

std::filesystem::path PersistentSyncCheckpointStore::checkpointFilePath() const {
    return m_dataDirectory / "sync" / "checkpoint.conf";
}

std::string persistentSyncCheckpointReadStatusToString(
    PersistentSyncCheckpointReadStatus status
) {
    switch (status) {
        case PersistentSyncCheckpointReadStatus::LOADED:     return "LOADED";
        case PersistentSyncCheckpointReadStatus::MISSING:    return "MISSING";
        case PersistentSyncCheckpointReadStatus::MALFORMED:  return "MALFORMED";
        case PersistentSyncCheckpointReadStatus::INVALID:    return "INVALID";
        case PersistentSyncCheckpointReadStatus::IO_FAILURE: return "IO_FAILURE";
        default:                                             return "IO_FAILURE";
    }
}

PersistentSyncCheckpointReadResult::PersistentSyncCheckpointReadResult()
    : m_status(PersistentSyncCheckpointReadStatus::IO_FAILURE),
      m_reason("Uninitialized."),
      m_checkpoint() {}

// static
PersistentSyncCheckpointReadResult PersistentSyncCheckpointReadResult::loaded(
    PersistentSyncCheckpoint checkpoint
) {
    PersistentSyncCheckpointReadResult result;
    result.m_status = PersistentSyncCheckpointReadStatus::LOADED;
    result.m_reason = "";
    result.m_checkpoint = std::move(checkpoint);
    return result;
}

// static
PersistentSyncCheckpointReadResult PersistentSyncCheckpointReadResult::missing() {
    PersistentSyncCheckpointReadResult result;
    result.m_status = PersistentSyncCheckpointReadStatus::MISSING;
    result.m_reason = "Checkpoint file does not exist.";
    return result;
}

// static
PersistentSyncCheckpointReadResult PersistentSyncCheckpointReadResult::malformed(std::string reason) {
    PersistentSyncCheckpointReadResult result;
    result.m_status = PersistentSyncCheckpointReadStatus::MALFORMED;
    result.m_reason = std::move(reason);
    return result;
}

// static
PersistentSyncCheckpointReadResult PersistentSyncCheckpointReadResult::invalid(std::string reason) {
    PersistentSyncCheckpointReadResult result;
    result.m_status = PersistentSyncCheckpointReadStatus::INVALID;
    result.m_reason = std::move(reason);
    return result;
}

// static
PersistentSyncCheckpointReadResult PersistentSyncCheckpointReadResult::ioFailure(std::string reason) {
    PersistentSyncCheckpointReadResult result;
    result.m_status = PersistentSyncCheckpointReadStatus::IO_FAILURE;
    result.m_reason = std::move(reason);
    return result;
}

PersistentSyncCheckpointReadStatus PersistentSyncCheckpointReadResult::status() const {
    return m_status;
}

const std::string& PersistentSyncCheckpointReadResult::reason() const {
    return m_reason;
}

bool PersistentSyncCheckpointReadResult::loaded() const {
    return m_status == PersistentSyncCheckpointReadStatus::LOADED;
}

const PersistentSyncCheckpoint& PersistentSyncCheckpointReadResult::checkpoint() const {
    return m_checkpoint;
}

std::string persistentSyncCheckpointWriteStatusToString(
    PersistentSyncCheckpointWriteStatus status
) {
    switch (status) {
        case PersistentSyncCheckpointWriteStatus::SAVED:              return "SAVED";
        case PersistentSyncCheckpointWriteStatus::INVALID_CHECKPOINT: return "INVALID_CHECKPOINT";
        case PersistentSyncCheckpointWriteStatus::IO_FAILURE:         return "IO_FAILURE";
        default:                                                      return "IO_FAILURE";
    }
}

PersistentSyncCheckpointWriteResult::PersistentSyncCheckpointWriteResult()
    : m_status(PersistentSyncCheckpointWriteStatus::IO_FAILURE),
      m_reason("Uninitialized.") {}

// static
PersistentSyncCheckpointWriteResult PersistentSyncCheckpointWriteResult::saved() {
    PersistentSyncCheckpointWriteResult result;
    result.m_status = PersistentSyncCheckpointWriteStatus::SAVED;
    result.m_reason = "";
    return result;
}

// static
PersistentSyncCheckpointWriteResult PersistentSyncCheckpointWriteResult::invalidCheckpoint(
    std::string reason
) {
    PersistentSyncCheckpointWriteResult result;
    result.m_status = PersistentSyncCheckpointWriteStatus::INVALID_CHECKPOINT;
    result.m_reason = std::move(reason);
    return result;
}

// static
PersistentSyncCheckpointWriteResult PersistentSyncCheckpointWriteResult::ioFailure(
    std::string reason
) {
    PersistentSyncCheckpointWriteResult result;
    result.m_status = PersistentSyncCheckpointWriteStatus::IO_FAILURE;
    result.m_reason = std::move(reason);
    return result;
}

PersistentSyncCheckpointWriteStatus PersistentSyncCheckpointWriteResult::status() const {
    return m_status;
}

const std::string& PersistentSyncCheckpointWriteResult::reason() const {
    return m_reason;
}

bool PersistentSyncCheckpointWriteResult::isSaved() const {
    return m_status == PersistentSyncCheckpointWriteStatus::SAVED;
}

bool PersistentSyncCheckpointStore::exists() const {
    return std::filesystem::exists(checkpointFilePath());
}

PersistentSyncCheckpointReadResult PersistentSyncCheckpointStore::read() const {
    const auto path = checkpointFilePath();

    try {
        if (!std::filesystem::exists(path)) {
            return PersistentSyncCheckpointReadResult::missing();
        }
    } catch (const std::exception& error) {
        return PersistentSyncCheckpointReadResult::ioFailure(
            "Filesystem error checking checkpoint: " + std::string(error.what())
        );
    }

    std::string text;
    try {
        text = storage::AtomicFile::readTextFile(path);
    } catch (const std::exception& error) {
        return PersistentSyncCheckpointReadResult::ioFailure(
            "Failed to read checkpoint file: " + std::string(error.what())
        );
    }

    PersistentSyncCheckpoint checkpoint;
    try {
        checkpoint = PersistentSyncCheckpoint::deserialize(text);
    } catch (const std::invalid_argument& error) {
        return PersistentSyncCheckpointReadResult::malformed(
            "Checkpoint file is malformed: " + std::string(error.what())
        );
    } catch (const std::exception& error) {
        return PersistentSyncCheckpointReadResult::malformed(
            "Checkpoint deserialization failed: " + std::string(error.what())
        );
    }

    if (!checkpoint.isValid()) {
        return PersistentSyncCheckpointReadResult::invalid(
            "Checkpoint file parsed but failed validation."
        );
    }

    return PersistentSyncCheckpointReadResult::loaded(std::move(checkpoint));
}

PersistentSyncCheckpointWriteResult PersistentSyncCheckpointStore::save(
    const PersistentSyncCheckpoint& checkpoint
) const {
    if (!checkpoint.isValid()) {
        return PersistentSyncCheckpointWriteResult::invalidCheckpoint(
            "Refusing to save invalid persistent sync checkpoint."
        );
    }

    try {
        const auto path = checkpointFilePath();
        std::filesystem::create_directories(path.parent_path());
        storage::AtomicFile::writeTextFile(path, checkpoint.serialize());
    } catch (const std::exception& error) {
        return PersistentSyncCheckpointWriteResult::ioFailure(
            "Failed to write checkpoint file: " + std::string(error.what())
        );
    }

    return PersistentSyncCheckpointWriteResult::saved();
}

PersistentBlockSyncItem::PersistentBlockSyncItem()
    : m_height(0),
      m_blockHash(),
      m_previousBlockHash(),
      m_serializedBlock(),
      m_finalizedStateRoot(),
      m_createdAt(0),
      m_serializedFinalizedRecord() {}

PersistentBlockSyncItem::PersistentBlockSyncItem(
    std::uint64_t height,
    std::string blockHash,
    std::string previousBlockHash,
    std::string serializedBlock,
    std::string finalizedStateRoot,
    std::int64_t createdAt,
    std::string serializedFinalizedRecord
) : m_height(height),
    m_blockHash(std::move(blockHash)),
    m_previousBlockHash(std::move(previousBlockHash)),
    m_serializedBlock(std::move(serializedBlock)),
    m_finalizedStateRoot(std::move(finalizedStateRoot)),
    m_createdAt(createdAt),
    m_serializedFinalizedRecord(std::move(serializedFinalizedRecord)) {}

std::uint64_t PersistentBlockSyncItem::height() const { return m_height; }
const std::string& PersistentBlockSyncItem::blockHash() const { return m_blockHash; }
const std::string& PersistentBlockSyncItem::previousBlockHash() const { return m_previousBlockHash; }
const std::string& PersistentBlockSyncItem::serializedBlock() const { return m_serializedBlock; }
const std::string& PersistentBlockSyncItem::finalizedStateRoot() const { return m_finalizedStateRoot; }
std::int64_t PersistentBlockSyncItem::createdAt() const { return m_createdAt; }
const std::string& PersistentBlockSyncItem::serializedFinalizedRecord() const { return m_serializedFinalizedRecord; }

bool PersistentBlockSyncItem::isValid() const {
    return m_height > 0 &&
           core::Block::isCanonicalCommitmentRoot(m_blockHash) &&
           core::Block::isCanonicalCommitmentRoot(m_previousBlockHash) &&
           !m_serializedBlock.empty() &&
           m_serializedBlock.size() <= MAX_SERIALIZED_BLOCK_BYTES &&
           core::Block::isCanonicalCommitmentRoot(m_finalizedStateRoot) &&
           m_serializedFinalizedRecord.size() <=
               MAX_SERIALIZED_FINALIZED_RECORD_BYTES &&
           m_createdAt > 0;
}

std::string PersistentBlockSyncItem::serialize() const {
    std::ostringstream output;
    output << "PersistentBlockSyncItem{height=" << m_height
           << ";blockHash=" << m_blockHash
           << ";previousBlockHash=" << m_previousBlockHash
           << ";serializedBlockBytes=" << m_serializedBlock.size()
           << ";finalizedStateRoot=" << m_finalizedStateRoot
           << ";createdAt=" << m_createdAt
           << ";hasFinalizedRecord=" << (!m_serializedFinalizedRecord.empty() ? "true" : "false")
           << "}";
    return output.str();
}

PersistentBlockSyncBatch::PersistentBlockSyncBatch()
    : m_sourcePeerId(),
      m_fromHeight(0),
      m_toHeight(0),
      m_items(),
      m_createdAt(0) {}

PersistentBlockSyncBatch::PersistentBlockSyncBatch(
    std::string sourcePeerId,
    std::uint64_t fromHeight,
    std::uint64_t toHeight,
    std::vector<PersistentBlockSyncItem> items,
    std::int64_t createdAt
) : m_sourcePeerId(std::move(sourcePeerId)),
    m_fromHeight(fromHeight),
    m_toHeight(toHeight),
    m_items(std::move(items)),
    m_createdAt(createdAt) {}

const std::string& PersistentBlockSyncBatch::sourcePeerId() const { return m_sourcePeerId; }
std::uint64_t PersistentBlockSyncBatch::fromHeight() const { return m_fromHeight; }
std::uint64_t PersistentBlockSyncBatch::toHeight() const { return m_toHeight; }
const std::vector<PersistentBlockSyncItem>& PersistentBlockSyncBatch::items() const { return m_items; }
std::int64_t PersistentBlockSyncBatch::createdAt() const { return m_createdAt; }

std::uint64_t PersistentBlockSyncBatch::blockCount() const {
    return static_cast<std::uint64_t>(m_items.size());
}

const PersistentBlockSyncItem* PersistentBlockSyncBatch::lastItem() const {
    if (m_items.empty()) {
        return nullptr;
    }
    return &m_items.back();
}

bool PersistentBlockSyncBatch::isValid() const {
    if (!isSafeScalar(m_sourcePeerId) ||
        m_fromHeight == 0 ||
        m_toHeight < m_fromHeight ||
        m_items.empty() ||
        m_items.size() > NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH ||
        m_createdAt <= 0) {
        return false;
    }

    const std::uint64_t expectedCount = m_toHeight - m_fromHeight + 1;
    if (expectedCount != m_items.size()) {
        return false;
    }

    for (std::size_t index = 0; index < m_items.size(); ++index) {
        const auto& item = m_items[index];
        if (!item.isValid()) {
            return false;
        }

        if (item.height() != m_fromHeight + index) {
            return false;
        }

        if (item.createdAt() > m_createdAt) {
            return false;
        }

        if (index > 0 && item.previousBlockHash() != m_items[index - 1].blockHash()) {
            return false;
        }
    }

    return true;
}

std::string PersistentBlockSyncBatch::serialize() const {
    std::ostringstream output;
    output << "PersistentBlockSyncBatch{sourcePeerId=" << m_sourcePeerId
           << ";fromHeight=" << m_fromHeight
           << ";toHeight=" << m_toHeight
           << ";blockCount=" << m_items.size()
           << ";createdAt=" << m_createdAt
           << ";items=[";

    for (std::size_t index = 0; index < m_items.size(); ++index) {
        if (index != 0) {
            output << ",";
        }
        output << m_items[index].serialize();
    }

    output << "]}";
    return output.str();
}

PersistentSnapshotSyncManifest::PersistentSnapshotSyncManifest()
    : m_sourcePeerId(),
      m_snapshotHeight(0),
      m_snapshotBlockHash(),
      m_snapshotStateRoot(),
      m_snapshotDigest(),
      m_createdAt(0) {}

PersistentSnapshotSyncManifest::PersistentSnapshotSyncManifest(
    std::string sourcePeerId,
    std::uint64_t snapshotHeight,
    std::string snapshotBlockHash,
    std::string snapshotStateRoot,
    std::string snapshotDigest,
    std::int64_t createdAt
) : m_sourcePeerId(std::move(sourcePeerId)),
    m_snapshotHeight(snapshotHeight),
    m_snapshotBlockHash(std::move(snapshotBlockHash)),
    m_snapshotStateRoot(std::move(snapshotStateRoot)),
    m_snapshotDigest(std::move(snapshotDigest)),
    m_createdAt(createdAt) {}

const std::string& PersistentSnapshotSyncManifest::sourcePeerId() const { return m_sourcePeerId; }
std::uint64_t PersistentSnapshotSyncManifest::snapshotHeight() const { return m_snapshotHeight; }
const std::string& PersistentSnapshotSyncManifest::snapshotBlockHash() const { return m_snapshotBlockHash; }
const std::string& PersistentSnapshotSyncManifest::snapshotStateRoot() const { return m_snapshotStateRoot; }
const std::string& PersistentSnapshotSyncManifest::snapshotDigest() const { return m_snapshotDigest; }
std::int64_t PersistentSnapshotSyncManifest::createdAt() const { return m_createdAt; }

bool PersistentSnapshotSyncManifest::isValid() const {
    return isSafeScalar(m_sourcePeerId) &&
           m_snapshotHeight > 0 &&
           isSafeScalar(m_snapshotBlockHash) &&
           isSafeScalar(m_snapshotStateRoot) &&
           isSafeScalar(m_snapshotDigest) &&
           m_createdAt > 0;
}

std::string PersistentSnapshotSyncManifest::serialize() const {
    std::ostringstream output;
    output << "PersistentSnapshotSyncManifest{sourcePeerId=" << m_sourcePeerId
           << ";snapshotHeight=" << m_snapshotHeight
           << ";snapshotBlockHash=" << m_snapshotBlockHash
           << ";snapshotStateRoot=" << m_snapshotStateRoot
           << ";snapshotDigest=" << m_snapshotDigest
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

// static
PersistentSnapshotSyncManifest PersistentSnapshotSyncManifest::deserialize(
    const std::string& serialized
) {
    const std::string prefix = "PersistentSnapshotSyncManifest{";
    if (serialized.size() < prefix.size() + 1 ||
        serialized.substr(0, prefix.size()) != prefix ||
        serialized.back() != '}') {
        throw std::invalid_argument("Invalid PersistentSnapshotSyncManifest format.");
    }

    const std::string inner = serialized.substr(
        prefix.size(),
        serialized.size() - prefix.size() - 1
    );

    std::map<std::string, std::string> fields;
    std::istringstream stream(inner);
    std::string token;
    while (std::getline(stream, token, ';')) {
        if (token.empty()) {
            continue;
        }
        const auto eq = token.find('=');
        if (eq == std::string::npos || eq == 0) {
            throw std::invalid_argument("Malformed manifest field: " + token);
        }
        const std::string key = token.substr(0, eq);
        const std::string value = token.substr(eq + 1);
        if (!fields.emplace(key, value).second) {
            throw std::invalid_argument("Duplicate manifest field: " + key);
        }
    }

    const std::string sourcePeerId     = requireField(fields, "sourcePeerId");
    const std::string snapshotBlockHash = requireField(fields, "snapshotBlockHash");
    const std::string snapshotStateRoot = requireField(fields, "snapshotStateRoot");
    const std::string snapshotDigest    = requireField(fields, "snapshotDigest");
    const std::uint64_t snapshotHeight  = parseU64Strict(requireField(fields, "snapshotHeight"), "snapshotHeight");
    const std::int64_t  createdAt       = parseI64Strict(requireField(fields, "createdAt"), "createdAt");

    return PersistentSnapshotSyncManifest(
        sourcePeerId,
        snapshotHeight,
        snapshotBlockHash,
        snapshotStateRoot,
        snapshotDigest,
        createdAt
    );
}

std::string persistentSyncPlanStatusToString(PersistentSyncPlanStatus status) {
    switch (status) {
        case PersistentSyncPlanStatus::NOT_REQUIRED:
            return "NOT_REQUIRED";
        case PersistentSyncPlanStatus::REQUEST_BLOCKS:
            return "REQUEST_BLOCKS";
        case PersistentSyncPlanStatus::REQUEST_SNAPSHOT:
            return "REQUEST_SNAPSHOT";
        case PersistentSyncPlanStatus::REJECTED:
        default:
            return "REJECTED";
    }
}

PersistentSyncPlan::PersistentSyncPlan()
    : m_status(PersistentSyncPlanStatus::REJECTED),
      m_reason("Uninitialized persistent sync plan."),
      m_blockRequest(std::nullopt),
      m_snapshotRequest(std::nullopt) {}

PersistentSyncPlan::PersistentSyncPlan(
    PersistentSyncPlanStatus status,
    std::string reason,
    std::optional<NetworkBlockSyncRequest> blockRequest,
    std::optional<PersistentSnapshotSyncManifest> snapshotRequest
) : m_status(status),
    m_reason(std::move(reason)),
    m_blockRequest(std::move(blockRequest)),
    m_snapshotRequest(std::move(snapshotRequest)) {}

PersistentSyncPlanStatus PersistentSyncPlan::status() const { return m_status; }
const std::string& PersistentSyncPlan::reason() const { return m_reason; }
const std::optional<NetworkBlockSyncRequest>& PersistentSyncPlan::blockRequest() const { return m_blockRequest; }
const std::optional<PersistentSnapshotSyncManifest>& PersistentSyncPlan::snapshotRequest() const { return m_snapshotRequest; }
bool PersistentSyncPlan::requestBlocks() const { return m_status == PersistentSyncPlanStatus::REQUEST_BLOCKS && m_blockRequest.has_value(); }
bool PersistentSyncPlan::requestSnapshot() const { return m_status == PersistentSyncPlanStatus::REQUEST_SNAPSHOT; }
bool PersistentSyncPlan::notRequired() const { return m_status == PersistentSyncPlanStatus::NOT_REQUIRED; }
bool PersistentSyncPlan::rejected() const { return m_status == PersistentSyncPlanStatus::REJECTED; }

std::string PersistentSyncPlan::serialize() const {
    std::ostringstream output;
    output << "PersistentSyncPlan{status=" << persistentSyncPlanStatusToString(m_status)
           << ";reason=" << m_reason
           << ";hasBlockRequest=" << (m_blockRequest.has_value() ? "true" : "false")
           << ";hasSnapshotRequest=" << (m_snapshotRequest.has_value() ? "true" : "false")
           << "}";
    return output.str();
}

PersistentSyncPlan PersistentBlockStateSyncPlanner::planFromRemoteStatus(
    const PersistentSyncCheckpoint& localCheckpoint,
    const ChainStatusMessage& remoteStatus,
    const std::string& localNodeId,
    const std::string& sourcePeerId,
    std::uint64_t maxBlocksPerRequest,
    std::int64_t now
) {
    if (!localCheckpoint.isValid()) {
        return PersistentSyncPlan(
            PersistentSyncPlanStatus::REJECTED,
            "Local persistent sync checkpoint is invalid.",
            std::nullopt
        );
    }

    if (!remoteStatus.isValid()) {
        return PersistentSyncPlan(
            PersistentSyncPlanStatus::REJECTED,
            "Remote chain status is invalid.",
            std::nullopt
        );
    }

    if (!isSafeScalar(localNodeId) || !isSafeScalar(sourcePeerId) || now <= 0) {
        return PersistentSyncPlan(
            PersistentSyncPlanStatus::REJECTED,
            "Sync planner identifiers or timestamp are invalid.",
            std::nullopt
        );
    }

    if (remoteStatus.finalizedHeight() <= localCheckpoint.finalizedHeight()) {
        return PersistentSyncPlan(
            PersistentSyncPlanStatus::NOT_REQUIRED,
            "Local checkpoint is already at or ahead of remote finalized state.",
            std::nullopt
        );
    }

    if (maxBlocksPerRequest == 0 || maxBlocksPerRequest > NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH) {
        maxBlocksPerRequest = NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH;
    }

    const std::uint64_t heightGap =
        remoteStatus.finalizedHeight() - localCheckpoint.finalizedHeight();

    const std::uint64_t fromHeight = localCheckpoint.finalizedHeight() + 1;
    const std::uint64_t requestedCount = std::min(heightGap, maxBlocksPerRequest);

    NetworkBlockSyncRequest request(
        localNodeId,
        BlockLocator(
            fromHeight,
            requestedCount,
            {localCheckpoint.finalizedBlockHash()}
        ),
        now
    );

    return PersistentSyncPlan(
        PersistentSyncPlanStatus::REQUEST_BLOCKS,
        "Remote peer is ahead; request the next validated block window.",
        request
    );
}

std::string persistentSyncApplyStatusToString(PersistentSyncApplyStatus status) {
    switch (status) {
        case PersistentSyncApplyStatus::APPLIED:
            return "APPLIED";
        case PersistentSyncApplyStatus::STALE:
            return "STALE";
        case PersistentSyncApplyStatus::REJECTED:
        default:
            return "REJECTED";
    }
}

PersistentSyncApplyResult::PersistentSyncApplyResult()
    : m_status(PersistentSyncApplyStatus::REJECTED),
      m_reason("Uninitialized persistent sync apply result."),
      m_checkpoint(std::nullopt) {}

PersistentSyncApplyResult::PersistentSyncApplyResult(
    PersistentSyncApplyStatus status,
    std::string reason,
    std::optional<PersistentSyncCheckpoint> checkpoint
) : m_status(status),
    m_reason(std::move(reason)),
    m_checkpoint(std::move(checkpoint)) {}

PersistentSyncApplyStatus PersistentSyncApplyResult::status() const { return m_status; }
const std::string& PersistentSyncApplyResult::reason() const { return m_reason; }
const std::optional<PersistentSyncCheckpoint>& PersistentSyncApplyResult::checkpoint() const { return m_checkpoint; }
bool PersistentSyncApplyResult::applied() const { return m_status == PersistentSyncApplyStatus::APPLIED && m_checkpoint.has_value(); }
bool PersistentSyncApplyResult::stale() const { return m_status == PersistentSyncApplyStatus::STALE; }

std::string PersistentSyncApplyResult::serialize() const {
    std::ostringstream output;
    output << "PersistentSyncApplyResult{status=" << persistentSyncApplyStatusToString(m_status)
           << ";reason=" << m_reason
           << ";hasCheckpoint=" << (m_checkpoint.has_value() ? "true" : "false")
           << "}";
    return output.str();
}

PersistentSyncApplyResult PersistentBlockStateSyncApplier::applyValidatedBatch(
    const PersistentSyncCheckpoint& checkpoint,
    const PersistentBlockSyncBatch& batch,
    core::Blockchain& blockchain,
    const core::ValidatorRegistry& validatorRegistry,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    std::function<core::StateTransitionPreviewContext(const core::Blockchain&)> contextBuilder,
    std::int64_t now,
    const core::ValidatorSetHistory* validatorSetHistory
) {
    // Shape validation runs before mandatory finality-proof and protocol-state
    // verification. A synchronized block is never accepted from commitments
    // alone.
    if (!checkpoint.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Current checkpoint is invalid.",
            std::nullopt
        );
    }

    if (!batch.isValid() || now <= 0) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Block sync batch or timestamp is invalid.",
            std::nullopt
        );
    }

    if (batch.fromHeight() != checkpoint.finalizedHeight() + 1) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Batch does not start at the next expected height.",
            std::nullopt
        );
    }

    if (batch.items().front().previousBlockHash() != checkpoint.finalizedBlockHash()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Batch first block does not connect to the current checkpoint.",
            std::nullopt
        );
    }

    core::ValidatorSetHistory stagedValidatorSetHistory;
    const core::ValidatorSetHistory* effectiveValidatorSetHistory = nullptr;
    if (validatorSetHistory != nullptr) {
        stagedValidatorSetHistory = *validatorSetHistory;
        for (const auto& item : batch.items()) {
            if (!stagedValidatorSetHistory.hasSet(item.height())) {
                const std::uint64_t previousHeight = item.height() - 1;
                if (!stagedValidatorSetHistory.hasSet(previousHeight) ||
                    !stagedValidatorSetHistory.recordSet(
                        item.height(), stagedValidatorSetHistory.setAt(previousHeight)
                    )) {
                    return PersistentSyncApplyResult(
                        PersistentSyncApplyStatus::REJECTED,
                        "Historical validator-set continuity is missing at block height "
                            + std::to_string(item.height()) + ".",
                        std::nullopt
                    );
                }
            }
        }
        effectiveValidatorSetHistory = &stagedValidatorSetHistory;
    }

    // Every synchronized block must carry a cryptographically valid
    // FinalizedBlockRecord proving consensus finality.
    for (const auto& item : batch.items()) {
        if (item.serializedFinalizedRecord().empty()) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Block at height " + std::to_string(item.height()) +
                    " is missing a required FinalizedBlockRecord.",
                std::nullopt
            );
        }

        consensus::FinalizedBlockRecord record;
        try {
            record = consensus::FinalizedBlockRecord::deserialize(
                item.serializedFinalizedRecord()
            );
        } catch (const std::exception& error) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Failed to deserialize FinalizedBlockRecord at height " +
                    std::to_string(item.height()) + ": " + error.what(),
                std::nullopt
            );
        }

        if (record.blockHash() != item.blockHash()) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "FinalizedBlockRecord blockHash does not match sync item blockHash at height " +
                    std::to_string(item.height()) + ".",
                std::nullopt
            );
        }

        const core::ValidatorRegistry* qcValidatorSet = &validatorRegistry;
        if (effectiveValidatorSetHistory != nullptr) {
            if (!effectiveValidatorSetHistory->hasSet(item.height())) {
                return PersistentSyncApplyResult(
                    PersistentSyncApplyStatus::REJECTED,
                    "Historical validator set is missing for block height "
                        + std::to_string(item.height()) + ".",
                    std::nullopt
                );
            }
            qcValidatorSet = &effectiveValidatorSetHistory->setAt(item.height());
        }

        if (!record.verify(*qcValidatorSet, policy, provider)) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "QuorumCertificate verification failed for block at height " +
                    std::to_string(item.height()) + ".",
                std::nullopt
            );
        }
    }

    // Full protocol commitment validation for each block. All writes
    // target a staged copy so failure of a later block cannot partially mutate
    // the caller's canonical chain.
    core::Blockchain stagedBlockchain = blockchain;
    // The context is rebuilt from the current chain state before each block so
    // that stateRoot and receiptsRoot are computed against all previously
    // applied blocks.  A single root mismatch aborts the entire batch.
    for (const auto& item : batch.items()) {
        std::optional<core::Block> blockOpt;
        try {
            blockOpt.emplace(serialization::BlockCodec::deserialize(item.serializedBlock()));
        } catch (const std::exception& error) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Failed to deserialize block at height " +
                    std::to_string(item.height()) + ": " + error.what(),
                std::nullopt
            );
        }

        const core::Block& block = blockOpt.value();
        if (block.index() != item.height() ||
            block.hash() != item.blockHash() ||
            block.previousHash() != item.previousBlockHash() ||
            block.stateRoot() != item.finalizedStateRoot()) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Serialized block identity or state commitment does not match sync metadata at height "
                    + std::to_string(item.height()) + ".",
                std::nullopt
            );
        }

        const consensus::FinalizedBlockRecord record =
            consensus::FinalizedBlockRecord::deserialize(item.serializedFinalizedRecord());
        if (record.blockIndex() != block.index() ||
            record.blockHash() != block.hash() ||
            record.previousHash() != block.previousHash()) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "FinalizedBlockRecord is not bound to the serialized block at height "
                    + std::to_string(item.height()) + ".",
                std::nullopt
            );
        }

        const core::StateTransitionPreviewContext context =
            contextBuilder(stagedBlockchain);

        const core::BlockValidationResult validation =
            core::BlockStateTransitionValidator::validateCandidateBlock(
                stagedBlockchain,
                blockOpt.value(),
                context
                // defaults to BlockValidationMode::ProtocolCommitment
            );

        if (!validation.accepted()) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Block failed protocol commitment validation at height " +
                    std::to_string(item.height()) + ": " + validation.reason(),
                std::nullopt
            );
        }

        try {
            stagedBlockchain.addBlock(blockOpt.value());
        } catch (const std::exception& error) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Failed to add block at height " +
                    std::to_string(item.height()) + " to blockchain: " + error.what(),
                std::nullopt
            );
        }
    }

    const PersistentBlockSyncItem* last = batch.lastItem();
    if (last == nullptr) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Batch has no final item.",
            std::nullopt
        );
    }

    PersistentSyncCheckpoint updatedCheckpoint(
        PersistentSyncCheckpoint::SCHEMA_VERSION,
        last->height(),
        last->blockHash(),
        last->finalizedStateRoot(),
        PersistentSyncStatus::COMPLETE,
        batch.sourcePeerId(),
        now
    );

    if (!updatedCheckpoint.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Updated checkpoint would be invalid.",
            std::nullopt
        );
    }

    blockchain = std::move(stagedBlockchain);

    return PersistentSyncApplyResult(
        PersistentSyncApplyStatus::APPLIED,
        "Protocol-commitment block sync batch advanced the durable checkpoint.",
        updatedCheckpoint
    );
}

PersistentSyncApplyResult PersistentBlockStateSyncApplier::importFinalizedBatch(
    const PersistentSyncCheckpoint& checkpoint,
    const PersistentBlockSyncBatch& batch,
    NodeRuntime& runtime,
    const NodeDataDirectoryConfig& directoryConfig,
    PersistentSyncCheckpointStore* checkpointStore,
    std::int64_t now,
    consensus::EvidencePool* pendingEvidencePool,
    storage::SlashingEvidenceStore* pendingEvidenceStore
) {
    if (!checkpoint.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Sync checkpoint is invalid for finalized sync import.",
            std::nullopt
        );
    }
    if (!batch.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Finalized sync batch is structurally invalid.",
            std::nullopt
        );
    }
    if (!runtime.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Runtime is invalid for finalized sync import.",
            std::nullopt
        );
    }
    if (!directoryConfig.isValid() || now <= 0) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Data directory or import timestamp is invalid for finalized "
            "sync import.",
            std::nullopt
        );
    }
    if (batch.createdAt() > now + 300) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Finalized sync batch timestamp is too far in the future.",
            std::nullopt
        );
    }
    if (runtime.blockchain().latestBlock().index() != checkpoint.finalizedHeight() ||
        runtime.blockchain().latestBlock().hash() != checkpoint.finalizedBlockHash()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Sync checkpoint does not match the local canonical tip.",
            std::nullopt
        );
    }

    const std::uint64_t localFinalizedHeight = checkpoint.finalizedHeight();
    if (batch.fromHeight() > localFinalizedHeight + 1) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Finalized sync batch starts at height " +
                std::to_string(batch.fromHeight()) +
                " but the local finalized tip is at height " +
                std::to_string(localFinalizedHeight) + ".",
            std::nullopt
        );
    }

    // A batch may legitimately overlap blocks this node already finalized:
    // the tip advances between the sync request and this response whenever
    // local consensus keeps running or two peers answered the same request.
    // Every overlapping item must describe the exact block already on the
    // local chain (batch item heights are contiguous, so the overlap is a
    // prefix); only the remainder is imported.
    const auto& localBlocks = runtime.blockchain().blocks();
    std::size_t firstNewItemIndex = 0;
    for (; firstNewItemIndex < batch.items().size(); ++firstNewItemIndex) {
        const PersistentBlockSyncItem& overlapping =
            batch.items()[firstNewItemIndex];
        if (overlapping.height() > localFinalizedHeight) {
            break;
        }
        const core::Block& localBlock =
            localBlocks[static_cast<std::size_t>(overlapping.height())];
        if (localBlock.hash() != overlapping.blockHash()) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Finalized sync batch disagrees with the local chain at "
                "already-finalized height " +
                    std::to_string(overlapping.height()) + ".",
                std::nullopt
            );
        }
    }
    if (firstNewItemIndex == batch.items().size()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::STALE,
            "Finalized sync batch is entirely behind the local tip; the "
            "local chain already contains every batch block.",
            std::nullopt
        );
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            runtime.config().genesisConfig().networkParameters().networkName()
        );
    if (!cryptoContext.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Protocol crypto context is invalid for finalized sync import.",
            std::nullopt
        );
    }

    NodeRuntime stagedRuntime = runtime;
    std::vector<RuntimeBlockPipelineResult> stagedResults;
    std::vector<core::Block> stagedImportedBlocks;
    stagedResults.reserve(batch.items().size() - firstNewItemIndex);
    stagedImportedBlocks.reserve(batch.items().size() - firstNewItemIndex);

    for (std::size_t itemIndex = firstNewItemIndex;
         itemIndex < batch.items().size(); ++itemIndex) {
        const PersistentBlockSyncItem& item = batch.items()[itemIndex];
        try {
            const core::Block block =
                serialization::BlockCodec::deserialize(item.serializedBlock());
            const consensus::FinalizedBlockRecord remoteRecord =
                consensus::FinalizedBlockRecord::deserialize(
                    item.serializedFinalizedRecord()
                );

            if (item.createdAt() > now + 300 ||
                remoteRecord.finalizedAt() > now + 300) {
                return PersistentSyncApplyResult(
                    PersistentSyncApplyStatus::REJECTED,
                    "Finalized sync item timestamp is too far in the future at height "
                        + std::to_string(item.height()) + ".",
                    std::nullopt
                );
            }

            if (block.index() != item.height() ||
                block.hash() != item.blockHash() ||
                block.previousHash() != item.previousBlockHash() ||
                block.stateRoot() != item.finalizedStateRoot() ||
                !remoteRecord.matchesBlock(block)) {
                return PersistentSyncApplyResult(
                    PersistentSyncApplyStatus::REJECTED,
                    "Finalized sync item identity mismatch at height "
                        + std::to_string(item.height()) + ".",
                    std::nullopt
                );
            }

            if (!stagedRuntime.validatorSetHistory().hasSet(item.height())) {
                return PersistentSyncApplyResult(
                    PersistentSyncApplyStatus::REJECTED,
                    "Historical validator set is unavailable at height "
                        + std::to_string(item.height()) + ".",
                    std::nullopt
                );
            }
            if (!remoteRecord.verify(
                    stagedRuntime.validatorSetHistory().setAt(item.height()),
                    cryptoContext.policy(),
                    cryptoContext.validatorSignatureProvider()
                )) {
                return PersistentSyncApplyResult(
                    PersistentSyncApplyStatus::REJECTED,
                    "Historical QC verification failed at height "
                        + std::to_string(item.height()) + ".",
                    std::nullopt
                );
            }

            RuntimeBlockPipelineResult result =
                RuntimeBlockPipeline::commitCertifiedBlock(
                    stagedRuntime,
                    block,
                    remoteRecord.quorumCertificate(),
                    remoteRecord.finalizedAt(),
                    nullptr
                );
            if (!result.finalized() ||
                result.finalizedRecord().serialize() != remoteRecord.serialize()) {
                return PersistentSyncApplyResult(
                    PersistentSyncApplyStatus::REJECTED,
                    "Canonical runtime import rejected finalized block at height "
                        + std::to_string(item.height()) + ": " + result.reason(),
                    std::nullopt
                );
            }

            const FinalizedSlashingEvidenceAuditResult slashingAudit =
                FinalizedSlashingEvidenceAudit::auditBlockEffects(
                    block,
                    stagedRuntime.validatorPenaltyLedger(),
                    stagedRuntime.validatorRegistry(),
                    stagedRuntime.stakingRegistry()
                );
            if (!slashingAudit.passed()) {
                return PersistentSyncApplyResult(
                    PersistentSyncApplyStatus::REJECTED,
                    "Finalized sync slashing audit failed at height " +
                        std::to_string(item.height()) + ": " +
                        slashingAudit.reason(),
                    std::nullopt
                );
            }

            stagedImportedBlocks.push_back(block);
            stagedResults.push_back(std::move(result));
        } catch (const std::exception& error) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Finalized sync import failed at height "
                    + std::to_string(item.height()) + ": " + error.what(),
                std::nullopt
            );
        }
    }

    const FinalizedBlockStoreResult persisted = FinalizedBlockStore::persistBatch(
        directoryConfig, stagedRuntime, stagedResults, now
    );
    if (!persisted.success()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Finalized sync batch persistence failed: " + persisted.reason(),
            std::nullopt
        );
    }

    const PersistentBlockSyncItem& last = batch.items().back();
    PersistentSyncCheckpoint updated(
        PersistentSyncCheckpoint::SCHEMA_VERSION,
        last.height(), last.blockHash(), last.finalizedStateRoot(),
        PersistentSyncStatus::COMPLETE, batch.sourcePeerId(), now
    );
    if (!updated.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Finalized sync batch produced an invalid checkpoint.",
            std::nullopt
        );
    }

    std::string evidenceCleanupReason;
    if (!removeFinalizedSlashingEvidenceFromPendingStores(
            stagedImportedBlocks,
            pendingEvidencePool,
            pendingEvidenceStore,
            evidenceCleanupReason)) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            evidenceCleanupReason,
            std::nullopt
        );
    }

    // Persist checkpoint to disk BEFORE mutating in-memory runtime.
    // If the process crashes after this write but before the runtime swap,
    // the on-disk state is consistent: blocks and checkpoint agree.
    if (checkpointStore != nullptr) {
        const PersistentSyncCheckpointWriteResult writeResult =
            checkpointStore->save(updated);
        if (!writeResult.isSaved()) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Checkpoint persistence failed before runtime commit: " + writeResult.reason(),
                std::nullopt
            );
        }
    }

    runtime = std::move(stagedRuntime);
    return PersistentSyncApplyResult(
        PersistentSyncApplyStatus::APPLIED,
        "Finalized sync batch was atomically imported and published.",
        updated
    );
}

PersistentSyncApplyResult PersistentBlockStateSyncApplier::importSnapshot(
    const PersistentSyncCheckpoint& checkpoint,
    const PersistentSnapshotSyncManifest& manifest,
    NodeRuntime& runtime,
    const NodeDataDirectoryConfig& directoryConfig,
    PersistentSyncCheckpointStore* checkpointStore,
    std::int64_t now
) {
    if (!checkpoint.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Cannot import snapshot from an invalid local checkpoint.",
            std::nullopt
        );
    }
    if (!manifest.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Cannot import snapshot from an invalid snapshot manifest.",
            std::nullopt
        );
    }
    if (!runtime.isValid() || !directoryConfig.isValid()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Cannot import snapshot into an invalid runtime or data directory.",
            std::nullopt
        );
    }
    if (manifest.snapshotHeight() <= checkpoint.finalizedHeight()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Snapshot height is not ahead of the local checkpoint.",
            std::nullopt
        );
    }

    const FastSyncSnapshotStore snapshotStore(
        directoryConfig.runtimeDirectoryPath() / "fast_sync_snapshots"
    );
    const std::optional<FastSyncSnapshot> snapshot =
        snapshotStore.load(manifest.snapshotHeight());
    if (!snapshot.has_value()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Snapshot payload is not available locally for the advertised manifest.",
            std::nullopt
        );
    }

    const FastSyncSnapshotImportResult verification =
        FastSyncSnapshotService::verifyAndCheckpoint(
            *snapshot,
            runtime.config().genesisConfig(),
            manifest,
            nullptr,
            now
        );
    if (!verification.imported()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            verification.reason(),
            std::nullopt
        );
    }

    // Safety boundary: this implementation verifies the portable snapshot and
    // can publish a checkpoint only when the live runtime is already hydrated
    // to the same finalized boundary.  It must never advance a checkpoint ahead
    // of an in-memory runtime that still holds genesis or an older block.
    if (runtime.blockchain().latestBlock().index() != snapshot->blockHeight() ||
        runtime.blockchain().latestBlock().hash() != snapshot->blockHash()) {
        return PersistentSyncApplyResult(
            PersistentSyncApplyStatus::REJECTED,
            "Snapshot payload verified, but runtime domain hydration is not yet "
            "available for this node state. Refusing unsafe checkpoint-only import.",
            std::nullopt
        );
    }

    if (checkpointStore != nullptr) {
        const PersistentSyncCheckpointWriteResult write =
            checkpointStore->save(*verification.checkpoint());
        if (!write.isSaved()) {
            return PersistentSyncApplyResult(
                PersistentSyncApplyStatus::REJECTED,
                "Checkpoint persistence failed after snapshot verification: " + write.reason(),
                std::nullopt
            );
        }
    }

    return PersistentSyncApplyResult(
        PersistentSyncApplyStatus::APPLIED,
        "Fast-sync snapshot was verified against its manifest and checkpointed.",
        verification.checkpoint()
    );
}

std::vector<unsigned char> PersistentBlockStateSyncCodec::encodeCheckpoint(
    const PersistentSyncCheckpoint& checkpoint
) {
    serialization::CanonicalWriter writer;
    writeHeader(writer, "PersistentSyncCheckpoint");
    writer.writeString(checkpoint.schemaVersion());
    writer.writeUInt64(checkpoint.finalizedHeight());
    writer.writeString(checkpoint.finalizedBlockHash());
    writer.writeString(checkpoint.finalizedStateRoot());
    writer.writeUInt32(encodeStatus(checkpoint.status()));
    writer.writeString(checkpoint.sourcePeerId());
    writer.writeInt64(checkpoint.updatedAt());
    return writer.bytes();
}

PersistentSyncCheckpoint PersistentBlockStateSyncCodec::decodeCheckpoint(
    const std::vector<unsigned char>& bytes
) {
    serialization::CanonicalReader reader(bytes, MAX_SYNC_CODEC_FIELD_BYTES);
    readHeader(reader, "PersistentSyncCheckpoint");

    const std::string schemaVersion = reader.readString();
    const std::uint64_t finalizedHeight = reader.readUInt64();
    const std::string finalizedBlockHash = reader.readString();
    const std::string finalizedStateRoot = reader.readString();
    const PersistentSyncStatus status = decodeStatus(reader.readUInt32());
    const std::string sourcePeerId = reader.readString();
    const std::int64_t updatedAt = reader.readInt64();

    PersistentSyncCheckpoint checkpoint(
        schemaVersion,
        finalizedHeight,
        finalizedBlockHash,
        finalizedStateRoot,
        status,
        sourcePeerId,
        updatedAt
    );

    reader.requireFullyConsumed();
    if (!checkpoint.isValid()) {
        throw std::runtime_error("Decoded checkpoint is invalid.");
    }
    return checkpoint;
}

std::string PersistentBlockStateSyncCodec::hashCheckpoint(
    const PersistentSyncCheckpoint& checkpoint
) {
    return serialization::CanonicalHash::hashBytes(
        encodeCheckpoint(checkpoint),
        "NODO_PERSISTENT_SYNC_CHECKPOINT_HASH_V1"
    );
}

std::vector<unsigned char> PersistentBlockStateSyncCodec::encodeBlockSyncItem(
    const PersistentBlockSyncItem& item
) {
    serialization::CanonicalWriter writer;
    writeHeader(writer, "PersistentBlockSyncItem");
    writer.writeUInt64(item.height());
    writer.writeString(item.blockHash());
    writer.writeString(item.previousBlockHash());
    writer.writeString(item.serializedBlock());
    writer.writeString(item.finalizedStateRoot());
    writer.writeInt64(item.createdAt());
    writer.writeString(item.serializedFinalizedRecord());
    return writer.bytes();
}

PersistentBlockSyncItem PersistentBlockStateSyncCodec::decodeBlockSyncItem(
    const std::vector<unsigned char>& bytes
) {
    serialization::CanonicalReader reader(bytes, MAX_SYNC_CODEC_FIELD_BYTES);
    readHeader(reader, "PersistentBlockSyncItem");

    const std::uint64_t height = reader.readUInt64();
    const std::string blockHash = reader.readString();
    const std::string previousBlockHash = reader.readString();
    const std::string serializedBlock = reader.readString();
    const std::string finalizedStateRoot = reader.readString();
    const std::int64_t createdAt = reader.readInt64();
    const std::string serializedFinalizedRecord = reader.readString();

    PersistentBlockSyncItem item(
        height,
        blockHash,
        previousBlockHash,
        serializedBlock,
        finalizedStateRoot,
        createdAt,
        serializedFinalizedRecord
    );

    reader.requireFullyConsumed();
    if (!item.isValid()) {
        throw std::runtime_error("Decoded persistent block sync item is invalid.");
    }
    return item;
}

std::vector<unsigned char> PersistentBlockStateSyncCodec::encodeBlockSyncBatch(
    const PersistentBlockSyncBatch& batch
) {
    serialization::CanonicalWriter writer;
    writeHeader(writer, "PersistentBlockSyncBatch");
    writer.writeString(batch.sourcePeerId());
    writer.writeUInt64(batch.fromHeight());
    writer.writeUInt64(batch.toHeight());
    writer.writeUInt32(static_cast<std::uint32_t>(batch.items().size()));
    for (const auto& item : batch.items()) {
        writer.writeBytes(encodeBlockSyncItem(item));
    }
    writer.writeInt64(batch.createdAt());
    return writer.bytes();
}

PersistentBlockSyncBatch PersistentBlockStateSyncCodec::decodeBlockSyncBatch(
    const std::vector<unsigned char>& bytes
) {
    serialization::CanonicalReader reader(bytes, MAX_SYNC_CODEC_FIELD_BYTES);
    readHeader(reader, "PersistentBlockSyncBatch");

    const std::string sourcePeerId = reader.readString();
    const std::uint64_t fromHeight = reader.readUInt64();
    const std::uint64_t toHeight = reader.readUInt64();
    const std::uint32_t itemCount = reader.readUInt32();

    if (itemCount > NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH) {
        throw std::runtime_error("Decoded persistent block sync batch is too large.");
    }

    std::vector<PersistentBlockSyncItem> items;
    items.reserve(itemCount);
    for (std::uint32_t index = 0; index < itemCount; ++index) {
        items.push_back(decodeBlockSyncItem(reader.readBytes()));
    }

    const std::int64_t createdAt = reader.readInt64();

    PersistentBlockSyncBatch batch(
        sourcePeerId,
        fromHeight,
        toHeight,
        items,
        createdAt
    );

    reader.requireFullyConsumed();
    if (!batch.isValid()) {
        throw std::runtime_error("Decoded persistent block sync batch is invalid.");
    }
    return batch;
}

std::string PersistentBlockStateSyncCodec::hashBlockSyncBatch(
    const PersistentBlockSyncBatch& batch
) {
    return serialization::CanonicalHash::hashBytes(
        encodeBlockSyncBatch(batch),
        "NODO_PERSISTENT_BLOCK_SYNC_BATCH_HASH_V1"
    );
}

std::vector<unsigned char> PersistentBlockStateSyncCodec::encodeSnapshotManifest(
    const PersistentSnapshotSyncManifest& manifest
) {
    serialization::CanonicalWriter writer;
    writeHeader(writer, "PersistentSnapshotSyncManifest");
    writer.writeString(manifest.sourcePeerId());
    writer.writeUInt64(manifest.snapshotHeight());
    writer.writeString(manifest.snapshotBlockHash());
    writer.writeString(manifest.snapshotStateRoot());
    writer.writeString(manifest.snapshotDigest());
    writer.writeInt64(manifest.createdAt());
    return writer.bytes();
}

PersistentSnapshotSyncManifest PersistentBlockStateSyncCodec::decodeSnapshotManifest(
    const std::vector<unsigned char>& bytes
) {
    serialization::CanonicalReader reader(bytes, MAX_SYNC_CODEC_FIELD_BYTES);
    readHeader(reader, "PersistentSnapshotSyncManifest");

    const std::string sourcePeerId = reader.readString();
    const std::uint64_t snapshotHeight = reader.readUInt64();
    const std::string snapshotBlockHash = reader.readString();
    const std::string snapshotStateRoot = reader.readString();
    const std::string snapshotDigest = reader.readString();
    const std::int64_t createdAt = reader.readInt64();

    PersistentSnapshotSyncManifest manifest(
        sourcePeerId,
        snapshotHeight,
        snapshotBlockHash,
        snapshotStateRoot,
        snapshotDigest,
        createdAt
    );

    reader.requireFullyConsumed();
    if (!manifest.isValid()) {
        throw std::runtime_error("Decoded snapshot sync manifest is invalid.");
    }
    return manifest;
}

std::string PersistentBlockStateSyncCodec::hashSnapshotManifest(
    const PersistentSnapshotSyncManifest& manifest
) {
    return serialization::CanonicalHash::hashBytes(
        encodeSnapshotManifest(manifest),
        "NODO_PERSISTENT_SNAPSHOT_MANIFEST_HASH_V1"
    );
}

} // namespace nodo::node
