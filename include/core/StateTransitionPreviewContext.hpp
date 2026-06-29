#ifndef NODO_CORE_STATE_TRANSITION_PREVIEW_CONTEXT_HPP
#define NODO_CORE_STATE_TRANSITION_PREVIEW_CONTEXT_HPP

#include "core/AccountStateView.hpp"
#include "core/LedgerRecord.hpp"
#include "core/Transaction.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <optional>
#include <map>
#include <functional>
#include <string>

namespace nodo::core {

class DeterministicStateTransitionResult {
public:
    static DeterministicStateTransitionResult accepted(
        AccountStateView accounts,
        std::map<std::string, std::string> domains
    );

    static DeterministicStateTransitionResult rejected(std::string reason);

    bool valid() const;
    const std::string& reason() const;
    const AccountStateView& accounts() const;
    const std::map<std::string, std::string>& domains() const;

private:
    bool m_valid = false;
    std::string m_reason;
    AccountStateView m_accounts;
    std::map<std::string, std::string> m_domains;
};

using DeterministicStateDomainTransition = std::function<
    DeterministicStateTransitionResult(
        const AccountStateView&,
        utils::Amount,
        const std::vector<Transaction>&,
        const std::vector<LedgerRecord>&,
        std::int64_t
    )
>;

// Called once per staking/lifecycle transaction inside previewBlock.
// Returns false if the transaction violates domain constraints (e.g. insufficient
// bonded stake, cooldown not elapsed, jailed/tombstoned validator).
// Implementations may maintain internal state so that consecutive transactions
// within the same block see the intermediate staking changes.
using DomainTransactionPreValidator = std::function<bool(const Transaction&)>;

class StateTransitionPreviewContext {
public:
    StateTransitionPreviewContext();

    StateTransitionPreviewContext(
        std::int64_t minimumFeeRawUnits,
        AccountStateView accountStateView,
        bool allowMissingAccounts,
        bool enforceAccountState,
        std::string feeRecipientAddress = "",
        std::int64_t wallClockNow = 0,
        std::string expectedChainId = "",
        std::string networkName = "",
        std::map<std::string, std::string> deterministicStateDomains = {},
        DeterministicStateDomainTransition stateDomainTransition = {}
    );

    static StateTransitionPreviewContext structuralOnly(
        std::int64_t minimumFeeRawUnits
    );

    std::int64_t minimumFeeRawUnits() const;
    const AccountStateView& accountStateView() const;
    bool allowMissingAccounts() const;
    bool enforceAccountState() const;
    const std::string& feeRecipientAddress() const;
    std::int64_t wallClockNow() const;
    const std::string& expectedChainId() const;
    bool protocolAuthorizationEnabled() const;
    const crypto::ProtocolCryptoContext& cryptoContext() const;
    const std::map<std::string, std::string>& deterministicStateDomains() const;
    DeterministicStateTransitionResult transitionProtocolState(
        const AccountStateView& accounts,
        utils::Amount totalFee,
        const std::vector<Transaction>& transactions,
        const std::vector<LedgerRecord>& protocolRecords,
        std::int64_t blockTimestamp
    ) const;

    bool coinLotPreviewEnabled() const;
    bool supplyAuditPreviewEnabled() const;

    void setDomainTransactionPreValidator(DomainTransactionPreValidator validator);
    bool hasDomainTransactionPreValidator() const;
    bool validateDomainTransaction(const Transaction& tx) const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::int64_t m_minimumFeeRawUnits;
    AccountStateView m_accountStateView;
    bool m_allowMissingAccounts;
    bool m_enforceAccountState;
    std::string m_feeRecipientAddress;
    std::int64_t m_wallClockNow;
    std::string m_expectedChainId;
    std::optional<crypto::ProtocolCryptoContext> m_cryptoContext;
    std::map<std::string, std::string> m_deterministicStateDomains;
    DeterministicStateDomainTransition m_stateDomainTransition;
    DomainTransactionPreValidator m_domainTransactionPreValidator;
    bool m_coinLotPreviewEnabled;
    bool m_supplyAuditPreviewEnabled;
};

} // namespace nodo::core

#endif
