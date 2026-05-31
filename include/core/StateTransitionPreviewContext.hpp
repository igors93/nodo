#ifndef NODO_CORE_STATE_TRANSITION_PREVIEW_CONTEXT_HPP
#define NODO_CORE_STATE_TRANSITION_PREVIEW_CONTEXT_HPP

#include "core/AccountStateView.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

class StateTransitionPreviewContext {
public:
    StateTransitionPreviewContext();

    StateTransitionPreviewContext(
        std::int64_t minimumFeeRawUnits,
        AccountStateView accountStateView,
        bool allowMissingAccounts,
        bool enforceAccountState,
        std::string feeRecipientAddress = ""
    );

    static StateTransitionPreviewContext structuralOnly(
        std::int64_t minimumFeeRawUnits
    );

    std::int64_t minimumFeeRawUnits() const;
    const AccountStateView& accountStateView() const;
    bool allowMissingAccounts() const;
    bool enforceAccountState() const;
    const std::string& feeRecipientAddress() const;

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
    bool m_coinLotPreviewEnabled;
    bool m_supplyAuditPreviewEnabled;
};

} // namespace nodo::core

#endif
