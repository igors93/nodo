#include "core/StateTransitionPreview.hpp"

#include "core/Transaction.hpp"

#include <exception>
#include <set>
#include <sstream>
#include <utility>

namespace nodo::core {

std::string stateTransitionPreviewStatusToString(
    StateTransitionPreviewStatus status
) {
    switch (status) {
        case StateTransitionPreviewStatus::VALID:
            return "VALID";
        case StateTransitionPreviewStatus::INVALID_BLOCK:
            return "INVALID_BLOCK";
        case StateTransitionPreviewStatus::INVALID_LEDGER_RECORD:
            return "INVALID_LEDGER_RECORD";
        case StateTransitionPreviewStatus::INVALID_TRANSACTION:
            return "INVALID_TRANSACTION";
        case StateTransitionPreviewStatus::DUPLICATE_TRANSACTION:
            return "DUPLICATE_TRANSACTION";
        default:
            return "INVALID_TRANSACTION";
    }
}

StateTransitionPreviewResult::StateTransitionPreviewResult()
    : m_status(StateTransitionPreviewStatus::INVALID_BLOCK),
      m_reason("Uninitialized state transition preview result."),
      m_processedTransactionCount(0),
      m_totalFee(),
      m_touchedAccounts(),
      m_transactionIds() {}

StateTransitionPreviewResult StateTransitionPreviewResult::valid(
    std::size_t processedTransactionCount,
    utils::Amount totalFee,
    std::vector<std::string> touchedAccounts,
    std::vector<std::string> transactionIds
) {
    StateTransitionPreviewResult result;
    result.m_status = StateTransitionPreviewStatus::VALID;
    result.m_reason = "";
    result.m_processedTransactionCount = processedTransactionCount;
    result.m_totalFee = totalFee;
    result.m_touchedAccounts = std::move(touchedAccounts);
    result.m_transactionIds = std::move(transactionIds);
    return result;
}

StateTransitionPreviewResult StateTransitionPreviewResult::rejected(
    StateTransitionPreviewStatus status,
    std::string reason,
    std::size_t processedTransactionCount
) {
    StateTransitionPreviewResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    result.m_processedTransactionCount = processedTransactionCount;
    return result;
}

StateTransitionPreviewStatus StateTransitionPreviewResult::status() const {
    return m_status;
}

const std::string& StateTransitionPreviewResult::reason() const {
    return m_reason;
}

bool StateTransitionPreviewResult::accepted() const {
    return m_status == StateTransitionPreviewStatus::VALID;
}

std::size_t StateTransitionPreviewResult::processedTransactionCount() const {
    return m_processedTransactionCount;
}

utils::Amount StateTransitionPreviewResult::totalFee() const {
    return m_totalFee;
}

const std::vector<std::string>& StateTransitionPreviewResult::touchedAccounts() const {
    return m_touchedAccounts;
}

const std::vector<std::string>& StateTransitionPreviewResult::transactionIds() const {
    return m_transactionIds;
}

std::string StateTransitionPreviewResult::serialize() const {
    std::ostringstream oss;

    oss << "StateTransitionPreviewResult{"
        << "status=" << stateTransitionPreviewStatusToString(m_status)
        << ";reason=" << m_reason
        << ";processedTransactionCount=" << m_processedTransactionCount
        << ";totalFeeRaw=" << m_totalFee.rawUnits()
        << ";touchedAccountCount=" << m_touchedAccounts.size()
        << ";transactionCount=" << m_transactionIds.size()
        << "}";

    return oss.str();
}

StateTransitionPreviewResult StateTransitionPreview::previewBlock(
    const Block& block,
    std::int64_t minimumFeeRawUnits
) {
    if (minimumFeeRawUnits < 0) {
        return StateTransitionPreviewResult::rejected(
            StateTransitionPreviewStatus::INVALID_TRANSACTION,
            "Minimum fee cannot be negative.",
            0
        );
    }

    if (!block.isValid()) {
        return StateTransitionPreviewResult::rejected(
            StateTransitionPreviewStatus::INVALID_BLOCK,
            "Block is structurally invalid.",
            0
        );
    }

    std::set<std::string> sourceIds;
    std::set<std::string> transactionIds;
    std::set<std::string> touchedAccountSet;
    std::vector<std::string> orderedTransactionIds;
    utils::Amount totalFee;
    std::size_t processedTransactionCount = 0;

    for (const LedgerRecord& record : block.records()) {
        if (!record.isValid()) {
            return StateTransitionPreviewResult::rejected(
                StateTransitionPreviewStatus::INVALID_LEDGER_RECORD,
                "Block contains an invalid ledger record.",
                processedTransactionCount
            );
        }

        if (!sourceIds.insert(record.sourceId()).second) {
            return StateTransitionPreviewResult::rejected(
                StateTransitionPreviewStatus::DUPLICATE_TRANSACTION,
                "Block contains duplicate ledger record source ids.",
                processedTransactionCount
            );
        }

        if (record.type() != LedgerRecordType::TRANSACTION) {
            continue;
        }

        try {
            const Transaction transaction =
                Transaction::deserializeForStateReplay(
                    record.payload()
                );

            if (transaction.id() != record.sourceId()) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::INVALID_TRANSACTION,
                    "Transaction ledger source id does not match transaction id.",
                    processedTransactionCount
                );
            }

            if (transaction.nonce() == 0 ||
                !transaction.amount().isPositive() ||
                transaction.fee().isNegative() ||
                transaction.fromAddress().empty() ||
                transaction.toAddress().empty() ||
                transaction.fromAddress() == transaction.toAddress()) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::INVALID_TRANSACTION,
                    "Block contains an invalid transaction ledger payload.",
                    processedTransactionCount
                );
            }

            if (transaction.fee().rawUnits() < minimumFeeRawUnits) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::INVALID_TRANSACTION,
                    "Block contains a transaction below the network minimum fee.",
                    processedTransactionCount
                );
            }

            if (!transactionIds.insert(transaction.id()).second) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::DUPLICATE_TRANSACTION,
                    "Block contains duplicate transaction ids.",
                    processedTransactionCount
                );
            }

            totalFee = totalFee + transaction.fee();
            touchedAccountSet.insert(transaction.fromAddress());
            touchedAccountSet.insert(transaction.toAddress());
            orderedTransactionIds.push_back(transaction.id());
            ++processedTransactionCount;
        } catch (const std::exception& error) {
            return StateTransitionPreviewResult::rejected(
                StateTransitionPreviewStatus::INVALID_TRANSACTION,
                error.what(),
                processedTransactionCount
            );
        }
    }

    return StateTransitionPreviewResult::valid(
        processedTransactionCount,
        totalFee,
        std::vector<std::string>(
            touchedAccountSet.begin(),
            touchedAccountSet.end()
        ),
        orderedTransactionIds
    );
}

} // namespace nodo::core
