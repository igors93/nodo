#ifndef NODO_CORE_COIN_LOT_TRANSFER_PLAN_HPP
#define NODO_CORE_COIN_LOT_TRANSFER_PLAN_HPP

#include "core/CoinLot.hpp"
#include "utils/Amount.hpp"

#include <string>
#include <vector>

namespace nodo::core {

/*
 * CoinLotTransferPlan is the deterministic bridge between a high-level
 * Transaction and low-level CoinLot movement.
 *
 * In simple terms:
 * - transaction says: Igor sends 20 to Ana and pays 1 fee;
 * - transfer plan says: which lots are consumed and which output lots are born.
 *
 * This keeps the security-critical lot movement explicit and testable.
 */
class CoinLotTransferPlan {
public:
    CoinLotTransferPlan(
        std::string transactionId,
        std::string senderAddress,
        std::string recipientAddress,
        std::vector<std::string> inputLotIds,
        std::vector<CoinLot> outputLots,
        utils::Amount transferAmount,
        utils::Amount feeAmount,
        utils::Amount changeAmount,
        utils::Amount totalInputAmount
    );

    const std::string& transactionId() const;
    const std::string& senderAddress() const;
    const std::string& recipientAddress() const;
    const std::vector<std::string>& inputLotIds() const;
    const std::vector<CoinLot>& outputLots() const;

    utils::Amount transferAmount() const;
    utils::Amount feeAmount() const;
    utils::Amount changeAmount() const;
    utils::Amount totalInputAmount() const;
    utils::Amount totalOutputAmount() const;
    utils::Amount totalDebitAmount() const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::string m_transactionId;
    std::string m_senderAddress;
    std::string m_recipientAddress;
    std::vector<std::string> m_inputLotIds;
    std::vector<CoinLot> m_outputLots;
    utils::Amount m_transferAmount;
    utils::Amount m_feeAmount;
    utils::Amount m_changeAmount;
    utils::Amount m_totalInputAmount;
};

} // namespace nodo::core

#endif
