#ifndef NODO_ECONOMICS_GENESIS_REWARD_RECORD_HPP
#define NODO_ECONOMICS_GENESIS_REWARD_RECORD_HPP

#include "core/CoinLot.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * GenesisRewardRecord creates new coins as a controlled reward for protection
 * work accepted by the blockchain.
 *
 * It replaces the long-term idea of arbitrary premine with:
 *
 * useful work -> epoch reward -> auditable coin lot birth
 */
enum class GenesisRewardReason {
    UNKNOWN,
    NETWORK_PROTECTION,
    BOOTSTRAP_PROTECTION,
    STORAGE_PROTECTION,
    CHALLENGE_RESPONSE
};

std::string genesisRewardReasonToString(
    GenesisRewardReason reason
);

GenesisRewardReason genesisRewardReasonFromString(
    const std::string& value
);

class GenesisRewardRecord {
public:
    GenesisRewardRecord();

    GenesisRewardRecord(
        std::uint64_t epoch,
        std::string validatorAddress,
        utils::Amount amount,
        GenesisRewardReason reason,
        std::string workSummaryHash,
        std::string policyVersion,
        std::string acceptedBlockHash,
        std::int64_t timestamp
    );

    std::uint64_t epoch() const;
    const std::string& validatorAddress() const;
    utils::Amount amount() const;
    GenesisRewardReason reason() const;
    const std::string& workSummaryHash() const;
    const std::string& policyVersion() const;
    const std::string& acceptedBlockHash() const;
    std::int64_t timestamp() const;

    bool isValid() const;

    std::string deterministicId() const;

    core::CoinLot createRewardCoinLot(
        std::uint64_t createdAtBlock
    ) const;

    std::string serialize() const;

    static GenesisRewardRecord deserialize(
        const std::string& serialized
    );

private:
    std::uint64_t m_epoch;
    std::string m_validatorAddress;
    utils::Amount m_amount;
    GenesisRewardReason m_reason;
    std::string m_workSummaryHash;
    std::string m_policyVersion;
    std::string m_acceptedBlockHash;
    std::int64_t m_timestamp;
};

} // namespace nodo::economics

#endif
