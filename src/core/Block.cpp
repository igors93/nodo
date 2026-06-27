#include "core/Block.hpp"

#include "core/LedgerRecord.hpp"
#include "core/MerkleTree.hpp"
#include "crypto/hash.h"

#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::core {

Block::Block(
    std::uint64_t index,
    std::string previousHash,
    std::vector<LedgerRecord> records,
    std::int64_t timestamp,
    std::string stateRoot,
    std::string receiptsRoot
)
    : m_index(index),
      m_previousHash(std::move(previousHash)),
      m_hash(""),
      m_records(std::move(records)),
      m_timestamp(timestamp),
      m_stateRoot(std::move(stateRoot)),
      m_receiptsRoot(std::move(receiptsRoot)) {
    if (m_previousHash.empty()) {
        throw std::invalid_argument("Block previousHash cannot be empty.");
    }

    if (m_records.empty() || m_records.size() > MAX_RECORDS) {
        throw std::invalid_argument("Block must contain at least one LedgerRecord.");
    }

    if (m_timestamp <= 0) {
        throw std::invalid_argument("Block timestamp must be positive.");
    }

    for (const auto& record : m_records) {
        if (!record.isValid() || record.payload().size() > MAX_RECORD_PAYLOAD_BYTES) {
            throw std::invalid_argument("Block rejected invalid LedgerRecord.");
        }
    }

    if (!isWithinResourceLimits()) {
        throw std::invalid_argument("Block exceeds canonical protocol resource limits.");
    }

    m_hash = calculateHash();
}

Block Block::createGenesisBlock(
    std::vector<LedgerRecord> records,
    std::int64_t timestamp,
    std::string stateRoot
) {
    return Block(
        0,
        "GENESIS",
        std::move(records),
        timestamp,
        std::move(stateRoot)
    );
}

std::uint64_t Block::index() const {
    return m_index;
}

const std::string& Block::previousHash() const {
    return m_previousHash;
}

const std::string& Block::hash() const {
    return m_hash;
}

std::int64_t Block::timestamp() const {
    return m_timestamp;
}

const std::vector<LedgerRecord>& Block::records() const {
    return m_records;
}

const std::string& Block::stateRoot() const {
    return m_stateRoot;
}

const std::string& Block::receiptsRoot() const {
    return m_receiptsRoot;
}

bool Block::isGenesisBlock() const {
    return m_index == 0 && m_previousHash == "GENESIS";
}

