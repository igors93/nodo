#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "core/MerkleTree.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "crypto/hash.h"
#include "economics/MintRecord.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

core::Transaction transaction(
    std::uint64_t nonce,
    std::int64_t feeRawUnits = 10,
    std::string fromAddress = "preview-sender",
    std::string toAddress = "preview-recipient"
) {
    core::Transaction tx(
        core::TransactionType::TRANSFER,
        std::move(fromAddress),
        std::move(toAddress),
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(feeRawUnits),
        nonce,
        kTimestamp + static_cast<std::int64_t>(nonce)
    );

    const crypto::KeyPair keyPair =
        crypto::KeyPair::createDeterministicEd25519KeyPair("preview-key");
    const crypto::Ed25519SignatureProvider provider;

    tx.attachSignatureBundle(
        crypto::SignatureBundle::createSignature(
            tx.signingPayload(),
            keyPair.publicKey(),
            keyPair.privateKeyForSigningOnly(),
            kTimestamp,
            provider,
            crypto::SigningDomain::USER_TRANSACTION
        )
    );

    return tx;
}

core::LedgerRecord record(
    const core::Transaction& tx
) {
    return core::LedgerRecord::fromTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        kTimestamp
    );
}

std::string hashString(
    const std::string& value
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(
        value.c_str(),
        output,
        sizeof(output)
    );
    return std::string(output);
}

core::LedgerRecord persistedTransactionRecord(
    const std::string& sourceId,
    const std::string& payload
) {
    const std::string payloadHash =
        hashString(payload);

    const std::string recordId =
        hashString(
            "LedgerRecordId{type=TRANSACTION;sourceId="
            + sourceId
            + ";payloadHash="
            + payloadHash
            + ";timestamp="
            + std::to_string(kTimestamp)
            + "}"
        );

    return core::LedgerRecord::fromPersistedFields(
        recordId,
        core::LedgerRecordType::TRANSACTION,
        sourceId,
        payload,
        payloadHash,
        kTimestamp
    );
}

core::LedgerRecord mintLedgerRecord(
    const std::string& recipientAddress = "mint-recipient",
    std::int64_t amountRaw = 500,
    std::int64_t ts = kTimestamp
) {
    economics::MintRecord mint(
        "mint-id-" + recipientAddress,
        "auth-id-" + recipientAddress,
        recipientAddress,
        utils::Amount::fromRawUnits(amountRaw),
        economics::MintReason::GENESIS_ALLOCATION,
        1,
        0,
        "genesis-block-hash",
        ts
    );
    return core::LedgerRecord::fromMintRecord(mint, ts);
}

core::StateTransitionPreviewContext economicContext(
    std::int64_t senderBalanceRawUnits = 1000,
    std::uint64_t senderNonce = 0,
    std::int64_t minimumFeeRawUnits = 10
) {
    core::AccountStateView view;

    if (!view.putAccount(
            core::AccountState(
                "preview-sender",
                utils::Amount::fromRawUnits(senderBalanceRawUnits),
                senderNonce
            )
        )) {
        throw std::runtime_error("Failed to create preview sender account.");
    }

    return core::StateTransitionPreviewContext(
        minimumFeeRawUnits,
        view,
        false,
        true
    );
}

core::AccountState findAccount(
    const std::vector<core::AccountState>& accounts,
    const std::string& address
) {
    for (const core::AccountState& account : accounts) {
        if (account.address() == address) {
            return account;
        }
    }

    throw std::runtime_error("Expected account not found: " + address);
}

void testValidPreviewSummarizesTransactions() {
    const core::Transaction first =
        transaction(1, 10);

    const core::Transaction second =
        transaction(2, 15);

    const core::Block block(
        1,
        "previous-hash",
        {record(first), record(second)},
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            10
        );

    requireCondition(
        result.accepted() &&
        result.processedTransactionCount() == 2 &&
        result.totalFee().rawUnits() == 25 &&
        result.transactionIds().size() == 2 &&
        result.touchedAccounts().size() == 2,
        "Valid preview should summarize processed transactions without mutating real state."
    );
}

