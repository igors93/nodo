#include "core/Block.hpp"

#include "core/LedgerRecord.hpp"
#include "crypto/hash.h"

#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

Block::Block(
    std::uint64_t index,
    std::string previousHash,
    std::vector<LedgerRecord> records,
    std::int64_t timestamp
)
    : m_index(index),
      m_previousHash(std::move(previousHash)),
      m_hash(""),
      m_records(std::move(records)),
      m_timestamp(timestamp) {
    if (m_previousHash.empty()) {
        throw std::invalid_argument("Block previousHash cannot be empty.");
    }

    if (m_records.empty()) {
        throw std::invalid_argument("Block must contain at least one LedgerRecord.");
    }

    if (m_timestamp <= 0) {
        throw std::invalid_argument("Block timestamp must be positive.");
    }

    for (const auto& record : m_records) {
        if (!record.isValid()) {
            throw std::invalid_argument("Block rejected invalid LedgerRecord.");
        }
    }

    m_hash = calculateHash();
}

Block Block::createGenesisBlock(
    std::vector<LedgerRecord> records,
    std::int64_t timestamp
) {
    return Block(
        0,
        "GENESIS",
        std::move(records),
        timestamp
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

bool Block::isGenesisBlock() const {
    return m_index == 0 && m_previousHash == "GENESIS";
}

bool Block::isValid() const {
    if (m_previousHash.empty()) {
        return false;
    }

    if (m_hash.empty()) {
        return false;
    }

    if (m_records.empty()) {
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

    return true;
}

std::string Block::headerPayload() const {
    std::ostringstream oss;

    /*
     * The order of these fields must remain stable.
     * Block hash depends on this exact representation.
     */
    oss << "BlockHeader{"
        << "index=" << m_index
        << ";previousHash=" << m_previousHash
        << ";timestamp=" << m_timestamp
        << ";records=[";

    for (std::size_t i = 0; i < m_records.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }

        oss << m_records[i].serialize();
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
    if (text.rfind("Block{", 0) != 0) return std::nullopt;

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
    const std::string payload     = extractField("payload");

    if (indexStr.empty() || prevHash.empty() || tsStr.empty() || payload.empty())
        return std::nullopt;

    std::uint64_t index = 0;
    std::int64_t  timestamp = 0;
    try {
        index     = static_cast<std::uint64_t>(std::stoull(indexStr));
        timestamp = std::stoll(tsStr);
    } catch (...) {
        return std::nullopt;
    }

    // Extract the records array from inside the payload (BlockHeader{...;records=[...]}).
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
        if (arrayContent.substr(i, 13) != "LedgerRecord{") break;

        const std::size_t start = i;
        int d = 0;
        while (i < arrayContent.size()) {
            const char c = arrayContent[i];
            if (c == '{') ++d;
            else if (c == '}') {
                --d;
                if (d == 0) { ++i; break; }
            }
            ++i;
        }

        auto rec = LedgerRecord::deserialize(arrayContent.substr(start, i - start));
        if (!rec.has_value()) return std::nullopt;
        records.push_back(std::move(rec.value()));
    }

    if (records.empty()) return std::nullopt;

    try {
        Block block(index, prevHash, std::move(records), timestamp);
        // Verify hash integrity.
        if (!storedHash.empty() && block.hash() != storedHash) return std::nullopt;
        return block;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace nodo::core