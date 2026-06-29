#include "core/StateTransitionPreviewContext.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

DeterministicStateTransitionResult DeterministicStateTransitionResult::accepted(
    AccountStateView accounts,
    std::map<std::string, std::string> domains
) {
    DeterministicStateTransitionResult result;
    result.m_valid = accounts.isValid();
    result.m_reason = result.m_valid ? "" : "Protocol transition produced invalid accounts.";
    result.m_accounts = std::move(accounts);
    result.m_domains = std::move(domains);
    for (const auto& [domain, payload] : result.m_domains) {
        if (domain.empty() || payload.empty()) {
            result.m_valid = false;
            result.m_reason = "Protocol transition produced an invalid state domain.";
            break;
        }
    }
    return result;
}

DeterministicStateTransitionResult DeterministicStateTransitionResult::rejected(
    std::string reason
) {
    DeterministicStateTransitionResult result;
    result.m_reason = std::move(reason);
    return result;
}

bool DeterministicStateTransitionResult::valid() const { return m_valid; }
const std::string& DeterministicStateTransitionResult::reason() const { return m_reason; }
const AccountStateView& DeterministicStateTransitionResult::accounts() const { return m_accounts; }
const std::map<std::string, std::string>& DeterministicStateTransitionResult::domains() const {
    return m_domains;
}

StateTransitionPreviewContext::StateTransitionPreviewContext()
    : m_minimumFeeRawUnits(-1),
      m_accountStateView(),
      m_allowMissingAccounts(false),
      m_enforceAccountState(false),
      m_feeRecipientAddress(""),
      m_wallClockNow(0),
      m_expectedChainId(""),
      m_cryptoContext(std::nullopt),
      m_deterministicStateDomains(),
      m_stateDomainTransition(),
      m_domainExecutorFactory(),
      m_requireDomainExecutor(false),
      m_coinLotRegistry(std::nullopt) {}

StateTransitionPreviewContext::StateTransitionPreviewContext(
    std::int64_t minimumFeeRawUnits,
    AccountStateView accountStateView,
    bool allowMissingAccounts,
    bool enforceAccountState,
    std::string feeRecipientAddress,
    std::int64_t wallClockNow,
    std::string expectedChainId,
    std::string networkName,
    std::map<std::string, std::string> deterministicStateDomains,
    DeterministicStateDomainTransition stateDomainTransition,
    TransactionDomainExecutorFactory domainExecutorFactory,
    bool requireDomainExecutor
)
    : m_minimumFeeRawUnits(minimumFeeRawUnits),
      m_accountStateView(std::move(accountStateView)),
      m_allowMissingAccounts(allowMissingAccounts),
      m_enforceAccountState(enforceAccountState),
      m_feeRecipientAddress(std::move(feeRecipientAddress)),
      m_wallClockNow(wallClockNow),
      m_expectedChainId(std::move(expectedChainId)),
      m_cryptoContext(networkName.empty()
          ? std::nullopt
            : std::optional<crypto::ProtocolCryptoContext>(
                crypto::ProtocolCryptoContext::fromNetworkName(networkName)
            )),
      m_deterministicStateDomains(std::move(deterministicStateDomains)),
      m_stateDomainTransition(std::move(stateDomainTransition)),
      m_domainExecutorFactory(std::move(domainExecutorFactory)),
      m_requireDomainExecutor(requireDomainExecutor),
      m_coinLotRegistry(std::nullopt) {}

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

const std::string& StateTransitionPreviewContext::expectedChainId() const {
    return m_expectedChainId;
}

bool StateTransitionPreviewContext::protocolAuthorizationEnabled() const {
    return !m_expectedChainId.empty() &&
           m_cryptoContext.has_value() &&
           m_cryptoContext->isValid();
}

const crypto::ProtocolCryptoContext& StateTransitionPreviewContext::cryptoContext() const {
    if (!m_cryptoContext.has_value()) {
        throw std::logic_error("State transition context has no protocol crypto context.");
    }
    return m_cryptoContext.value();
}

const std::map<std::string, std::string>&
StateTransitionPreviewContext::deterministicStateDomains() const {
    return m_deterministicStateDomains;
}

DeterministicStateTransitionResult
StateTransitionPreviewContext::transitionProtocolState(
    const AccountStateView& accounts,
    utils::Amount totalFee,
    const std::vector<Transaction>& transactions,
    const std::vector<LedgerRecord>& protocolRecords,
    std::int64_t blockTimestamp
) const {
    if (!m_stateDomainTransition) {
        return DeterministicStateTransitionResult::accepted(
            accounts, m_deterministicStateDomains
        );
    }
    return m_stateDomainTransition(
        accounts,
        totalFee,
        transactions,
        protocolRecords,
        blockTimestamp
    );
}

std::unique_ptr<TransactionDomainExecutor>
StateTransitionPreviewContext::createDomainExecutor() const {
    return m_domainExecutorFactory ? m_domainExecutorFactory() : nullptr;
}

bool StateTransitionPreviewContext::requireDomainExecutor() const {
    return m_requireDomainExecutor;
}

bool StateTransitionPreviewContext::coinLotPreviewEnabled() const {
    return m_coinLotRegistry.has_value();
}

const CoinLotRegistry& StateTransitionPreviewContext::coinLotRegistry() const {
    if (!m_coinLotRegistry.has_value()) {
        throw std::logic_error("CoinLot preview is not enabled in this context.");
    }
    return m_coinLotRegistry.value();
}

void StateTransitionPreviewContext::enableCoinLotPreview(CoinLotRegistry registry) {
    m_coinLotRegistry = std::move(registry);
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

    if (m_expectedChainId.empty() != !m_cryptoContext.has_value()) {
        return false;
    }

    if (m_cryptoContext.has_value() && !m_cryptoContext->isValid()) {
        return false;
    }

    for (const auto& [domain, payload] : m_deterministicStateDomains) {
        if (domain.empty() || payload.empty()) {
            return false;
        }
    }

    if (m_requireDomainExecutor && !m_domainExecutorFactory) return false;

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
        << ";expectedChainId=" << m_expectedChainId
        << ";protocolAuthorizationEnabled="
        << (protocolAuthorizationEnabled() ? "true" : "false")
        << ";deterministicStateDomainCount=" << m_deterministicStateDomains.size()
        << ";coinLotPreviewEnabled=" << (m_coinLotRegistry.has_value() ? "true" : "false")
        << ";requireDomainExecutor=" << (m_requireDomainExecutor ? "true" : "false")
        << ";accountStateView=" << m_accountStateView.serialize()
        << "}";

    return oss.str();
}

} // namespace nodo::core
