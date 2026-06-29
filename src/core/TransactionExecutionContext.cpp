#include "core/TransactionExecutionContext.hpp"

#include <utility>

namespace nodo::core {

TransactionDomainExecutionResult TransactionDomainExecutionResult::accepted(
    AccountStateView accounts,
    std::map<std::string, std::string> domains
) {
    TransactionDomainExecutionResult result;
    result.m_applied = accounts.isValid();
    result.m_accounts = std::move(accounts);
    result.m_domains = std::move(domains);
    if (!result.m_applied) result.m_reason = "Domain handler produced invalid account state.";
    for (const auto& [name, payload] : result.m_domains) {
        if (name.empty() || payload.empty()) {
            result.m_applied = false;
            result.m_reason = "Domain handler produced invalid commitment data.";
        }
    }
    return result;
}

TransactionDomainExecutionResult TransactionDomainExecutionResult::rejected(
    std::string code,
    std::string reason
) {
    TransactionDomainExecutionResult result;
    result.m_code = std::move(code);
    result.m_reason = std::move(reason);
    return result;
}

bool TransactionDomainExecutionResult::applied() const { return m_applied; }
const std::string& TransactionDomainExecutionResult::code() const { return m_code; }
const std::string& TransactionDomainExecutionResult::reason() const { return m_reason; }
const AccountStateView& TransactionDomainExecutionResult::accounts() const { return m_accounts; }
const std::map<std::string, std::string>& TransactionDomainExecutionResult::domains() const { return m_domains; }

TransactionExecutionContext::TransactionExecutionContext(
    AccountStateView accounts,
    std::uint64_t blockHeight,
    std::int64_t blockTimestamp,
    bool enforceAccountState,
    bool allowMissingAccounts,
    bool requireDomainExecutor,
    std::string feeRecipientAddress,
    CoinLotRegistry* coinLots,
    TransactionDomainExecutor* domainExecutor
) : m_accounts(std::move(accounts)), m_blockHeight(blockHeight),
    m_blockTimestamp(blockTimestamp), m_enforceAccountState(enforceAccountState),
    m_allowMissingAccounts(allowMissingAccounts),
    m_requireDomainExecutor(requireDomainExecutor),
    m_feeRecipientAddress(std::move(feeRecipientAddress)),
    m_coinLots(coinLots), m_domainExecutor(domainExecutor) {}

const AccountStateView& TransactionExecutionContext::accounts() const { return m_accounts; }
std::uint64_t TransactionExecutionContext::blockHeight() const { return m_blockHeight; }
std::int64_t TransactionExecutionContext::blockTimestamp() const { return m_blockTimestamp; }
bool TransactionExecutionContext::enforceAccountState() const { return m_enforceAccountState; }
bool TransactionExecutionContext::allowMissingAccounts() const { return m_allowMissingAccounts; }
bool TransactionExecutionContext::requireDomainExecutor() const { return m_requireDomainExecutor; }
const std::string& TransactionExecutionContext::feeRecipientAddress() const { return m_feeRecipientAddress; }
CoinLotRegistry* TransactionExecutionContext::coinLots() const { return m_coinLots; }
TransactionDomainExecutor* TransactionExecutionContext::domainExecutor() const { return m_domainExecutor; }

} // namespace nodo::core
