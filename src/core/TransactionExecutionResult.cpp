#include "core/TransactionExecutionResult.hpp"

#include <utility>
#include <sstream>

namespace nodo::core {

std::string transactionExecutionStatusToString(TransactionExecutionStatus status) {
    switch (status) {
        case TransactionExecutionStatus::APPLIED: return "APPLIED";
        case TransactionExecutionStatus::INVALID_TRANSACTION: return "INVALID_TRANSACTION";
        case TransactionExecutionStatus::INVALID_NONCE: return "INVALID_NONCE";
        case TransactionExecutionStatus::INSUFFICIENT_BALANCE: return "INSUFFICIENT_BALANCE";
        case TransactionExecutionStatus::DOMAIN_REJECTED: return "DOMAIN_REJECTED";
        case TransactionExecutionStatus::STATE_ERROR: return "STATE_ERROR";
    }
    return "STATE_ERROR";
}

TransactionExecutionResult TransactionExecutionResult::applied(
    AccountStateView accounts,
    TransactionReceipt receipt,
    std::vector<std::string> touchedAccounts,
    std::vector<std::string> touchedDomains
) {
    TransactionExecutionResult result;
    result.m_status = TransactionExecutionStatus::APPLIED;
    result.m_accounts = std::move(accounts);
    result.m_receipt = std::move(receipt);
    result.m_touchedAccounts = std::move(touchedAccounts);
    result.m_touchedDomains = std::move(touchedDomains);
    return result;
}

TransactionExecutionResult TransactionExecutionResult::rejected(
    TransactionExecutionStatus status,
    std::string code,
    std::string reason
) {
    TransactionExecutionResult result;
    result.m_status = status;
    result.m_code = std::move(code);
    result.m_reason = std::move(reason);
    return result;
}

bool TransactionExecutionResult::success() const { return m_status == TransactionExecutionStatus::APPLIED; }
TransactionExecutionStatus TransactionExecutionResult::status() const { return m_status; }
const std::string& TransactionExecutionResult::code() const { return m_code; }
const std::string& TransactionExecutionResult::reason() const { return m_reason; }
const AccountStateView& TransactionExecutionResult::accounts() const { return m_accounts; }
const TransactionReceipt& TransactionExecutionResult::receipt() const { return m_receipt; }
const std::vector<std::string>& TransactionExecutionResult::touchedAccounts() const { return m_touchedAccounts; }
const std::vector<std::string>& TransactionExecutionResult::touchedDomains() const { return m_touchedDomains; }

std::string TransactionExecutionResult::serialize() const {
    std::ostringstream out;
    out << "TransactionExecutionResult{status="
        << transactionExecutionStatusToString(m_status)
        << ";code=" << m_code
        << ";reason=" << m_reason
        << ";touchedAccountCount=" << m_touchedAccounts.size()
        << ";touchedDomainCount=" << m_touchedDomains.size()
        << ";receipt=" << (m_receipt.isValid() ? m_receipt.serialize() : "NONE")
        << "}";
    return out.str();
}

} // namespace nodo::core
