#include "core/ChainStateRebuilder.hpp"

#include "core/LedgerRecord.hpp"
#include "core/Transaction.hpp"
#include "economics/MintRecord.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

StateRebuildReport::StateRebuildReport()
    : m_success(true),
      m_failureReason(""),
      m_blockCount(0),
      m_ledgerRecordCount(0),
      m_mintRecordCount(0),
      m_transactionRecordCount(0) {}

bool StateRebuildReport::success() const {
    return m_success;
}

const std::string& StateRebuildReport::failureReason() const {
    return m_failureReason;
}

std::size_t StateRebuildReport::blockCount() const {
    return m_blockCount;
}

std::size_t StateRebuildReport::ledgerRecordCount() const {
    return m_ledgerRecordCount;
}

std::size_t StateRebuildReport::mintRecordCount() const {
    return m_mintRecordCount;
}

std::size_t StateRebuildReport::transactionRecordCount() const {
    return m_transactionRecordCount;
}

void StateRebuildReport::markFailure(std::string reason) {
    m_success = false;
    m_failureReason = std::move(reason);
}

void StateRebuildReport::setBlockCount(std::size_t value) {
    m_blockCount = value;
}

void StateRebuildReport::incrementLedgerRecordCount() {
    ++m_ledgerRecordCount;
}

void StateRebuildReport::incrementMintRecordCount() {
    ++m_mintRecordCount;
}

void StateRebuildReport::incrementTransactionRecordCount() {
    ++m_transactionRecordCount;
}

std::string StateRebuildReport::serialize() const {
    std::ostringstream oss;

    oss << "StateRebuildReport{"
        << "success=" << (m_success ? "true" : "false")
        << ";failureReason=" << m_failureReason
        << ";blockCount=" << m_blockCount
        << ";ledgerRecordCount=" << m_ledgerRecordCount
        << ";mintRecordCount=" << m_mintRecordCount
        << ";transactionRecordCount=" << m_transactionRecordCount
        << "}";

    return oss.str();
}

StateRebuildReport ChainStateRebuilder::auditBlockchain(
    const Blockchain& blockchain
) {
    StateRebuildReport report;

    if (blockchain.empty()) {
        report.markFailure("Blockchain is empty.");
        return report;
    }

    if (!blockchain.isValid()) {
        report.markFailure("Blockchain validation failed.");
        return report;
    }

    report.setBlockCount(blockchain.size());

    for (const auto& block : blockchain.blocks()) {
        if (!block.isValid()) {
            report.markFailure("Invalid block found during rebuild audit.");
            return report;
        }

        for (const auto& record : block.records()) {
            if (!record.isValid()) {
                report.markFailure("Invalid LedgerRecord found during rebuild audit.");
                return report;
            }

            report.incrementLedgerRecordCount();

            if (record.type() == LedgerRecordType::MINT) {
                report.incrementMintRecordCount();
            } else if (record.type() == LedgerRecordType::TRANSACTION) {
                report.incrementTransactionRecordCount();
            } else {
                report.markFailure("Unknown LedgerRecord type found during rebuild audit.");
                return report;
            }
        }
    }

    return report;
}

State ChainStateRebuilder::rebuildStateFromMintRecords(
    const Blockchain& blockchain
) {
    StateRebuildReport report = auditBlockchain(blockchain);

    if (!report.success()) {
        throw std::logic_error(
            "Cannot rebuild State from invalid Blockchain: "
            + report.failureReason()
        );
    }

    State rebuiltState;

    for (const auto& block : blockchain.blocks()) {
        for (const auto& record : block.records()) {
            if (record.type() != LedgerRecordType::MINT) {
                continue;
            }

            economics::MintRecord mintRecord =
                economics::MintRecord::deserialize(record.payload());

            rebuiltState.applyMintRecord(mintRecord);
        }
    }

    if (!rebuiltState.isSupplyAuditable()) {
        throw std::logic_error("Rebuilt State failed supply audit.");
    }

    return rebuiltState;
}

State ChainStateRebuilder::rebuildStateFromLedgerRecords(
    const Blockchain& blockchain
) {
    StateRebuildReport report = auditBlockchain(blockchain);

    if (!report.success()) {
        throw std::logic_error(
            "Cannot rebuild State from invalid Blockchain: "
            + report.failureReason()
        );
    }

    State rebuiltState;

    for (const auto& block : blockchain.blocks()) {
        if (rebuiltState.currentBlockIndex() != block.index()) {
            throw std::logic_error("State block index does not match Blockchain block index.");
        }

        for (const auto& record : block.records()) {
            if (record.type() == LedgerRecordType::MINT) {
                economics::MintRecord mintRecord =
                    economics::MintRecord::deserialize(record.payload());

                rebuiltState.applyMintRecord(mintRecord);
                continue;
            }

            if (record.type() == LedgerRecordType::TRANSACTION) {
                Transaction transaction =
                    Transaction::deserializeForStateReplay(record.payload());

                if (transaction.type() != TransactionType::TRANSFER) {
                    throw std::logic_error("Unsupported transaction type during State rebuild.");
                }

                rebuiltState.applyTransferTransaction(transaction);
                continue;
            }

            throw std::logic_error("Unsupported LedgerRecord type during State rebuild.");
        }

        if (block.index() + 1 < blockchain.size()) {
            rebuiltState.advanceBlock();
        }
    }

    if (!rebuiltState.isSupplyAuditable()) {
        throw std::logic_error("Rebuilt State failed supply audit.");
    }

    return rebuiltState;
}

} // namespace nodo::core