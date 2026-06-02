#ifndef NODO_ECONOMICS_TREASURY_ACCOUNT_HPP
#define NODO_ECONOMICS_TREASURY_ACCOUNT_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

enum class TreasuryDebitStatus {
    ACCEPTED,
    LOCKED,
    INSUFFICIENT_BALANCE,
    NEGATIVE_AMOUNT
};

std::string treasuryDebitStatusToString(TreasuryDebitStatus status);

class TreasuryDebitResult {
public:
    TreasuryDebitResult();

    static TreasuryDebitResult accepted(utils::Amount balanceAfter);
    static TreasuryDebitResult locked();
    static TreasuryDebitResult insufficientBalance(
        utils::Amount available, utils::Amount requested
    );
    static TreasuryDebitResult negativeAmount();

    bool accepted() const;
    TreasuryDebitStatus status() const;
    const std::string& reason() const;
    utils::Amount balanceAfter() const;

private:
    TreasuryDebitStatus m_status;
    std::string m_reason;
    utils::Amount m_balanceAfter;
};

/*
 * TreasuryAccount holds the protocol treasury balance for a given epoch.
 *
 * Security principle:
 * The treasury is not a wallet. No code path may debit the treasury without
 * an explicit spend authorization chain (proposal -> approval -> validator).
 * This class enforces balance invariants; the authorization chain is enforced
 * by TreasurySpendValidator.
 */
class TreasuryAccount {
public:
    TreasuryAccount();

    TreasuryAccount(
        std::string treasuryId,
        std::string accountAddress,
        utils::Amount balance,
        std::uint64_t epoch,
        bool locked,
        std::string reason
    );

    static TreasuryAccount invalid(std::string reason);

    const std::string& treasuryId() const;
    const std::string& accountAddress() const;
    utils::Amount balance() const;
    std::uint64_t epoch() const;
    bool isLocked() const;
    const std::string& lockReason() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    // Returns a new account with balance increased by amount.
    // Rejects if amount is negative or the result would overflow int64.
    TreasuryAccount credit(utils::Amount amount) const;

    // Returns a debit result. Does not mutate this account.
    // Caller applies the returned balanceAfter to create a new TreasuryAccount.
    TreasuryDebitResult tryDebit(utils::Amount amount) const;

    std::string serialize() const;

private:
    std::string m_treasuryId;
    std::string m_accountAddress;
    utils::Amount m_balance;
    std::uint64_t m_epoch;
    bool m_locked;
    std::string m_lockReason;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
