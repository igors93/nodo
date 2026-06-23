#include "core/EventLog.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <utility>

namespace nodo::core {

namespace {

std::string hashString(
    const std::string& value
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));
    return std::string(output);
}

} // namespace

std::string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::TRANSFER:               return "TRANSFER";
        case EventType::STAKE_DEPOSITED:        return "STAKE_DEPOSITED";
        case EventType::STAKE_WITHDRAWN:        return "STAKE_WITHDRAWN";
        case EventType::VALIDATOR_REGISTERED:   return "VALIDATOR_REGISTERED";
        case EventType::VALIDATOR_DEREGISTERED: return "VALIDATOR_DEREGISTERED";
        case EventType::VALIDATOR_PENALIZED:    return "VALIDATOR_PENALIZED";
        case EventType::GOVERNANCE_PROPOSED:    return "GOVERNANCE_PROPOSED";
        case EventType::GOVERNANCE_VOTED:       return "GOVERNANCE_VOTED";
        case EventType::GOVERNANCE_EXECUTED:    return "GOVERNANCE_EXECUTED";
        case EventType::TOKEN_BURNED:           return "TOKEN_BURNED";
        case EventType::TOKEN_MINTED:           return "TOKEN_MINTED";
        case EventType::FEE_BURNED:             return "FEE_BURNED";
        default:                                return "UNKNOWN";
    }
}

EventLog::EventLog()
    : m_transactionId(""),
      m_blockHeight(0),
      m_type(EventType::TRANSFER),
      m_primaryAddress(""),
      m_secondaryAddress(""),
      m_amount(),
      m_data(""),
      m_timestamp(0) {}

EventLog::EventLog(
    std::string transactionId,
    std::uint64_t blockHeight,
    EventType type,
    std::string primaryAddress,
    std::string secondaryAddress,
    utils::Amount amount,
    std::string data,
    std::int64_t timestamp
)
    : m_transactionId(std::move(transactionId)),
      m_blockHeight(blockHeight),
      m_type(type),
      m_primaryAddress(std::move(primaryAddress)),
      m_secondaryAddress(std::move(secondaryAddress)),
      m_amount(amount),
      m_data(std::move(data)),
      m_timestamp(timestamp) {}

EventLog EventLog::transfer(
    const std::string& txId,
    const std::string& from,
    const std::string& to,
    utils::Amount amount,
    std::uint64_t blockHeight,
    std::int64_t timestamp
) {
    return EventLog(txId, blockHeight, EventType::TRANSFER, from, to, amount, "", timestamp);
}

EventLog EventLog::stakeDeposited(
    const std::string& txId,
    const std::string& address,
    utils::Amount amount,
    std::uint64_t blockHeight,
    std::int64_t timestamp
) {
    return EventLog(txId, blockHeight, EventType::STAKE_DEPOSITED, address, "", amount, "", timestamp);
}

EventLog EventLog::stakeWithdrawn(
    const std::string& txId,
    const std::string& address,
    utils::Amount amount,
    std::uint64_t blockHeight,
    std::int64_t timestamp
) {
    return EventLog(txId, blockHeight, EventType::STAKE_WITHDRAWN, address, "", amount, "", timestamp);
}

EventLog EventLog::validatorRegistered(
    const std::string& txId,
    const std::string& address,
    std::uint64_t blockHeight,
    std::int64_t timestamp
) {
    return EventLog(txId, blockHeight, EventType::VALIDATOR_REGISTERED,
        address, "", utils::Amount(), "", timestamp);
}

EventLog EventLog::validatorPenalized(
    const std::string& txId,
    const std::string& address,
    utils::Amount penaltyAmount,
    std::uint64_t blockHeight,
    std::int64_t timestamp
) {
    return EventLog(txId, blockHeight, EventType::VALIDATOR_PENALIZED,
        address, "", penaltyAmount, "", timestamp);
}

EventLog EventLog::governanceProposed(
    const std::string& txId,
    const std::string& proposer,
    const std::string& proposalId,
    std::uint64_t blockHeight,
    std::int64_t timestamp
) {
    return EventLog(txId, blockHeight, EventType::GOVERNANCE_PROPOSED,
        proposer, proposalId, utils::Amount(), "", timestamp);
}

EventLog EventLog::governanceExecuted(
    const std::string& txId,
    const std::string& proposalId,
    std::uint64_t blockHeight,
    std::int64_t timestamp
) {
    return EventLog(txId, blockHeight, EventType::GOVERNANCE_EXECUTED,
        proposalId, "", utils::Amount(), "", timestamp);
}

EventLog EventLog::tokenBurned(
    const std::string& txId,
    const std::string& address,
    utils::Amount amount,
    std::uint64_t blockHeight,
    std::int64_t timestamp
) {
    return EventLog(txId, blockHeight, EventType::TOKEN_BURNED,
        address, "", amount, "", timestamp);
}

const std::string& EventLog::transactionId() const {
    return m_transactionId;
}

std::uint64_t EventLog::blockHeight() const {
    return m_blockHeight;
}

EventType EventLog::type() const {
    return m_type;
}

const std::string& EventLog::primaryAddress() const {
    return m_primaryAddress;
}

const std::string& EventLog::secondaryAddress() const {
    return m_secondaryAddress;
}

utils::Amount EventLog::amount() const {
    return m_amount;
}

const std::string& EventLog::data() const {
    return m_data;
}

std::int64_t EventLog::timestamp() const {
    return m_timestamp;
}

bool EventLog::isValid() const {
    if (m_transactionId.empty() || m_transactionId.size() > 128) {
        return false;
    }
    if (m_primaryAddress.empty() || m_primaryAddress.size() > 200) {
        return false;
    }
    if (m_secondaryAddress.size() > 200) {
        return false;
    }
    if (m_timestamp <= 0) {
        return false;
    }
    if (m_amount.isNegative()) {
        return false;
    }
    return true;
}

std::string EventLog::serialize() const {
    std::ostringstream oss;
    oss << "EventLog{"
        << "transactionId=" << m_transactionId
        << ";blockHeight=" << m_blockHeight
        << ";type=" << eventTypeToString(m_type)
        << ";primaryAddress=" << m_primaryAddress
        << ";secondaryAddress=" << m_secondaryAddress
        << ";amountRaw=" << m_amount.rawUnits()
        << ";data=" << m_data
        << ";timestamp=" << m_timestamp
        << "}";
    return oss.str();
}

std::string EventLog::eventHash() const {
    return hashString("NODO_EVENT_LOG_V1|" + serialize());
}

} // namespace nodo::core
