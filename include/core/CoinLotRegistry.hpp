#ifndef NODO_CORE_COIN_LOT_REGISTRY_HPP
#define NODO_CORE_COIN_LOT_REGISTRY_HPP

#include "core/CoinLot.hpp"
#include "core/CoinLotVerificationResult.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * CoinLotRegistry is the official in-memory view of coin lot existence.
 *
 * It answers the most important question before a coin interacts with the
 * blockchain:
 *
 *   does this lot exist and can it be used?
 *
 * Security principle:
 * This registry must be rebuildable from accepted blockchain history. It is a
 * view, not an independent source of truth.
 */
class CoinLotRegistry {
public:
    CoinLotRegistry();

    static CoinLotRegistry fromCoinLots(
        const std::vector<CoinLot>& lots
    );

    void addLot(
        const CoinLot& lot
    );

    bool hasLot(
        const std::string& lotId
    ) const;

    const CoinLot& lot(
        const std::string& lotId
    ) const;

    CoinLotVerificationResult verifyExists(
        const std::string& lotId
    ) const;

    CoinLotVerificationResult verifySpendable(
        const std::string& lotId,
        const std::string& expectedOwner
    ) const;

    CoinLotVerificationResult verifyExactSpend(
        const std::string& lotId,
        const std::string& expectedOwner,
        utils::Amount expectedAmount
    ) const;

    /*
     * Consumes an existing input lot and creates output lots.
     *
     * This is the basis for traceable lot movement:
     *
     *   input lot -> recipient lot + change lot + optional fee lot
     *
     * Rule:
     *   sum(outputs) must equal input amount.
     */
    void consumeLotAndCreateOutputs(
        const std::string& inputLotId,
        const std::string& expectedOwner,
        const std::vector<CoinLot>& outputLots
    );

    void markSpent(
        const std::string& lotId,
        const std::string& expectedOwner
    );

    void lockForSecurity(
        const std::string& lotId,
        const std::string& expectedOwner,
        std::uint64_t lockedUntilBlock
    );

    void unlockIfMature(
        const std::string& lotId,
        std::uint64_t currentBlock
    );

    void markSlashed(
        const std::string& lotId
    );

    utils::Amount availableBalanceForOwner(
        const std::string& ownerAddress
    ) const;

    utils::Amount lockedBalanceForOwner(
        const std::string& ownerAddress
    ) const;

    utils::Amount totalTrackedAmount() const;
    utils::Amount totalAvailableAmount() const;
    utils::Amount totalLockedAmount() const;
    utils::Amount totalSpentAmount() const;
    utils::Amount totalSlashedAmount() const;

    std::size_t size() const;

    const std::map<std::string, CoinLot>& lots() const;

    bool isValid() const;

    std::string serialize() const;

private:
    void assertLotCanBeAdded(
        const CoinLot& lot
    ) const;

    static utils::Amount sumLots(
        const std::vector<CoinLot>& lots
    );

    std::map<std::string, CoinLot> m_lots;
};

} // namespace nodo::core

#endif
