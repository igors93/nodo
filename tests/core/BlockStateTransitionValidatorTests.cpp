#include "core/Block.hpp"
#include "core/BlockStateTransitionValidator.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
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
    const std::string& nonce
) {
    core::Transaction tx(
        core::TransactionType::TRANSFER,
        "validator-test-sender",
        "validator-test-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(1),
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

} // namespace

int main() {
    try {
        testAcceptsAppendableCandidate();
        testRejectsWrongPreviousHash();
        testRejectsDuplicateLedgerSource();

        std::cout << "Nodo block state transition validator tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo block state transition validator tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
