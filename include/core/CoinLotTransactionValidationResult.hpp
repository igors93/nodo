#ifndef NODO_CORE_COIN_LOT_TRANSACTION_VALIDATION_RESULT_HPP
#define NODO_CORE_COIN_LOT_TRANSACTION_VALIDATION_RESULT_HPP

#include <string>

namespace nodo::core {

/*
 * CoinLotTransactionValidationResult is a small explicit result object used by
 * transaction-to-coin-lot validation.
 *
 * We avoid returning only bool because maintenance is much easier when a failed
 * validation carries a deterministic reason.
 */
class CoinLotTransactionValidationResult {
public:
    static CoinLotTransactionValidationResult valid();

    static CoinLotTransactionValidationResult invalid(
        std::string reason
    );

    bool success() const;
    const std::string& reason() const;

    std::string serialize() const;

private:
    CoinLotTransactionValidationResult(
        bool success,
        std::string reason
    );

    bool m_success;
    std::string m_reason;
};

} // namespace nodo::core

#endif
