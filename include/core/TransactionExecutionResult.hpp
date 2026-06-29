#ifndef NODO_CORE_TRANSACTION_EXECUTION_RESULT_HPP
#define NODO_CORE_TRANSACTION_EXECUTION_RESULT_HPP

#include "core/AccountStateView.hpp"
#include "core/TransactionReceipt.hpp"

#include <string>
#include <vector>

namespace nodo::core {

enum class TransactionExecutionStatus {
    APPLIED,
    INVALID_TRANSACTION,
    INVALID_NONCE,
    INSUFFICIENT_BALANCE,
    DOMAIN_REJECTED,
    STATE_ERROR
};

std::string transactionExecutionStatusToString(TransactionExecutionStatus status);

class TransactionExecutionResult {
public:
    static TransactionExecutionResult applied(
        AccountStateView accounts,
        TransactionReceipt receipt,
        std::vector<std::string> touchedAccounts,
        std::vector<std::string> touchedDomains
    );
    static TransactionExecutionResult rejected(
        TransactionExecutionStatus status,
        std::string code,
        std::string reason
    );

    bool success() const;
    TransactionExecutionStatus status() const;
    const std::string& code() const;
    const std::string& reason() const;
    const AccountStateView& accounts() const;
    const TransactionReceipt& receipt() const;
    const std::vector<std::string>& touchedAccounts() const;
    const std::vector<std::string>& touchedDomains() const;
    std::string serialize() const;

private:
    TransactionExecutionStatus m_status = TransactionExecutionStatus::INVALID_TRANSACTION;
    std::string m_code;
    std::string m_reason;
    AccountStateView m_accounts;
    TransactionReceipt m_receipt;
    std::vector<std::string> m_touchedAccounts;
    std::vector<std::string> m_touchedDomains;
};

} // namespace nodo::core

#endif
