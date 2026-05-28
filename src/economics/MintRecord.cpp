#include "economics/MintRecord.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

namespace {

std::string extractSerializedField(
    const std::string& serialized,
    const std::string& key
) {
    const std::string prefix = key + "=";
    const std::size_t start = serialized.find(prefix);

    if (start == std::string::npos) {
        throw std::invalid_argument("Missing serialized MintRecord field: " + key);
    }

    const std::size_t valueStart = start + prefix.size();
    std::size_t valueEnd = serialized.find(';', valueStart);

    if (valueEnd == std::string::npos) {
        valueEnd = serialized.find('}', valueStart);
    }

    if (valueEnd == std::string::npos || valueEnd <= valueStart) {
        throw std::invalid_argument("Invalid serialized MintRecord field: " + key);
    }

    return serialized.substr(valueStart, valueEnd - valueStart);
}

} // namespace

std::string mintReasonToString(MintReason reason) {
    switch (reason) {
        case MintReason::GENESIS_ALLOCATION:
            return "GENESIS_ALLOCATION";

        case MintReason::NETWORK_DEFENSE_REWARD:
            return "NETWORK_DEFENSE_REWARD";

        case MintReason::LOCKED_RESERVE_REWARD:
            return "LOCKED_RESERVE_REWARD";

        case MintReason::TREASURY_REWARD:
            return "TREASURY_REWARD";

        default:
            return "UNKNOWN";
    }
}

MintReason mintReasonFromString(const std::string& value) {
    if (value == "GENESIS_ALLOCATION") {
        return MintReason::GENESIS_ALLOCATION;
    }

    if (value == "NETWORK_DEFENSE_REWARD") {
        return MintReason::NETWORK_DEFENSE_REWARD;
    }

    if (value == "LOCKED_RESERVE_REWARD") {
        return MintReason::LOCKED_RESERVE_REWARD;
    }

    if (value == "TREASURY_REWARD") {
        return MintReason::TREASURY_REWARD;
    }

    throw std::invalid_argument("Unknown MintReason: " + value);
}

MintRecord::MintRecord(
    std::string id,
    std::string recipientAddress,
    utils::Amount amount,
    MintReason reason,
    std::uint64_t epoch,
    std::uint64_t sourceBlockIndex,
    std::string sourceBlockHash,
    std::int64_t timestamp
)
    : m_id(std::move(id)),
      m_recipientAddress(std::move(recipientAddress)),
      m_amount(amount),
      m_reason(reason),
      m_epoch(epoch),
      m_sourceBlockIndex(sourceBlockIndex),
      m_sourceBlockHash(std::move(sourceBlockHash)),
      m_timestamp(timestamp) {}

const std::string& MintRecord::id() const {
    return m_id;
}

const std::string& MintRecord::recipientAddress() const {
    return m_recipientAddress;
}

utils::Amount MintRecord::amount() const {
    return m_amount;
}

MintReason MintRecord::reason() const {
    return m_reason;
}

std::uint64_t MintRecord::epoch() const {
    return m_epoch;
}

std::uint64_t MintRecord::sourceBlockIndex() const {
    return m_sourceBlockIndex;
}

const std::string& MintRecord::sourceBlockHash() const {
    return m_sourceBlockHash;
}

std::int64_t MintRecord::timestamp() const {
    return m_timestamp;
}

bool MintRecord::isValid() const {
    if (m_id.empty()) {
        return false;
    }

    if (m_recipientAddress.empty()) {
        return false;
    }

    if (!m_amount.isPositive()) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    if (m_sourceBlockHash.empty()) {
        return false;
    }

    return true;
}

std::string MintRecord::serialize() const {
    std::ostringstream oss;

    oss << "MintRecord{"
        << "id=" << m_id
        << ";recipient=" << m_recipientAddress
        << ";amountRaw=" << m_amount.rawUnits()
        << ";reason=" << mintReasonToString(m_reason)
        << ";epoch=" << m_epoch
        << ";sourceBlockIndex=" << m_sourceBlockIndex
        << ";sourceBlockHash=" << m_sourceBlockHash
        << ";timestamp=" << m_timestamp
        << "}";

    return oss.str();
}

MintRecord MintRecord::deserialize(const std::string& serialized) {
    if (serialized.rfind("MintRecord{", 0) != 0) {
        throw std::invalid_argument("Serialized data is not a MintRecord.");
    }

    const std::string id = extractSerializedField(serialized, "id");
    const std::string recipient = extractSerializedField(serialized, "recipient");
    const std::string amountRaw = extractSerializedField(serialized, "amountRaw");
    const std::string reason = extractSerializedField(serialized, "reason");
    const std::string epoch = extractSerializedField(serialized, "epoch");
    const std::string sourceBlockIndex = extractSerializedField(serialized, "sourceBlockIndex");
    const std::string sourceBlockHash = extractSerializedField(serialized, "sourceBlockHash");
    const std::string timestamp = extractSerializedField(serialized, "timestamp");

    MintRecord record(
        id,
        recipient,
        utils::Amount::fromRawUnits(std::stoll(amountRaw)),
        mintReasonFromString(reason),
        static_cast<std::uint64_t>(std::stoull(epoch)),
        static_cast<std::uint64_t>(std::stoull(sourceBlockIndex)),
        sourceBlockHash,
        std::stoll(timestamp)
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Deserialized MintRecord is invalid.");
    }

    if (record.serialize() != serialized) {
        throw std::invalid_argument("MintRecord serialization round-trip failed.");
    }

    return record;
}

} // namespace nodo::economics