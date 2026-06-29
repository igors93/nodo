#include "core/StateTransitionPreview.hpp"

#include "core/CoinLotRegistry.hpp"
#include "core/LedgerRecordDomainValidator.hpp"
#include "core/MerkleTree.hpp"
#include "core/StateRootCalculator.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionExecutionRouter.hpp"
#include "core/TransactionTypePolicy.hpp"
#include "crypto/hash.h"

#include <exception>
#include <map>
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
        case StateTransitionPreviewStatus::INVALID_CONTEXT:
            return "INVALID_CONTEXT";
        case StateTransitionPreviewStatus::INVALID_LEDGER_RECORD:
            return "INVALID_LEDGER_RECORD";
        case StateTransitionPreviewStatus::INVALID_TRANSACTION:
            return "INVALID_TRANSACTION";
        case StateTransitionPreviewStatus::UNSUPPORTED_TRANSITION:
            return "UNSUPPORTED_TRANSITION";
        case StateTransitionPreviewStatus::DUPLICATE_TRANSACTION:
            return "DUPLICATE_TRANSACTION";
        case StateTransitionPreviewStatus::INSUFFICIENT_BALANCE:
            return "INSUFFICIENT_BALANCE";
        case StateTransitionPreviewStatus::INVALID_NONCE:
            return "INVALID_NONCE";
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
      m_touchedDomains(),
      m_transactionIds(),
      m_resultingAccounts(),
      m_stateRoot(""),
      m_receipts(),
      m_receiptsRoot("") {}

