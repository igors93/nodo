#include "core/StateTransitionPreview.hpp"

#include "core/LedgerRecordDomainValidator.hpp"
#include "core/MerkleTree.hpp"
#include "core/StateRootCalculator.hpp"
#include "core/Transaction.hpp"

#include <exception>
#include <limits>
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
      m_transactionIds(),
      m_resultingAccounts(),
      m_stateRoot(""),
      m_receipts(),
      m_receiptsRoot("") {}

StateTransitionPreviewResult StateTransitionPreviewResult::valid(
    std::size_t processedTransactionCount,
    utils::Amount totalFee,
    std::vector<std::string> touchedAccounts,
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
    std::vector<std::string> orderedTransactionIds;
    std::vector<Transaction> orderedTransactions;
    std::vector<LedgerRecord> orderedProtocolRecords;
    std::vector<TransactionReceipt> receipts;
    AccountStateView workingAccountState =
        context.accountStateView();
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

            const bool isTransfer =
                transaction.type() == TransactionType::TRANSFER;
            const bool isGovernanceProposal =
                transaction.type() == TransactionType::GOVERNANCE_PROPOSE;
            if (!isTransfer && !isGovernanceProposal) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::UNSUPPORTED_TRANSITION,
                    "Transaction type has no authoritative deterministic state transition.",
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

            if (transaction.nonce() == 0 ||
                (isTransfer && !transaction.amount().isPositive()) ||
                (isGovernanceProposal && !transaction.amount().isZero()) ||
                transaction.fee().isNegative() ||
                transaction.fromAddress().empty() ||
                transaction.toAddress().empty() ||
                (isTransfer && transaction.fromAddress() == transaction.toAddress())) {
                return StateTransitionPreviewResult::rejected(
                    StateTransitionPreviewStatus::INVALID_TRANSACTION,
                    "Block contains an invalid transaction ledger payload.",
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

            if (context.enforceAccountState()) {
                if (!workingAccountState.hasAccount(transaction.fromAddress()) &&
                    !context.allowMissingAccounts()) {
                    return StateTransitionPreviewResult::rejected(
                        StateTransitionPreviewStatus::INVALID_TRANSACTION,
                        "Sender account does not exist in preview state.",
                        processedTransactionCount
                    );
                }

                const AccountState sender =
                    workingAccountState.accountOrDefault(transaction.fromAddress());

                const AccountState recipient = isTransfer
                    ? workingAccountState.accountOrDefault(transaction.toAddress())
                    : AccountState();

                if (!sender.isValid() ||
                    (isTransfer && !recipient.isValid())) {
                    return StateTransitionPreviewResult::rejected(
                        StateTransitionPreviewStatus::INVALID_TRANSACTION,
                        "Preview account state is invalid.",
                        processedTransactionCount
                    );
                }

                if (sender.nonce() == std::numeric_limits<std::uint64_t>::max()) {
                    return StateTransitionPreviewResult::rejected(
                        StateTransitionPreviewStatus::INVALID_NONCE,
                        "Sender nonce cannot advance without overflow.",
                        processedTransactionCount
                    );
                }

                const std::uint64_t expectedNonce =
                    sender.nonce() + 1;

                if (transaction.nonce() != expectedNonce) {
                    return StateTransitionPreviewResult::rejected(
                        StateTransitionPreviewStatus::INVALID_NONCE,
                        "Transaction nonce does not match expected sender nonce.",
                        processedTransactionCount
                    );
                }

                const utils::Amount required = isTransfer
                    ? transaction.amount() + transaction.fee()
                    : transaction.fee();

                if (sender.balance() < required) {
                    return StateTransitionPreviewResult::rejected(
                        StateTransitionPreviewStatus::INSUFFICIENT_BALANCE,
                        "Sender balance is insufficient for amount plus fee.",
                        processedTransactionCount
                    );
                }

                const utils::Amount senderBalance =
                    sender.balance() - required;

                if (!workingAccountState.putAccount(
                        AccountState(
                            sender.address(),
                            senderBalance,
                            transaction.nonce()
                        )
                    )) {
                    return StateTransitionPreviewResult::rejected(
                        StateTransitionPreviewStatus::INVALID_TRANSACTION,
                        "Preview account state update failed.",
                        processedTransactionCount
                    );
                }

                if (isTransfer &&
                    !workingAccountState.putAccount(
                        AccountState(
                            recipient.address(),
                            recipient.balance() + transaction.amount(),
                            recipient.nonce()
                        )
                    )) {
                    return StateTransitionPreviewResult::rejected(
                        StateTransitionPreviewStatus::INVALID_TRANSACTION,
                        "Preview recipient account update failed.",
                        processedTransactionCount
                    );
                }

                if (!context.feeRecipientAddress().empty()) {
                    const AccountState feeRecipient =
                        workingAccountState.accountOrDefault(
                            context.feeRecipientAddress()
                        );

                    if (!workingAccountState.putAccount(
                            AccountState(
                                feeRecipient.address(),
                                feeRecipient.balance() + transaction.fee(),
                                feeRecipient.nonce()
                            )
                        )) {
                        return StateTransitionPreviewResult::rejected(
                            StateTransitionPreviewStatus::INVALID_TRANSACTION,
                            "Preview fee recipient update failed.",
                            processedTransactionCount
                        );
                    }

                    touchedAccountSet.insert(context.feeRecipientAddress());
                }

                receipts.push_back(
                    TransactionReceipt::applied(
                        transaction.id(),
                        transaction.fromAddress(),
                        isTransfer ? transaction.toAddress() : "nodo_governance",
                        transaction.amount(),
                        transaction.fee(),
                        sender.nonce(),
                        transaction.nonce(),
                        StateRootCalculator::calculateAccountStateRoot(
                            workingAccountState
                        )
                    )
                );
            }

            totalFee = totalFee + transaction.fee();
            touchedAccountSet.insert(transaction.fromAddress());
            if (isTransfer) {
                touchedAccountSet.insert(transaction.toAddress());
            }
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
        protocolTransition = context.transitionProtocolState(
            workingAccountState,
            totalFee,
            orderedTransactions,
            orderedProtocolRecords,
            block.timestamp()
        );
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
    const std::string combinedStateRoot =
        StateRootCalculator::calculateProtocolStateRoot(
            workingAccountState,
            protocolTransition.domains()
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
        orderedTransactionIds,
        workingAccountState.accounts(),
        combinedStateRoot,
        std::move(receipts),
        MerkleTree::buildRoot(receiptPayloads)
    );
}

} // namespace nodo::core
