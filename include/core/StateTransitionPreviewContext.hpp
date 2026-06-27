#ifndef NODO_CORE_STATE_TRANSITION_PREVIEW_CONTEXT_HPP
#define NODO_CORE_STATE_TRANSITION_PREVIEW_CONTEXT_HPP

#include "core/AccountStateView.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <optional>
#include <map>
#include <functional>
#include <string>

namespace nodo::core {

using DeterministicStateDomainTransition = std::function<
    std::map<std::string, std::string>(utils::Amount)
>;

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
    std::map<std::string, std::string> transitionedStateDomains(
        utils::Amount totalFee
    ) const;

    bool coinLotPreviewEnabled() const;
    bool supplyAuditPreviewEnabled() const;

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
    bool m_coinLotPreviewEnabled;
    bool m_supplyAuditPreviewEnabled;
};

} // namespace nodo::core

#endif
