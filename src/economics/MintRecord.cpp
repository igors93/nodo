#include "economics/MintRecord.hpp"

#include <sstream>

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

    /*
     * No bloco gênesis, o hash fonte pode ser "GENESIS".
     * Nos próximos blocos, deverá ser um hash real.
     */
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

} // namespace nodo::economics