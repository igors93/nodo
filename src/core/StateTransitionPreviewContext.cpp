#include "core/StateTransitionPreviewContext.hpp"

#include <sstream>
#include <utility>

namespace nodo::core {

StateTransitionPreviewContext::StateTransitionPreviewContext()
    : m_minimumFeeRawUnits(-1),
      m_accountStateView(),
      m_allowMissingAccounts(false),
      m_enforceAccountState(false),
      m_feeRecipientAddress(""),
      m_wallClockNow(0),
      m_coinLotPreviewEnabled(false),
      m_supplyAuditPreviewEnabled(false) {}

StateTransitionPreviewContext::StateTransitionPreviewContext(
    std::int64_t minimumFeeRawUnits,
    AccountStateView accountStateView,
    bool allowMissingAccounts,
    bool enforceAccountState,
    std::string feeRecipientAddress,
    std::int64_t wallClockNow
)
    : m_minimumFeeRawUnits(minimumFeeRawUnits),
      m_accountStateView(std::move(accountStateView)),
      m_allowMissingAccounts(allowMissingAccounts),
      m_enforceAccountState(enforceAccountState),
      m_feeRecipientAddress(std::move(feeRecipientAddress)),
      m_wallClockNow(wallClockNow),
      m_coinLotPreviewEnabled(false),
      m_supplyAuditPreviewEnabled(false) {}

StateTransitionPreviewContext StateTransitionPreviewContext::structuralOnly(
    std::int64_t minimumFeeRawUnits
) {
    return StateTransitionPreviewContext(
        minimumFeeRawUnits,
        AccountStateView(),
        true,
        false
    );
}

std::int64_t StateTransitionPreviewContext::minimumFeeRawUnits() const {
    return m_minimumFeeRawUnits;
}

const AccountStateView& StateTransitionPreviewContext::accountStateView() const {
    return m_accountStateView;
}

bool StateTransitionPreviewContext::allowMissingAccounts() const {
    return m_allowMissingAccounts;
}

bool StateTransitionPreviewContext::enforceAccountState() const {
    return m_enforceAccountState;
}

const std::string& StateTransitionPreviewContext::feeRecipientAddress() const {
    return m_feeRecipientAddress;
}

std::int64_t StateTransitionPreviewContext::wallClockNow() const {
    return m_wallClockNow;
}

bool StateTransitionPreviewContext::coinLotPreviewEnabled() const {
    return m_coinLotPreviewEnabled;
}

bool StateTransitionPreviewContext::supplyAuditPreviewEnabled() const {
    return m_supplyAuditPreviewEnabled;
}

bool StateTransitionPreviewContext::isValid() const {
    if (m_minimumFeeRawUnits < 0 ||
        !m_accountStateView.isValid()) {
        return false;
    }

    if (!m_feeRecipientAddress.empty() &&
        !m_accountStateView.hasAccount(m_feeRecipientAddress)) {
        return false;
    }

    return true;
}

std::string StateTransitionPreviewContext::serialize() const {
    std::ostringstream oss;

    oss << "StateTransitionPreviewContext{"
        << "minimumFeeRawUnits=" << m_minimumFeeRawUnits
        << ";allowMissingAccounts=" << (m_allowMissingAccounts ? "true" : "false")
        << ";enforceAccountState=" << (m_enforceAccountState ? "true" : "false")
        << ";feeRecipientAddress=" << m_feeRecipientAddress
        << ";wallClockNow=" << m_wallClockNow
        << ";coinLotPreviewEnabled=" << (m_coinLotPreviewEnabled ? "true" : "false")
        << ";supplyAuditPreviewEnabled=" << (m_supplyAuditPreviewEnabled ? "true" : "false")
        << ";accountStateView=" << m_accountStateView.serialize()
        << "}";

    return oss.str();
}

} // namespace nodo::core