// static
bool Block::isCanonicalCommitmentRoot(const std::string& root) {
    // A protocol commitment root is a lowercase hex-encoded SHA-256 hash.
    // The hasher always produces exactly 64 hex characters.
    if (root.size() != 64) {
        return false;
    }
    for (const char c : root) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

bool Block::hasCanonicalStateRoot() const {
    return isCanonicalCommitmentRoot(m_stateRoot);
}

bool Block::hasCanonicalReceiptsRoot() const {
    return isCanonicalCommitmentRoot(m_receiptsRoot);
}

bool Block::isValid(bool requireProtocolCommitments) const {
    if (m_previousHash.empty()) {
        return false;
    }

    if (m_hash.empty()) {
        return false;
    }

    if (m_records.empty() || !isWithinResourceLimits()) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    for (const auto& record : m_records) {
        if (!record.isValid()) {
            return false;
        }
    }

    if (m_hash != calculateHash()) {
        return false;
    }

    if (m_index == 0 && m_previousHash != "GENESIS") {
        return false;
    }

    if (m_index > 0 && m_previousHash == "GENESIS") {
        return false;
    }

    if (requireProtocolCommitments && m_index > 0) {
        // Both roots must be canonical 64-char lowercase hex hashes.
        // This rejects empty strings, placeholder labels, and any non-hex input.
        if (!isCanonicalCommitmentRoot(m_stateRoot) ||
            !isCanonicalCommitmentRoot(m_receiptsRoot)) {
            return false;
        }
    }

    return true;
}

bool Block::isWithinResourceLimits() const {
    if (m_records.empty() || m_records.size() > MAX_RECORDS) {
        return false;
    }
    std::size_t totalBytes = 0;
    for (const LedgerRecord& record : m_records) {
        if (record.payload().size() > MAX_RECORD_PAYLOAD_BYTES) {
            return false;
        }
        const std::size_t recordBytes = record.serialize().size();
        if (recordBytes > MAX_SERIALIZED_BYTES - totalBytes) {
            return false;
        }
        totalBytes += recordBytes;
    }
    return totalBytes <= MAX_SERIALIZED_BYTES;
}

std::string Block::headerPayload() const {
    std::ostringstream oss;

    /*
     * The order of these fields must remain stable.
     * Block hash depends on this exact representation.
     *
     * Records are committed via a Merkle root instead of concatenation.
     * Direct concatenation with a ',' separator created a second-preimage
     * risk: an address containing ',' could produce an ambiguous serialisation
     * where two distinct record sets yield the same headerPayload string, and
     * therefore the same block hash.  MerkleTree::buildRoot hashes each leaf
     * independently with a domain-separated prefix, eliminating that ambiguity.
     */
    std::vector<std::string> recordPayloads;
    recordPayloads.reserve(m_records.size());
    for (const auto& record : m_records) {
        recordPayloads.push_back(record.serialize());
    }
    const std::string recordsMerkleRoot = MerkleTree::buildRoot(recordPayloads);

    oss << "BlockHeader{"
        << "index=" << m_index
        << ";previousHash=" << m_previousHash
        << ";timestamp=" << m_timestamp
        << ";recordsMerkleRoot=" << recordsMerkleRoot
        << ";stateRoot=" << m_stateRoot
        << ";receiptsRoot=" << m_receiptsRoot
        << ";records=[";

    for (std::size_t i = 0; i < m_records.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << recordPayloads[i];
    }

    oss << "]}";

    return oss.str();
}

std::string Block::serialize() const {
    std::ostringstream oss;

    oss << "Block{"
        << "index=" << m_index
        << ";previousHash=" << m_previousHash
        << ";hash=" << m_hash
        << ";timestamp=" << m_timestamp
        << ";stateRoot=" << m_stateRoot
        << ";receiptsRoot=" << m_receiptsRoot
        << ";recordCount=" << m_records.size()
        << ";payload=" << headerPayload()
        << "}";

    return oss.str();
}

std::string Block::calculateHash() const {
    return hashString(headerPayload());
}

std::string Block::hashString(const std::string& value) {
    char output[65] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));

    return std::string(output);
}

