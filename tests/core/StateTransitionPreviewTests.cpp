#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

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
    std::int64_t feeRawUnits = 10
) {
    core::Transaction tx(
        core::TransactionType::TRANSFER,
        "preview-sender",
        "preview-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(feeRawUnits),
        nonce,
        kTimestamp + static_cast<std::int64_t>(nonce)
    );

    const crypto::PublicKey publicKey(
        crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "preview-public-key"
    );

    const crypto::PrivateKey privateKey(
        crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "preview-private-key"
    );

    tx.attachSignatureBundle(
        crypto::SignatureBundle::createDevelopmentSignature(
            tx.signingPayload(),
            publicKey,
            privateKey,
            kTimestamp
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

} // namespace

int main() {
    try {
        testValidPreviewSummarizesTransactions();
        testRejectsDuplicateTransaction();
        testRejectsTransactionBelowMinimumFee();

        std::cout << "Nodo state transition preview tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo state transition preview tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
