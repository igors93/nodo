#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/ChainStateRebuilder.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;
using nodo::core::ChainStateRebuilder;
using nodo::core::StateRebuildReport;

constexpr std::int64_t kTimestamp = 1900000200;

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

core::Transaction makeTransfer(
    const std::string& from,
    const std::string& to,
    std::uint64_t nonce,
    std::int64_t feeRaw = 1000
) {
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicEd25519KeyPair("rebuilder-test-" + from);
    const crypto::Ed25519SignatureProvider provider;

    core::Transaction tx(
        core::TransactionType::TRANSFER,
        from,
        to,
        utils::Amount::fromRawUnits(50000),
        utils::Amount::fromRawUnits(feeRaw),
        nonce,
        kTimestamp
    );

    tx.attachSignatureBundle(
        crypto::SignatureBundle::createSignature(
            tx.signingPayload(),
            kp.publicKey(),
            kp.privateKeyForSigningOnly(),
            kTimestamp,
            provider,
            crypto::SigningDomain::USER_TRANSACTION
        )
    );

    return tx;
}

core::LedgerRecord makeRecord(const core::Transaction& tx) {
    return core::LedgerRecord::fromTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        kTimestamp
    );
}

core::StateTransitionPreviewContext contextFor(
    const std::string& sender,
    std::uint64_t nonce = 0,
    std::int64_t minFee = 1000
) {
    core::AccountStateView view;
    view.putAccount(core::AccountState(
        sender,
        utils::Amount::fromRawUnits(100000000),
        nonce
    ));
    return core::StateTransitionPreviewContext(minFee, view, false, true);
}

// Build a block with correct stateRoot/receiptsRoot using StateTransitionPreview.
core::Block blockWithRealRoots(
    const core::Blockchain& chain,
    const core::Transaction& tx,
    const core::StateTransitionPreviewContext& ctx,
    std::uint64_t height,
    std::int64_t ts
) {
    const core::Block draft(
        height,
        chain.latestBlock().hash(),
        {makeRecord(tx)},
        ts,
        "",
        ""
    );

    const core::StateTransitionPreviewResult preview =
        core::StateTransitionPreview::previewBlock(draft, ctx);

    if (!preview.accepted()) {
        throw std::runtime_error(
            "blockWithRealRoots: preview failed: " + preview.reason()
        );
    }

    return core::Block(
        height,
        chain.latestBlock().hash(),
        {makeRecord(tx)},
        ts,
        preview.stateRoot(),
        preview.receiptsRoot()
    );
}