void testValidPreviewAppliesEconomicState() {
    const core::Block block(
        1,
        "previous-hash",
        {record(transaction(1, 10))},
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext()
        );

    const core::AccountState sender =
        findAccount(
            result.resultingAccounts(),
            "preview-sender"
        );

    const core::AccountState recipient =
        findAccount(
            result.resultingAccounts(),
            "preview-recipient"
        );

    requireCondition(
        result.accepted() &&
        result.processedTransactionCount() == 1 &&
        result.totalFee().rawUnits() == 10 &&
        !result.stateRoot().empty() &&
        result.receipts().size() == 1 &&
        result.receipts()[0].isValid() &&
        result.receipts()[0].applied() &&
        result.receipts()[0].senderNonceBefore() == 0 &&
        result.receipts()[0].senderNonceAfter() == 1 &&
        result.receipts()[0].stateRootAfter() == result.stateRoot() &&
        result.receiptsRoot() == core::MerkleTree::buildRoot(
            {result.receipts()[0].serialize()}
        ) &&
        sender.balance().rawUnits() == 890 &&
        sender.nonce() == 1 &&
        recipient.balance().rawUnits() == 100 &&
        recipient.nonce() == 0,
        "Economic preview should debit sender, credit recipient and advance sender nonce."
    );
}

void testRejectsInsufficientBalance() {
    const core::Block block(
        1,
        "previous-hash",
        {record(transaction(1, 10))},
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext(109)
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INSUFFICIENT_BALANCE,
        "Economic preview should reject insufficient sender balance."
    );
}

void testRejectsLowerNonce() {
    const core::Block block(
        1,
        "previous-hash",
        {record(transaction(1, 10))},
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext(1000, 1)
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INVALID_NONCE,
        "Economic preview should reject a nonce lower than the expected nonce."
    );
}

void testRejectsHigherNonce() {
    const core::Block block(
        1,
        "previous-hash",
        {record(transaction(3, 10))},
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext(1000, 0)
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INVALID_NONCE,
        "Economic preview should reject a nonce higher than the expected nonce."
    );
}

void testAcceptsCorrectNonce() {
    const core::Block block(
        1,
        "previous-hash",
        {record(transaction(2, 10))},
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext(1000, 1)
        );

    requireCondition(
        result.accepted(),
        "Economic preview should accept the exact next sender nonce."
    );
}

void testAcceptsSequentialTransactionsFromSameAccount() {
    const core::Block block(
        1,
        "previous-hash",
        {
            record(transaction(1, 10)),
            record(transaction(2, 10, "preview-sender", "second-recipient"))
        },
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext(1000, 0)
        );

    const core::AccountState sender =
        findAccount(
            result.resultingAccounts(),
            "preview-sender"
        );

    requireCondition(
        result.accepted() &&
        result.processedTransactionCount() == 2 &&
        sender.balance().rawUnits() == 780 &&
        sender.nonce() == 2,
        "Economic preview should accept sequential nonces from the same sender."
    );
}

void testRejectsDuplicateNonceFromSameAccount() {
    const core::Block block(
        1,
        "previous-hash",
        {
            record(transaction(1, 10)),
            record(transaction(1, 10, "preview-sender", "second-recipient"))
        },
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext(1000, 0)
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INVALID_NONCE &&
        result.processedTransactionCount() == 1,
        "Economic preview should reject duplicate sender nonce in one block."
    );
}

void testRejectsTransactionsThatTogetherExceedBalance() {
    const core::Block block(
        1,
        "previous-hash",
        {
            record(transaction(1, 10)),
            record(transaction(2, 10, "preview-sender", "second-recipient"))
        },
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext(210, 0)
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INSUFFICIENT_BALANCE &&
        result.processedTransactionCount() == 1,
        "Economic preview should reject a block when cumulative spends exceed balance."
    );
}

void testRejectsDuplicateTransaction() {
    const core::Transaction tx =
        transaction(1, 10);

    const core::LedgerRecord ledgerRecord =
        record(tx);

    const core::Block block(
        1,
        "previous-hash",
        {ledgerRecord, ledgerRecord},
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            10
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::DUPLICATE_TRANSACTION &&
        result.processedTransactionCount() == 1,
        "Preview should reject duplicate transaction/source ids and report processed count before failure."
    );
}

