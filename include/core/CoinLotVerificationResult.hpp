#ifndef NODO_CORE_COIN_LOT_VERIFICATION_RESULT_HPP
#define NODO_CORE_COIN_LOT_VERIFICATION_RESULT_HPP

#include <string>

namespace nodo::core {

/*
 * CoinLotVerificationResult describes whether a coin lot can be used.
 *
 * In simple terms:
 * Before a coin interacts with the blockchain, Nodo must be able to answer:
 *
 *   does this coin lot exist?
 *   is it available?
 *   does the owner match?
 *   does the amount match?
 */
class CoinLotVerificationResult {
public:
    static CoinLotVerificationResult valid();

    static CoinLotVerificationResult invalid(
        std::string reason
    );

    bool success() const;
    const std::string& reason() const;

    std::string serialize() const;

private:
    CoinLotVerificationResult(
        bool success,
        std::string reason
    );

    bool m_success;
    std::string m_reason;
};

} // namespace nodo::core

#endif