// static
std::optional<Block> Block::deserialize(const std::string& text) {
    if (text.size() > MAX_SERIALIZED_BYTES) return std::nullopt;
    if (text.rfind("Block{", 0) != 0) return std::nullopt;
    if (text.size() < 8 || text.back() != '}') return std::nullopt;

    // Extract top-level fields.
    auto extractField = [&](const std::string& key) -> std::string {
        const std::string needle = key + "=";
        const auto pos = text.find(needle);
        if (pos == std::string::npos) return "";
        const auto start = pos + needle.size();
        int depth = 0;
        std::size_t i = start;
        while (i < text.size()) {
            const char c = text[i];
            if (c == '{' || c == '[') ++depth;
            else if (c == '}' || c == ']') {
                if (depth == 0) break;
                --depth;
            } else if (c == ';' && depth == 0) {
                break;
            }
            ++i;
        }
        return text.substr(start, i - start);
    };

    const std::string indexStr    = extractField("index");
    const std::string prevHash    = extractField("previousHash");
    const std::string storedHash  = extractField("hash");
    const std::string tsStr       = extractField("timestamp");
    const std::string stateRoot   = extractField("stateRoot");
    const std::string receiptsRoot = extractField("receiptsRoot");
    const std::string countStr    = extractField("recordCount");
    const std::string payload     = extractField("payload");

    if (indexStr.empty() ||
        prevHash.empty() ||
        storedHash.empty() ||
        tsStr.empty() ||
        countStr.empty() ||
        payload.empty()) {
        return std::nullopt;
    }

    std::uint64_t index = 0;
    std::int64_t  timestamp = 0;
    std::uint64_t recordCount = 0;
    try {
        std::size_t parsed = 0;
        const unsigned long long parsedIndex = std::stoull(indexStr, &parsed);
        if (parsed != indexStr.size() ||
            parsedIndex > static_cast<unsigned long long>(
                std::numeric_limits<std::uint64_t>::max()
            )) {
            return std::nullopt;
        }
        index = static_cast<std::uint64_t>(parsedIndex);

        parsed = 0;
        timestamp = std::stoll(tsStr, &parsed);
        if (parsed != tsStr.size()) {
            return std::nullopt;
        }

        parsed = 0;
        const unsigned long long parsedRecordCount =
            std::stoull(countStr, &parsed);
        if (parsed != countStr.size() ||
            parsedRecordCount > static_cast<unsigned long long>(
                std::numeric_limits<std::uint64_t>::max()
            )) {
            return std::nullopt;
        }
        recordCount = static_cast<std::uint64_t>(parsedRecordCount);
    } catch (...) {
        return std::nullopt;
    }
    if (recordCount == 0) return std::nullopt;

    // Extract the records array from inside the payload (BlockHeader{...;records=[...]}).
    if (payload.rfind("BlockHeader{", 0) != 0 || payload.size() < 16) {
        return std::nullopt;
    }

    const std::string recordsKey = "records=[";
    const auto recPos = payload.find(recordsKey);
    if (recPos == std::string::npos) return std::nullopt;

    const auto arrayStart = recPos + recordsKey.size();
    // Find the matching ']' for the records array.
    int depth = 0;
    std::size_t arrayEnd = arrayStart;
    while (arrayEnd < payload.size()) {
        const char c = payload[arrayEnd];
        if (c == '[') ++depth;
        else if (c == ']') {
            if (depth == 0) break;
            --depth;
        }
        ++arrayEnd;
    }
    if (arrayEnd >= payload.size() ||
        payload[arrayEnd] != ']' ||
        arrayEnd + 2 != payload.size() ||
        payload[arrayEnd + 1] != '}') {
        return std::nullopt;
    }
    const std::string arrayContent = payload.substr(arrayStart, arrayEnd - arrayStart);

    // Split arrayContent into individual LedgerRecord strings by tracking brace depth.
    std::vector<LedgerRecord> records;
    std::size_t i = 0;
    while (i < arrayContent.size()) {
        // Skip commas and whitespace between records.
        while (i < arrayContent.size() &&
               (arrayContent[i] == ',' || arrayContent[i] == ' ')) {
            ++i;
        }
        if (i >= arrayContent.size()) break;
        if (arrayContent.substr(i, 13) != "LedgerRecord{") return std::nullopt;

        const std::size_t start = i;
        int d = 0;
        bool completeRecord = false;
        while (i < arrayContent.size()) {
            const char c = arrayContent[i];
            if (c == '{') ++d;
            else if (c == '}') {
                --d;
                if (d == 0) {
                    ++i;
                    completeRecord = true;
                    break;
                }
            }
            ++i;
        }
        if (!completeRecord) return std::nullopt;

        auto rec = LedgerRecord::deserialize(arrayContent.substr(start, i - start));
        if (!rec.has_value()) return std::nullopt;
        records.push_back(std::move(rec.value()));
    }

    if (records.empty()) return std::nullopt;
    if (recordCount > static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max()
        )) {
        return std::nullopt;
    }
    if (records.size() != static_cast<std::size_t>(recordCount)) {
        return std::nullopt;
    }

    try {
        Block block(index, prevHash, std::move(records), timestamp, stateRoot, receiptsRoot);
        // Verify hash integrity.
        if (block.hash() != storedHash) return std::nullopt;
        if (block.serialize() != text) return std::nullopt;
        return block;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace nodo::core