void testRejectsTransactionBelowMinimumFee() {
    const core::Block block(
        1,
        "previous-hash",
        {record(transaction(1, 9))},
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            10
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INVALID_TRANSACTION &&
        result.reason().find("minimum fee") != std::string::npos,
        "Preview should reject transactions below the network minimum fee."
    );
}

void testRejectsSenderEqualsRecipient() {
    const core::Transaction tx =
        transaction(
            1,
            10,
            "preview-sender",
            "preview-sender"
        );

    const core::Block block(
        1,
        "previous-hash",
        {
            persistedTransactionRecord(
                tx.id(),
                tx.serialize()
            )
        },
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext()
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INVALID_TRANSACTION,
        "Preview should reject transactions where sender and recipient are equal."
    );
}

void testRejectsInvalidPayload() {
    const core::Block block(
        1,
        "previous-hash",
        {
            persistedTransactionRecord(
                "invalid-payload-source",
                "not-a-transaction"
            )
        },
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            economicContext()
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INVALID_TRANSACTION,
        "Preview should reject impossible transaction payloads."
    );
}

void testFailureDoesNotMutateInitialState() {
    core::AccountStateView view;
    view.putAccount(
        core::AccountState(
            "preview-sender",
            utils::Amount::fromRawUnits(1000),
            0
        )
    );

    const core::StateTransitionPreviewContext context(
        10,
        view,
        false,
        true
    );

    const core::Block block(
        1,
        "previous-hash",
        {
            record(transaction(1, 10)),
            record(transaction(3, 10, "preview-sender", "second-recipient"))
        },
        kTimestamp + 10
    );

    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(
            block,
            context
        );

    const core::AccountState originalSender =
        view.accountOrDefault("preview-sender");

    requireCondition(
        !result.accepted() &&
        result.processedTransactionCount() == 1 &&
        originalSender.balance().rawUnits() == 1000 &&
        originalSender.nonce() == 0,
        "Preview failure should not mutate the original account state view."
    );
}

void testRejectsEmptyProtocolStateCommitment() {
    core::AccountStateView view;
    view.putAccount(core::AccountState(
        "preview-sender",
        utils::Amount::fromRawUnits(1000),
        0
    ));

    const core::StateTransitionPreviewContext context(
        10,
        view,
        false,
        true,
        "",
        0,
        "",
        "",
        {},
        [](const core::AccountStateView& accounts,
           utils::Amount,
           const std::vector<core::Transaction>&,
           const std::vector<core::LedgerRecord>&,
           std::int64_t) {
            return core::DeterministicStateTransitionResult::accepted(
                accounts,
                {{"accounts", "shadowed-account-state"}}
            );
        }
    );

    const core::Block block(
        1,
        "previous-hash",
        {record(transaction(1, 10))},
        kTimestamp + 10
    );
    const core::StateTransitionPreviewResult result =
        core::StateTransitionPreview::previewBlock(block, context);

    requireCondition(
        !result.accepted() &&
        result.status() == core::StateTransitionPreviewStatus::INVALID_CONTEXT,
        "Preview must reject a transition that cannot produce a state commitment."
    );
}

} // namespace

int main() {
    try {
        testValidPreviewSummarizesTransactions();
        testValidPreviewAppliesEconomicState();
        testRejectsInsufficientBalance();
        testRejectsLowerNonce();
        testRejectsHigherNonce();
        testAcceptsCorrectNonce();
        testAcceptsSequentialTransactionsFromSameAccount();
        testRejectsDuplicateNonceFromSameAccount();
        testRejectsTransactionsThatTogetherExceedBalance();
        testRejectsDuplicateTransaction();
        testRejectsTransactionBelowMinimumFee();
        testRejectsSenderEqualsRecipient();
        testRejectsInvalidPayload();
        testFailureDoesNotMutateInitialState();
        testRejectsEmptyProtocolStateCommitment();

        std::cout << "Nodo state transition preview tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo state transition preview tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