StateTransitionPreviewResult StateTransitionPreviewResult::valid(
    std::size_t processedTransactionCount,
    utils::Amount totalFee,
    std::vector<std::string> touchedAccounts,
    std::vector<std::string> touchedDomains,
    std::vector<std::string> transactionIds,
    std::vector<AccountState> resultingAccounts,
    std::string stateRoot,
    std::vector<TransactionReceipt> receipts,
    std::string receiptsRoot
) {
    StateTransitionPreviewResult result;
    result.m_status = StateTransitionPreviewStatus::VALID;
    result.m_reason = "";
    result.m_processedTransactionCount = processedTransactionCount;
    result.m_totalFee = totalFee;
    result.m_touchedAccounts = std::move(touchedAccounts);
    result.m_touchedDomains = std::move(touchedDomains);
    result.m_transactionIds = std::move(transactionIds);
    result.m_resultingAccounts = std::move(resultingAccounts);
    result.m_stateRoot = std::move(stateRoot);
    result.m_receipts = std::move(receipts);
    result.m_receiptsRoot = std::move(receiptsRoot);
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

const std::vector<std::string>& StateTransitionPreviewResult::touchedDomains() const {
    return m_touchedDomains;
}

const std::vector<std::string>& StateTransitionPreviewResult::transactionIds() const {
    return m_transactionIds;
}

const std::vector<AccountState>& StateTransitionPreviewResult::resultingAccounts() const {
    return m_resultingAccounts;
}

const std::string& StateTransitionPreviewResult::stateRoot() const {
    return m_stateRoot;
}

const std::vector<TransactionReceipt>& StateTransitionPreviewResult::receipts() const {
    return m_receipts;
}

const std::string& StateTransitionPreviewResult::receiptsRoot() const {
    return m_receiptsRoot;
}

std::string StateTransitionPreviewResult::serialize() const {
    std::ostringstream oss;

    oss << "StateTransitionPreviewResult{"
        << "status=" << stateTransitionPreviewStatusToString(m_status)
        << ";reason=" << m_reason
        << ";processedTransactionCount=" << m_processedTransactionCount
        << ";totalFeeRaw=" << m_totalFee.rawUnits()
        << ";touchedAccountCount=" << m_touchedAccounts.size()
        << ";touchedDomainCount=" << m_touchedDomains.size()
        << ";transactionCount=" << m_transactionIds.size()
        << ";resultingAccountCount=" << m_resultingAccounts.size()
        << ";stateRoot=" << m_stateRoot
        << ";receiptCount=" << m_receipts.size()
        << ";receiptsRoot=" << m_receiptsRoot
        << "}";

    return oss.str();
}

StateTransitionPreviewResult StateTransitionPreview::previewBlock(
    const Block& block,
    std::int64_t minimumFeeRawUnits
) {
    return previewBlock(
        block,
        StateTransitionPreviewContext::structuralOnly(
            minimumFeeRawUnits
        )
    );
}

StateTransitionPreviewResult StateTransitionPreview::previewBlock(
    const Block& block,
    const StateTransitionPreviewContext& context
) {
    if (!context.isValid()) {
        return StateTransitionPreviewResult::rejected(
            StateTransitionPreviewStatus::INVALID_CONTEXT,
            "State transition preview context is invalid.",
            0
        );
    }

    if (!block.isValid(false)) {
        return StateTransitionPreviewResult::rejected(
            StateTransitionPreviewStatus::INVALID_BLOCK,
            "Block is structurally invalid.",
            0
        );
    }

    std::set<std::string> sourceIds;
    std::set<std::string> transactionIds;
    std::set<std::string> touchedAccountSet;
    std::set<std::string> touchedDomainSet;
    std::vector<std::string> orderedTransactionIds;
    std::vector<Transaction> orderedTransactions;
    std::vector<LedgerRecord> orderedProtocolRecords;
    std::vector<TransactionReceipt> receipts;
    AccountStateView workingAccountState =
        context.accountStateView();
    std::optional<CoinLotRegistry> workingCoinLotRegistry =
        context.coinLotPreviewEnabled()
            ? std::optional<CoinLotRegistry>(context.coinLotRegistry())
            : std::nullopt;
    std::unique_ptr<TransactionDomainExecutor> domainExecutor =
        context.createDomainExecutor();
    utils::Amount totalFee;
    std::size_t processedTransactionCount = 0;
    bool evidenceSectionStarted = false;
    std::string previousEvidenceId;

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
            const LedgerRecordDomainValidator::Result domainResult =
                LedgerRecordDomainValidator::validate(record);
            if (!domainResult.valid) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::INVALID_LEDGER_RECORD,
                    "Non-transaction ledger record failed domain validation: " + domainResult.reason,
                    processedTransactionCount
                );
            }
            if (record.type() != LedgerRecordType::SLASHING_EVIDENCE) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::UNSUPPORTED_TRANSITION,
                    "Non-genesis block contains a record without an implemented deterministic state transition.",
                    processedTransactionCount
                );
            }
            if (record.timestamp() != block.timestamp() ||
                (!previousEvidenceId.empty() &&
                 record.sourceId() <= previousEvidenceId)) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::INVALID_LEDGER_RECORD,
                    "Slashing evidence records must use the block timestamp and canonical id order.",
                    processedTransactionCount
                );
            }
            evidenceSectionStarted = true;
            previousEvidenceId = record.sourceId();
            orderedProtocolRecords.push_back(record);
            touchedDomainSet.insert("slashing");
            touchedDomainSet.insert("validators");
            touchedDomainSet.insert("staking");
            continue;
        }

        if (evidenceSectionStarted) {
            return StateTransitionPreviewResult::rejected(
                StateTransitionPreviewStatus::INVALID_LEDGER_RECORD,
                "Transactions cannot appear after slashing evidence records.",
                processedTransactionCount
            );
        }

        try {
            const Transaction transaction =
                Transaction::deserialize(
                    record.payload()
                );

            if (transaction.id() != record.sourceId()) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::INVALID_TRANSACTION,
                    "Transaction ledger source id does not match transaction id.",
                    processedTransactionCount
                );
            }

            if (context.protocolAuthorizationEnabled()) {
                const crypto::ProtocolCryptoContext& cryptoContext =
                    context.cryptoContext();
                if (!transaction.verifyAuthorization(
                        context.expectedChainId(),
                        cryptoContext.policy(),
                        crypto::SecurityContext::USER_TRANSACTION,
                        cryptoContext.userSignatureProvider()
                    )) {
                    return StateTransitionPreviewResult::rejected(
                        StateTransitionPreviewStatus::INVALID_TRANSACTION,
                        "Transaction chain binding, sender binding, or signature verification failed.",
                        processedTransactionCount
                    );
                }
            }

            std::string shapeReason;
            if (transaction.nonce() == 0 || transaction.fee().isNegative() ||
                !TransactionTypePolicyRegistry::validateShape(transaction, shapeReason)) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::INVALID_TRANSACTION,
                    "Block contains an invalid transaction ledger payload: " + shapeReason,
                    processedTransactionCount
                );
            }

            if (transaction.fee().rawUnits() < context.minimumFeeRawUnits()) {
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

            const TransactionExecutionContext executionContext(
                workingAccountState, block.index(), block.timestamp(),
                context.enforceAccountState(), context.allowMissingAccounts(),
                context.requireDomainExecutor(), context.feeRecipientAddress(),
                workingCoinLotRegistry.has_value() ? &*workingCoinLotRegistry : nullptr,
                domainExecutor.get()
            );
            const TransactionExecutionResult execution =
                TransactionExecutionRouter::execute(transaction, executionContext);
            if (!execution.success()) {
                StateTransitionPreviewStatus status = StateTransitionPreviewStatus::INVALID_TRANSACTION;
                if (execution.status() == TransactionExecutionStatus::INVALID_NONCE) {
                    status = StateTransitionPreviewStatus::INVALID_NONCE;
                } else if (execution.status() == TransactionExecutionStatus::INSUFFICIENT_BALANCE) {
                    status = StateTransitionPreviewStatus::INSUFFICIENT_BALANCE;
                }
                return StateTransitionPreviewResult::rejected(
                    status, execution.code() + ": " + execution.reason(), processedTransactionCount
                );
            }

            workingAccountState = execution.accounts();
            for (const auto& address : execution.touchedAccounts()) {
                touchedAccountSet.insert(address);
            }
            for (const auto& domain : execution.touchedDomains()) {
                touchedDomainSet.insert(domain);
            }
            if (context.enforceAccountState()) {
                receipts.push_back(execution.receipt());
            }

            totalFee = totalFee + transaction.fee();
            orderedTransactionIds.push_back(transaction.id());
            orderedTransactions.push_back(transaction);
            ++processedTransactionCount;
        } catch (const std::exception& error) {
            return StateTransitionPreviewResult::rejected(
                StateTransitionPreviewStatus::INVALID_TRANSACTION,
                error.what(),
                processedTransactionCount
            );
        }
    }

    std::vector<std::string> receiptPayloads;
    receiptPayloads.reserve(receipts.size());
    for (const TransactionReceipt& receipt : receipts) {
        if (!receipt.isValid()) {
            return StateTransitionPreviewResult::rejected(
                StateTransitionPreviewStatus::INVALID_TRANSACTION,
                "State transition generated an invalid transaction receipt.",
                processedTransactionCount
            );
        }
        receiptPayloads.push_back(receipt.serialize());
    }

    DeterministicStateTransitionResult protocolTransition;
    try {
        if (domainExecutor) {
            const TransactionDomainExecutionResult finalized = domainExecutor->finalizeBlock(
                workingAccountState, totalFee, orderedProtocolRecords,
                block.index(), block.timestamp()
            );
            protocolTransition = finalized.applied()
                ? DeterministicStateTransitionResult::accepted(finalized.accounts(), finalized.domains())
                : DeterministicStateTransitionResult::rejected(finalized.code() + ": " + finalized.reason());
        } else {
            protocolTransition = context.transitionProtocolState(
                workingAccountState, totalFee, orderedTransactions,
                orderedProtocolRecords, block.timestamp()
            );
        }
    } catch (const std::exception& error) {
        return StateTransitionPreviewResult::rejected(
            StateTransitionPreviewStatus::INVALID_CONTEXT,
            std::string("Protocol state transition failed: ") + error.what(),
            processedTransactionCount
        );
    }

    if (!protocolTransition.valid()) {
        return StateTransitionPreviewResult::rejected(
            StateTransitionPreviewStatus::INVALID_TRANSACTION,
            protocolTransition.reason(),
            processedTransactionCount
        );
    }

    workingAccountState = protocolTransition.accounts();

    if (totalFee.isPositive()) {
        touchedDomainSet.insert("accounts");
        touchedDomainSet.insert("burns");
        touchedDomainSet.insert("supply");
    }

    std::map<std::string, std::string> stateRootDomains = protocolTransition.domains();
    if (workingCoinLotRegistry.has_value()) {
        const std::string registrySerial = workingCoinLotRegistry->serialize();
        char lotDigest[NODO_HASH_BUFFER_SIZE] = {0};
        nodo_hash_string(registrySerial.c_str(), lotDigest, sizeof(lotDigest));
        stateRootDomains["coin_lots"] = std::string(lotDigest);
    }

    const std::string combinedStateRoot =
        StateRootCalculator::calculateProtocolStateRoot(
            workingAccountState,
            stateRootDomains
        );
    if (combinedStateRoot.empty()) {
        return StateTransitionPreviewResult::rejected(
            StateTransitionPreviewStatus::INVALID_CONTEXT,
            "Protocol state transition produced an invalid state commitment.",
            processedTransactionCount
        );
    }

    return StateTransitionPreviewResult::valid(
        processedTransactionCount,
        totalFee,
        std::vector<std::string>(
            touchedAccountSet.begin(),
            touchedAccountSet.end()
        ),
        std::vector<std::string>(
            touchedDomainSet.begin(),
            touchedDomainSet.end()
        ),
        orderedTransactionIds,
        workingAccountState.accounts(),
        combinedStateRoot,
        std::move(receipts),
        MerkleTree::buildRoot(receiptPayloads)
    );
}

} // namespace nodo::core
