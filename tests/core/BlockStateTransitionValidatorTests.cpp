#include "core/Block.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreviewContext.hpp"
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
    const std::string& nonce,
    std::int64_t feeRawUnits = 1
) {
    core::Transaction tx(
        core::TransactionType::TRANSFER,
        "validator-test-sender",
        "validator-test-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(feeRawUnits),
        static_cast<std::uint64_t>(std::stoull(nonce)),
        kTimestamp
    );

    const crypto::PublicKey publicKey(
        crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "state-transition-validator-public-key"
    );

    const crypto::PrivateKey privateKey(
        crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "state-transition-validator-private-key"
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

core::Blockchain chain() {
    const core::Transaction tx =
        transaction("1");

    core::Blockchain blockchain;
    blockchain.addGenesisBlock(
        core::Block::createGenesisBlock(
            {record(tx)},
            kTimestamp
        )
    );

    return blockchain;
}

core::StateTransitionPreviewContext economicContext(
    std::int64_t senderBalanceRawUnits = 1000,
    std::uint64_t senderNonce = 0,
    std::int64_t minimumFeeRawUnits = 1
) {
    core::AccountStateView view;

    if (!view.putAccount(
            core::AccountState(
                "validator-test-sender",
                utils::Amount::fromRawUnits(senderBalanceRawUnits),
                senderNonce
            )
        )) {
        throw std::runtime_error("Failed to create validator preview account state.");
    }

    return core::StateTransitionPreviewContext(
        minimumFeeRawUnits,
        view,
        false,
        true
    );
}

void testAcceptsAppendableCandidate() {
    const core::Blockchain blockchain =
        chain();

    const core::Transaction tx =
        transaction("2");

    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1
    );

    const core::BlockValidationResult result =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            blockchain,
            block
        );

    requireCondition(
        result.accepted(),
        "Appendable candidate block should pass state transition validation."
    );
}

void testAcceptsCandidateWithValidEconomicState() {
    const core::Blockchain blockchain =
        chain();

    const core::Transaction tx =
        transaction("2", 5);

    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1
    );

    const core::BlockValidationResult result =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            blockchain,
            block,
            economicContext(1000, 1, 5)
        );

    requireCondition(
        result.accepted() &&
        !result.stateRoot().empty(),
        "Candidate block with valid balance and nonce should pass validation."
    );
}

void testRejectsCandidateWithInsufficientBalance() {
    const core::Blockchain blockchain =
        chain();

    const core::Transaction tx =
        transaction("2", 5);

    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1
    );

    const core::BlockValidationResult result =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            blockchain,
            block,
            economicContext(104, 1, 5)
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::BlockValidationStatus::INVALID_TRANSACTION &&
        result.reason().find("insufficient") != std::string::npos,
        "Candidate block with insufficient sender balance should be rejected."
    );
}

void testRejectsCandidateWithInvalidNonce() {
    const core::Blockchain blockchain =
        chain();

    const core::Transaction tx =
        transaction("3", 5);

    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1
    );

    const core::BlockValidationResult result =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            blockchain,
            block,
            economicContext(1000, 1, 5)
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::BlockValidationStatus::INVALID_TRANSACTION &&
        result.reason().find("nonce") != std::string::npos,
        "Candidate block with invalid sender nonce should be rejected."
    );
}

void testRejectsWrongPreviousHash() {
    const core::Blockchain blockchain =
        chain();

    const core::Transaction tx =
        transaction("2");

    const core::Block block(
        1,
        "wrong-previous-hash",
        {record(tx)},
        kTimestamp + 1
    );

    const core::BlockValidationResult result =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            blockchain,
            block
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::BlockValidationStatus::INVALID_PREVIOUS_HASH,
        "Candidate block with wrong previous hash should be rejected."
    );
}

void testRejectsDuplicateLedgerSource() {
    const core::Blockchain blockchain =
        chain();

    const core::Transaction tx =
        transaction("2");

    const core::LedgerRecord ledgerRecord =
        record(tx);

    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {ledgerRecord, ledgerRecord},
        kTimestamp + 1
    );

    const core::BlockValidationResult result =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            blockchain,
            block
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::BlockValidationStatus::DUPLICATE_LEDGER_SOURCE,
        "Candidate block with duplicate ledger source ids should be rejected."
    );
}

void testRejectsTransactionBelowMinimumFee() {
    const core::Blockchain blockchain =
        chain();

    const core::Transaction tx =
        transaction("2", 4);

    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1
    );

    const core::BlockValidationResult result =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            blockchain,
            block,
            5
        );

    requireCondition(
        !result.accepted() &&
        result.status() == core::BlockValidationStatus::INVALID_TRANSACTION &&
        result.reason().find("minimum fee") != std::string::npos,
        "Candidate block with transaction below minimum fee should be rejected."
    );
}

void testAcceptsTransactionAtMinimumFee() {
    const core::Blockchain blockchain =
        chain();

    const core::Transaction tx =
        transaction("2", 5);

    const core::Block block(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1
    );

    const core::BlockValidationResult result =
        core::BlockStateTransitionValidator::validateCandidateBlock(
            blockchain,
            block,
            5
        );

    requireCondition(
        result.accepted(),
        "Candidate block with transaction at minimum fee should be accepted."
    );
}

} // namespace

int main() {
    try {
        testAcceptsAppendableCandidate();
        testAcceptsCandidateWithValidEconomicState();
        testRejectsCandidateWithInsufficientBalance();
        testRejectsCandidateWithInvalidNonce();
        testRejectsWrongPreviousHash();
        testRejectsDuplicateLedgerSource();
        testRejectsTransactionBelowMinimumFee();
        testAcceptsTransactionAtMinimumFee();

        std::cout << "Nodo block state transition validator tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo block state transition validator tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
