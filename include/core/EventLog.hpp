#ifndef NODO_CORE_EVENT_LOG_HPP
#define NODO_CORE_EVENT_LOG_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

enum class EventType {
    TRANSFER,
    STAKE_DEPOSITED,
    STAKE_WITHDRAWN,
    VALIDATOR_REGISTERED,
    VALIDATOR_DEREGISTERED,
    VALIDATOR_PENALIZED,
    GOVERNANCE_PROPOSED,
    GOVERNANCE_VOTED,
    GOVERNANCE_EXECUTED,
    TOKEN_BURNED,
    TOKEN_MINTED,
    FEE_BURNED
};

std::string eventTypeToString(EventType type);

/*
 * EventLog records a single ledger event emitted during transaction execution.
 *
 * Security principle:
 * Events are read-only audit records. They do not modify state. Their hash
 * commitments may be included in block headers for light-client verification.
 */
class EventLog {
public:
    EventLog();

    EventLog(
        std::string transactionId,
        std::uint64_t blockHeight,
        EventType type,
        std::string primaryAddress,     // from/sender/subject
        std::string secondaryAddress,   // to/target (may be empty)
        utils::Amount amount,
        std::string data,               // extra serialized context
        std::int64_t timestamp
    );

    // Factory methods
    static EventLog transfer(
        const std::string& txId,
        const std::string& from,
        const std::string& to,
        utils::Amount amount,
        std::uint64_t blockHeight,
        std::int64_t timestamp
    );

    static EventLog stakeDeposited(
        const std::string& txId,
        const std::string& address,
        utils::Amount amount,
        std::uint64_t blockHeight,
        std::int64_t timestamp
    );

    static EventLog stakeWithdrawn(
        const std::string& txId,
        const std::string& address,
        utils::Amount amount,
        std::uint64_t blockHeight,
        std::int64_t timestamp
    );

    static EventLog validatorRegistered(
        const std::string& txId,
        const std::string& address,
        std::uint64_t blockHeight,
        std::int64_t timestamp
    );

    static EventLog validatorPenalized(
        const std::string& txId,
        const std::string& address,
        utils::Amount penaltyAmount,
        std::uint64_t blockHeight,
        std::int64_t timestamp
    );

    static EventLog governanceProposed(
        const std::string& txId,
        const std::string& proposer,
        const std::string& proposalId,
        std::uint64_t blockHeight,
        std::int64_t timestamp
    );

    static EventLog governanceExecuted(
        const std::string& txId,
        const std::string& proposalId,
        std::uint64_t blockHeight,
        std::int64_t timestamp
    );

    static EventLog tokenBurned(
        const std::string& txId,
        const std::string& address,
        utils::Amount amount,
        std::uint64_t blockHeight,
        std::int64_t timestamp
    );

    const std::string& transactionId() const;
    std::uint64_t blockHeight() const;
    EventType type() const;
    const std::string& primaryAddress() const;
    const std::string& secondaryAddress() const;
    utils::Amount amount() const;
    const std::string& data() const;
    std::int64_t timestamp() const;

    bool isValid() const;
    std::string serialize() const;
    std::string eventHash() const; // SHA-256 of serialize()

private:
    std::string m_transactionId;
    std::uint64_t m_blockHeight;
    EventType m_type;
    std::string m_primaryAddress;
    std::string m_secondaryAddress;
    utils::Amount m_amount;
    std::string m_data;
    std::int64_t m_timestamp;
};

} // namespace nodo::core

#endif
