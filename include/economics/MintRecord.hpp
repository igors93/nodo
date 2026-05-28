#ifndef NODO_ECONOMICS_MINT_RECORD_HPP
#define NODO_ECONOMICS_MINT_RECORD_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * MintReason explains why new coins were created.
 *
 * Nodo rule:
 * No coin can be created without an explicit reason.
 */
enum class MintReason {
    GENESIS_ALLOCATION,
    NETWORK_DEFENSE_REWARD,
    LOCKED_RESERVE_REWARD,
    TREASURY_REWARD
};

std::string mintReasonToString(MintReason reason);
MintReason mintReasonFromString(const std::string& value);

/*
 * MintRecord records the origin of newly created coins.
 *
 * Core principle:
 * Every NODO coin must be born from a MintRecord.
 */
class MintRecord {
public:
    MintRecord(
        std::string id,
        std::string recipientAddress,
        utils::Amount amount,
        MintReason reason,
        std::uint64_t epoch,
        std::uint64_t sourceBlockIndex,
        std::string sourceBlockHash,
        std::int64_t timestamp
    );

    const std::string& id() const;
    const std::string& recipientAddress() const;
    utils::Amount amount() const;
    MintReason reason() const;
    std::uint64_t epoch() const;
    std::uint64_t sourceBlockIndex() const;
    const std::string& sourceBlockHash() const;
    std::int64_t timestamp() const;

    bool isValid() const;

    /*
     * Deterministic serialization.
     *
     * Security rule:
     * Data used for hashing must always be serialized in the same order
     * and in the same format.
     */
    std::string serialize() const;

    /*
     * Rebuilds a MintRecord from its deterministic serialized form.
     *
     * This method delegates parsing to serialization::MintRecordCodec so that
     * MintRecord parsing remains centralized in the serialization module.
     */
    static MintRecord deserialize(const std::string& serialized);

private:
    std::string m_id;
    std::string m_recipientAddress;
    utils::Amount m_amount;
    MintReason m_reason;
    std::uint64_t m_epoch;
    std::uint64_t m_sourceBlockIndex;
    std::string m_sourceBlockHash;
    std::int64_t m_timestamp;
};

} // namespace nodo::economics

#endif