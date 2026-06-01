#include "economics/MintRecord.hpp"

#include "serialization/MintRecordCodec.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

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
    std::string authorizationId,
    std::string recipientAddress,
    utils::Amount amount,
    MintReason reason,
    std::uint64_t epoch,
    std::uint64_t sourceBlockIndex,
    std::string sourceBlockHash,
    std::int64_t timestamp
)
    : m_id(std::move(id)),
      m_authorizationId(std::move(authorizationId)),
      m_recipientAddress(std::move(recipientAddress)),
      m_amount(amount),
      m_reason(reason),
      m_epoch(epoch),
      m_sourceBlockIndex(sourceBlockIndex),
      m_sourceBlockHash(std::move(sourceBlockHash)),
      m_timestamp(timestamp) {}

const std::string& MintRecord::id() const { return m_id; }
const std::string& MintRecord::authorizationId() const { return m_authorizationId; }
const std::string& MintRecord::recipientAddress() const { return m_recipientAddress; }
utils::Amount MintRecord::amount() const { return m_amount; }
MintReason MintRecord::reason() const { return m_reason; }
std::uint64_t MintRecord::epoch() const { return m_epoch; }
std::uint64_t MintRecord::sourceBlockIndex() const { return m_sourceBlockIndex; }
const std::string& MintRecord::sourceBlockHash() const { return m_sourceBlockHash; }
std::int64_t MintRecord::timestamp() const { return m_timestamp; }

bool MintRecord::isValid() const {
    return rejectionReason().empty();
}

std::string MintRecord::rejectionReason() const {
    if (m_id.empty()) {
        return "MintRecord rejected: id is empty.";
    }
    if (m_authorizationId.empty()) {
        return "MintRecord rejected: authorizationId is empty. "
               "Every mint must be linked to a MintAuthorization.";
    }
    if (m_recipientAddress.empty()) {
        return "MintRecord rejected: recipientAddress is empty.";
    }
    if (!m_amount.isPositive()) {
        return "MintRecord rejected: amount must be positive, got " +
               std::to_string(m_amount.rawUnits()) + ".";
    }
    if (m_sourceBlockHash.empty()) {
        return "MintRecord rejected: sourceBlockHash is empty.";
    }
    if (m_timestamp <= 0) {
        return "MintRecord rejected: timestamp must be positive.";
    }
    return "";
}

std::string MintRecord::serialize() const {
    std::ostringstream oss;

    oss << "MintRecord{"
        << "id=" << m_id
        << ";authorizationId=" << m_authorizationId
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
    return serialization::MintRecordCodec::deserialize(serialized);
}

} // namespace nodo::economics
