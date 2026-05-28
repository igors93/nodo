#include "core/Block.hpp"

#include "crypto/hash.h"

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

} // namespace nodo::core