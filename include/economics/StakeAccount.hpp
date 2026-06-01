#ifndef NODO_ECONOMICS_STAKE_ACCOUNT_HPP
#define NODO_ECONOMICS_STAKE_ACCOUNT_HPP

#include "utils/Amount.hpp"

#include <string>

namespace nodo::economics {

/*
 * StakeAccount tracks the bonded and slashed amounts for one validator.
 *
 * Security principle:
 * Bonded amount must never go negative. Slashed amount must never exceed
 * bonded amount. A tombstoned or jailed validator is not eligible to
 * participate in consensus regardless of their stake balance.
 */
class StakeAccount {
public:
    StakeAccount();

    StakeAccount(
        std::string validatorAddress,
        utils::Amount bondedAmount
    );

    const std::string& validatorAddress() const;
    utils::Amount bondedAmount() const;
    utils::Amount slashedAmount() const;
    bool jailed() const;
    bool tombstoned() const;

    bool isEligible() const;
    bool canSlash(utils::Amount amount) const;

    void applySlash(utils::Amount amount);
    void jail();
    void tombstone();
    void unjail();

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    utils::Amount m_bondedAmount;
    utils::Amount m_slashedAmount;
    bool m_jailed;
    bool m_tombstoned;
};

} // namespace nodo::economics

#endif
