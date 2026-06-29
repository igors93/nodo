#ifndef NODO_CORE_TRANSACTION_EXECUTION_CONTEXT_HPP
#define NODO_CORE_TRANSACTION_EXECUTION_CONTEXT_HPP

#include "core/AccountStateView.hpp"
#include "core/CoinLotRegistry.hpp"
#include "core/LedgerRecord.hpp"
#include "core/Transaction.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nodo::core {

class TransactionDomainExecutionResult {
public:
    static TransactionDomainExecutionResult accepted(
        AccountStateView accounts,
        std::map<std::string, std::string> domains
    );
    static TransactionDomainExecutionResult rejected(std::string code, std::string reason);

    bool applied() const;
    const std::string& code() const;
    const std::string& reason() const;
    const AccountStateView& accounts() const;
    const std::map<std::string, std::string>& domains() const;

private:
    bool m_applied = false;
    std::string m_code;
    std::string m_reason;
    AccountStateView m_accounts;
    std::map<std::string, std::string> m_domains;
};

class TransactionDomainExecutor {
public:
    virtual ~TransactionDomainExecutor() = default;

    virtual TransactionDomainExecutionResult applyBurn(const Transaction&, const AccountStateView&, std::uint64_t, std::int64_t) = 0;
    virtual TransactionDomainExecutionResult applyStakeDeposit(const Transaction&, const AccountStateView&, std::uint64_t, std::int64_t) = 0;
    virtual TransactionDomainExecutionResult applyStakeWithdraw(const Transaction&, const AccountStateView&, std::uint64_t, std::int64_t) = 0;
    virtual TransactionDomainExecutionResult applyStakeTopUp(const Transaction&, const AccountStateView&, std::uint64_t, std::int64_t) = 0;
    virtual TransactionDomainExecutionResult applyValidatorRegister(const Transaction&, const AccountStateView&, std::uint64_t, std::int64_t) = 0;
    virtual TransactionDomainExecutionResult applyValidatorExitRequest(const Transaction&, const AccountStateView&, std::uint64_t, std::int64_t) = 0;
    virtual TransactionDomainExecutionResult applyValidatorUnjailRequest(const Transaction&, const AccountStateView&, std::uint64_t, std::int64_t) = 0;
    virtual TransactionDomainExecutionResult applyGovernanceProposal(const Transaction&, const AccountStateView&, std::uint64_t, std::int64_t) = 0;
    virtual TransactionDomainExecutionResult applyGovernanceVote(const Transaction&, const AccountStateView&, std::uint64_t, std::int64_t) = 0;

    virtual TransactionDomainExecutionResult finalizeBlock(
        const AccountStateView& accounts,
        utils::Amount totalFee,
        const std::vector<LedgerRecord>& protocolRecords,
        std::uint64_t blockHeight,
        std::int64_t blockTimestamp
    ) = 0;

    virtual const std::map<std::string, std::string>& domains() const = 0;
};

class TransactionExecutionContext {
public:
    TransactionExecutionContext(
        AccountStateView accounts,
        std::uint64_t blockHeight,
        std::int64_t blockTimestamp,
        bool enforceAccountState,
        bool allowMissingAccounts,
        bool requireDomainExecutor,
        std::string feeRecipientAddress,
        CoinLotRegistry* coinLots,
        TransactionDomainExecutor* domainExecutor
    );

    const AccountStateView& accounts() const;
    std::uint64_t blockHeight() const;
    std::int64_t blockTimestamp() const;
    bool enforceAccountState() const;
    bool allowMissingAccounts() const;
    bool requireDomainExecutor() const;
    const std::string& feeRecipientAddress() const;
    CoinLotRegistry* coinLots() const;
    TransactionDomainExecutor* domainExecutor() const;

private:
    AccountStateView m_accounts;
    std::uint64_t m_blockHeight;
    std::int64_t m_blockTimestamp;
    bool m_enforceAccountState;
    bool m_allowMissingAccounts;
    bool m_requireDomainExecutor;
    std::string m_feeRecipientAddress;
    CoinLotRegistry* m_coinLots;
    TransactionDomainExecutor* m_domainExecutor;
};

using TransactionDomainExecutorFactory = std::function<std::unique_ptr<TransactionDomainExecutor>()>;

} // namespace nodo::core

#endif
