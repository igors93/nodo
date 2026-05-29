#ifndef NODO_CORE_COIN_LOT_TRANSACTION_VALIDATOR_HPP
#define NODO_CORE_COIN_LOT_TRANSACTION_VALIDATOR_HPP

#include "core/CoinLotRegistry.hpp"
#include "core/CoinLotTransactionValidationResult.hpp"
#include "core/CoinLotTransferPlan.hpp"
#include "core/Transaction.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * CoinLotTransactionValidator connects high-level transactions to the
 * CoinLotRegistry.
 *
 * Security principle:
 * A transfer is not valid just because the account balance looks high enough.
 * The transaction must be backed by real, spendable coin lots.
 */
class CoinLotTransactionValidator {
public:
    static CoinLotTransactionValidationResult validateTransferTransaction(
        const Transaction& transaction,
        const CoinLotRegistry& registry
    );

    static CoinLotTransferPlan buildTransferPlan(
        const Transaction& transaction,
        const CoinLotRegistry& registry,
        std::uint64_t currentBlockIndex
    );

    static void applyTransfer(
        CoinLotRegistry& registry,
        const Transaction& transaction,
        std::uint64_t currentBlockIndex
    );

private:
    static std::vector<CoinLot> selectInputLots(
        const Transaction& transaction,
        const CoinLotRegistry& registry,
        utils::Amount totalDebit
    );

    static CoinLotTransactionValidationResult validateSelectedInputLots(
        const Transaction& transaction,
        const std::vector<CoinLot>& selectedInputLots,
        utils::Amount totalDebit
    );

    static std::string createTransferOutputCoinLotId(
        const Transaction& transaction,
        const CoinLot& inputLot,
        const std::string& outputKind,
        std::size_t outputIndex
    );

    static bool outputIdAlreadyExistsInRegistryOrPending(
        const CoinLotRegistry& registry,
        const std::vector<CoinLot>& pendingOutputs,
        const std::string& outputLotId
    );
};

} // namespace nodo::core

#endif