// Build a minimal valid genesis LedgerRecord (blocks require at least one record).
core::LedgerRecord genesisRecord() {
    return makeRecord(makeTransfer("genesis-issuer", "alice", 1));
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testGenesisOnlyChainPassesVerification() {
    core::Blockchain chain;
    chain.addGenesisBlock(core::Block::createGenesisBlock({genesisRecord()}, kTimestamp));

    const StateRebuildReport report = ChainStateRebuilder::rebuildAndVerifyViaEngine(
        chain,
        [](const core::Blockchain&) { return contextFor("nobody"); }
    );

    requireCondition(
        report.success(),
        "Genesis-only chain must pass: " + report.failureReason()
    );
    requireCondition(
        report.commitmentVerificationPassed(),
        "Genesis-only chain commitment verification must pass."
    );
    requireCondition(
        report.blockCount() == 1,
        "Genesis-only chain should have blockCount=1."
    );
}

void testTwoBlockChainPassesVerification() {
    core::Blockchain chain;
    chain.addGenesisBlock(core::Block::createGenesisBlock({genesisRecord()}, kTimestamp));

    const auto ctx = contextFor("alice", 0, 1000);
    const auto tx = makeTransfer("alice", "bob", 1);
    const auto block1 = blockWithRealRoots(chain, tx, ctx, 1, kTimestamp + 1);
    chain.addBlock(block1);

    const StateRebuildReport report = ChainStateRebuilder::rebuildAndVerifyViaEngine(
        chain,
        [](const core::Blockchain&) { return contextFor("alice", 0, 1000); }
    );

    requireCondition(
        report.success(),
        "Two-block chain with valid commitments must pass: " + report.failureReason()
    );
    requireCondition(
        report.commitmentVerificationPassed(),
        "Two-block chain commitment verification must pass."
    );
    requireCondition(
        report.blockCount() == 2,
        "Two-block chain should have blockCount=2."
    );
}

void testBlockWithTamperedStateRootFails() {
    core::Blockchain chain;
    chain.addGenesisBlock(core::Block::createGenesisBlock({genesisRecord()}, kTimestamp));

    const auto ctx = contextFor("alice", 0, 1000);
    const auto tx = makeTransfer("alice", "bob", 1);
    const auto block1 = blockWithRealRoots(chain, tx, ctx, 1, kTimestamp + 1);

    // Replace stateRoot with a garbage value.
    const core::Block tamperedBlock1(
        block1.index(),
        block1.previousHash(),
        block1.records(),
        block1.timestamp(),
        "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef",
        block1.receiptsRoot()
    );

    core::Blockchain tamperedChain;
    tamperedChain.addGenesisBlock(chain.blocks()[0]);
    tamperedChain.addBlock(tamperedBlock1);

    const StateRebuildReport report = ChainStateRebuilder::rebuildAndVerifyViaEngine(
        tamperedChain,
        [](const core::Blockchain&) { return contextFor("alice", 0, 1000); }
    );

    requireCondition(
        !report.success(),
        "Tampered stateRoot must cause rebuildAndVerifyViaEngine to fail."
    );
    requireCondition(
        !report.commitmentVerificationPassed(),
        "commitmentVerificationPassed must be false for tampered chain."
    );
    requireCondition(
        report.firstFailedCommitmentHeight() == 1,
        "firstFailedCommitmentHeight must be 1 for tampered block 1."
    );
    requireCondition(
        !report.commitmentFailureReason().empty(),
        "commitmentFailureReason must be non-empty."
    );
}

void testBlockWithTamperedReceiptsRootFails() {
    core::Blockchain chain;
    chain.addGenesisBlock(core::Block::createGenesisBlock({genesisRecord()}, kTimestamp));

    const auto ctx = contextFor("alice", 0, 1000);
    const auto tx = makeTransfer("alice", "bob", 1);
    const auto block1 = blockWithRealRoots(chain, tx, ctx, 1, kTimestamp + 1);

    const core::Block tamperedBlock1(
        block1.index(),
        block1.previousHash(),
        block1.records(),
        block1.timestamp(),
        block1.stateRoot(),
        "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef"
    );

    core::Blockchain tamperedChain;
    tamperedChain.addGenesisBlock(chain.blocks()[0]);
    tamperedChain.addBlock(tamperedBlock1);

    const StateRebuildReport report = ChainStateRebuilder::rebuildAndVerifyViaEngine(
        tamperedChain,
        [](const core::Blockchain&) { return contextFor("alice", 0, 1000); }
    );

    requireCondition(
        !report.success(),
        "Tampered receiptsRoot must cause rebuildAndVerifyViaEngine to fail."
    );
    requireCondition(
        !report.commitmentVerificationPassed(),
        "commitmentVerificationPassed must be false for tampered receipts root."
    );
    requireCondition(
        report.firstFailedCommitmentHeight() == 1,
        "firstFailedCommitmentHeight must be 1."
    );
}

void testReportSerializesNewFields() {
    core::Blockchain chain;
    chain.addGenesisBlock(core::Block::createGenesisBlock({genesisRecord()}, kTimestamp));

    const StateRebuildReport report = ChainStateRebuilder::rebuildAndVerifyViaEngine(
        chain,
        [](const core::Blockchain&) { return contextFor("nobody"); }
    );

    const std::string serialized = report.serialize();

    requireCondition(
        serialized.find("commitmentVerificationPassed=true") != std::string::npos,
        "Serialized report must contain commitmentVerificationPassed=true for genesis chain."
    );
    requireCondition(
        serialized.find("firstFailedCommitmentHeight=0") != std::string::npos,
        "Serialized report must contain firstFailedCommitmentHeight=0."
    );
}

void testAuditBlockchainDoesNotSetCommitmentFields() {
    core::Blockchain chain;
    chain.addGenesisBlock(core::Block::createGenesisBlock({genesisRecord()}, kTimestamp));

    // auditBlockchain does NOT call the engine — commitment fields should be defaults.
    const StateRebuildReport report = ChainStateRebuilder::auditBlockchain(chain);

    requireCondition(
        report.success(),
        "auditBlockchain should succeed on a valid genesis chain."
    );
    requireCondition(
        !report.commitmentVerificationPassed(),
        "auditBlockchain must NOT set commitmentVerificationPassed (no engine call)."
    );
}

} // namespace

int main() {
    try {
        testGenesisOnlyChainPassesVerification();
        std::cout << "PASS testGenesisOnlyChainPassesVerification" << std::endl;

        testTwoBlockChainPassesVerification();
        std::cout << "PASS testTwoBlockChainPassesVerification" << std::endl;

        testBlockWithTamperedStateRootFails();
        std::cout << "PASS testBlockWithTamperedStateRootFails" << std::endl;

        testBlockWithTamperedReceiptsRootFails();
        std::cout << "PASS testBlockWithTamperedReceiptsRootFails" << std::endl;

        testReportSerializesNewFields();
        std::cout << "PASS testReportSerializesNewFields" << std::endl;

        testAuditBlockchainDoesNotSetCommitmentFields();
        std::cout << "PASS testAuditBlockchainDoesNotSetCommitmentFields" << std::endl;

        std::cout << "All ChainStateRebuilderVerificationTests passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
