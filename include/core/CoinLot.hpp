#ifndef NODO_CORE_COIN_LOT_HPP
#define NODO_CORE_COIN_LOT_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

/*
 * CoinLotStatus describes the lifecycle of a traceable coin lot.
 *
 * AVAILABLE:
 * The lot can be used as transaction input.
 *
 * LOCKED_FOR_SECURITY:
 * The lot is locked and contributes to economic security.
 *
 * SPENT:
 * The lot was consumed by a transaction and must never be spent again.
 *
 * SLASHED:
 * The lot was penalized for invalid behavior.
 */
enum class CoinLotStatus {
    AVAILABLE,
    LOCKED_FOR_SECURITY,
    SPENT,
    SLASHED
};

std::string coinLotStatusToString(CoinLotStatus status);

/*
 * CoinLot represents a traceable group of NODO coins.
 *
 * Core principle:
 * NODO coins are not only numbers in a balance.
 * They have origin, owner, status and history.
 */
class CoinLot {
public:
    CoinLot(
        std::string id,
        std::string originMintRecordId,
        std::string ownerAddress,
        utils::Amount amount,
        CoinLotStatus status,
        std::uint64_t createdAtBlock,
        std::uint64_t lockedUntilBlock,
        std::int64_t timestamp
    );

    const std::string& id() const;
    const std::string& originMintRecordId() const;
    const std::string& ownerAddress() const;
    utils::Amount amount() const;
    CoinLotStatus status() const;
    std::uint64_t createdAtBlock() const;
    std::uint64_t lockedUntilBlock() const;
    std::int64_t timestamp() const;

    bool isAvailable() const;
    bool isLockedForSecurity() const;
    bool isSpent() const;
    bool isSlashed() const;

    /*
     * Only spendable lots can be consumed by transfers.
     *
     * Security rule:
     * Locked, spent and slashed lots must never be used as transaction inputs.
     */
    bool isSpendable() const;

    bool isValid() const;

    void lockForSecurity(std::uint64_t lockedUntilBlock);
    void unlockIfMature(std::uint64_t currentBlock);
    void markSpent();
    void markSlashed();

    std::string serialize() const;

private:
    std::string m_id;
    std::string m_originMintRecordId;
    std::string m_ownerAddress;
    utils::Amount m_amount;
    CoinLotStatus m_status;
    std::uint64_t m_createdAtBlock;
    std::uint64_t m_lockedUntilBlock;
    std::int64_t m_timestamp;
};

} // namespace nodo::core

#endif