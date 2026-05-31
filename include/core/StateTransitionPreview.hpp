#ifndef NODO_CORE_STATE_TRANSITION_PREVIEW_HPP
#define NODO_CORE_STATE_TRANSITION_PREVIEW_HPP

#include "core/AccountState.hpp"
#include "core/Block.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

enum class StateTransitionPreviewStatus {
    VALID,
    INVALID_BLOCK,
    INVALID_CONTEXT,
    INVALID_LEDGER_RECORD,
    INVALID_TRANSACTION,
    DUPLICATE_TRANSACTION,
    INSUFFICIENT_BALANCE,
    INVALID_NONCE
};

std::string stateTransitionPreviewStatusToString(
    StateTransitionPreviewStatus status
);

class StateTransitionPreviewResult {
public:
    StateTransitionPreviewResult();

    static StateTransitionPreviewResult valid(
        std::size_t processedTransactionCount,
        utils::Amount totalFee,
        std::vector<std::string> touchedAccounts,
        std::vector<std::string> transactionIds,
        std::vector<AccountState> resultingAccounts,
        std::string stateRoot
    );

    static StateTransitionPreviewResult rejected(
        StateTransitionPreviewStatus status,
        std::string reason,
        std::size_t processedTransactionCount
    );

    StateTransitionPreviewStatus status() const;
    const std::string& reason() const;
    bool accepted() const;
    std::size_t processedTransactionCount() const;
    utils::Amount totalFee() const;
    const std::vector<std::string>& touchedAccounts() const;
    const std::vector<std::string>& transactionIds() const;
    const std::vector<AccountState>& resultingAccounts() const;
    const std::string& stateRoot() const;

    std::string serialize() const;

private:
    StateTransitionPreviewStatus m_status;
    std::string m_reason;
    std::size_t m_processedTransactionCount;
    utils::Amount m_totalFee;
    std::vector<std::string> m_touchedAccounts;
    std::vector<std::string> m_transactionIds;
    std::vector<AccountState> m_resultingAccounts;
    std::string m_stateRoot;
};

class StateTransitionPreview {
public:
    static StateTransitionPreviewResult previewBlock(
        const Block& block,
        std::int64_t minimumFeeRawUnits
    );

    static StateTransitionPreviewResult previewBlock(
        const Block& block,
        const StateTransitionPreviewContext& context
    );
};

} // namespace nodo::core

#endif
