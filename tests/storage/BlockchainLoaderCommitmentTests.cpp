#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/BlockStateTransitionValidator.hpp"
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
#include "storage/BlockchainLoader.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;
using nodo::storage::BlockchainCommitmentVerificationReport;
using nodo::storage::BlockchainLoader;

constexpr std::int64_t kTimestamp = 1900000100;

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
    const crypto::KeyPair keyPair =
        crypto::KeyPair::createDeterministicEd25519KeyPair("commitment-test-key-" + from);
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
            keyPair.publicKey(),
            keyPair.privateKeyForSigningOnly(),
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

// Build a StateTransitionPreviewContext that gives `sender` a large balance.
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

// Build a minimal valid genesis LedgerRecord (required: blocks need >= 1 record).
core::LedgerRecord genesisRecord() {
    return makeRecord(makeTransfer("genesis-issuer", "alice", 1));
}

// ---------------------------------------------------------------------------
// Helper: build a genesis chain + one fully committed block
// ---------------------------------------------------------------------------

struct TwoBlockFixture {
    core::Blockchain blockchain;
    core::StateTransitionPreviewContext block1Context;
};

TwoBlockFixture buildTwoBlockChain() {
    core::Blockchain blockchain;
    blockchain.addGenesisBlock(
        core::Block::createGenesisBlock({genesisRecord()}, kTimestamp)
    );

    const core::Transaction tx = makeTransfer("alice", "bob", 1);
    const auto ctx = contextFor("alice", 0, 1000);

    const core::Block block1 = blockWithRealRoots(
        blockchain, tx, ctx, 1, kTimestamp + 1
    );

    blockchain.addBlock(block1);

    TwoBlockFixture f;
    f.blockchain = blockchain;
    f.block1Context = ctx;
    return f;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testPassesForGenesisOnlyChain() {
    core::Blockchain blockchain;
    blockchain.addGenesisBlock(
        core::Block::createGenesisBlock({genesisRecord()}, kTimestamp)
    );

    const auto report = BlockchainLoader::verifyChainCommitmentsViaEngine(
        blockchain,
        [](const core::Blockchain&) {
            return contextFor("nobody");
        }
    );

    requireCondition(
        report.commitmentsPassed(),
        "Genesis-only chain must pass commitment verification: " + report.reason()
    );
    requireCondition(
        report.verifiedBlockCount() == 1,
        "Genesis-only chain should report 1 verified block."
    );
}

void testPassesForValidTwoBlockChain() {
    const TwoBlockFixture f = buildTwoBlockChain();

    const auto report = BlockchainLoader::verifyChainCommitmentsViaEngine(
        f.blockchain,
        [](const core::Blockchain&) {
            return contextFor("alice", 0, 1000);
        }
    );

    requireCondition(
        report.commitmentsPassed(),
        "Two-block chain with valid commitments must pass: " + report.reason()
    );
    requireCondition(
        report.verifiedBlockCount() == 2,
        "Two-block chain should report 2 verified blocks."
    );
}

void testFailsForBlockWithTamperedStateRoot() {
    const TwoBlockFixture f = buildTwoBlockChain();

    // Inject a chain where block 1 has a wrong stateRoot.
    const core::Block& goodBlock1 = f.blockchain.blocks()[1];
    const core::Block tamperedBlock1(
        goodBlock1.index(),
        goodBlock1.previousHash(),
        goodBlock1.records(),
        goodBlock1.timestamp(),
        "0000000000000000000000000000000000000000000000000000000000000000",
        goodBlock1.receiptsRoot()
    );

    core::Blockchain tamperedChain;
    tamperedChain.addGenesisBlock(f.blockchain.blocks()[0]);
    tamperedChain.addBlock(tamperedBlock1);

    const auto report = BlockchainLoader::verifyChainCommitmentsViaEngine(
        tamperedChain,
        [](const core::Blockchain&) {
            return contextFor("alice", 0, 1000);
        }
    );

    requireCondition(
        !report.commitmentsPassed(),
        "Tampered stateRoot must fail commitment verification."
    );
    requireCondition(
        report.firstFailedHeight() == 1,
        "First failed height must be 1 for tampered block 1."
    );
    requireCondition(
        !report.reason().empty(),
        "Failure reason must be non-empty."
    );
}

void testFailsForBlockWithTamperedReceiptsRoot() {
    const TwoBlockFixture f = buildTwoBlockChain();

    const core::Block& goodBlock1 = f.blockchain.blocks()[1];
    const core::Block tamperedBlock1(
        goodBlock1.index(),
        goodBlock1.previousHash(),
        goodBlock1.records(),
        goodBlock1.timestamp(),
        goodBlock1.stateRoot(),
        "0000000000000000000000000000000000000000000000000000000000000000"
    );

    core::Blockchain tamperedChain;
    tamperedChain.addGenesisBlock(f.blockchain.blocks()[0]);
    tamperedChain.addBlock(tamperedBlock1);

    const auto report = BlockchainLoader::verifyChainCommitmentsViaEngine(
        tamperedChain,
        [](const core::Blockchain&) {
            return contextFor("alice", 0, 1000);
        }
    );

    requireCondition(
        !report.commitmentsPassed(),
        "Tampered receiptsRoot must fail commitment verification."
    );
    requireCondition(
        report.firstFailedHeight() == 1,
        "First failed height must be 1 for tampered block 1."
    );
}

void testReportSerializesCorrectly() {
    const auto success = BlockchainCommitmentVerificationReport::passed(5);
    const std::string serialized = success.serialize();

    requireCondition(
        serialized.find("passed=true") != std::string::npos,
        "Serialized passed report must contain 'passed=true'."
    );
    requireCondition(
        serialized.find("verifiedBlockCount=5") != std::string::npos,
        "Serialized passed report must contain 'verifiedBlockCount=5'."
    );

    const auto failure = BlockchainCommitmentVerificationReport::failed(
        3, "stateRoot mismatch at height 3"
    );
    const std::string failSerialized = failure.serialize();

    requireCondition(
        failSerialized.find("passed=false") != std::string::npos,
        "Serialized failed report must contain 'passed=false'."
    );
    requireCondition(
        failSerialized.find("firstFailedHeight=3") != std::string::npos,
        "Serialized failed report must contain 'firstFailedHeight=3'."
    );
}

} // namespace

int main() {
    try {
        testPassesForGenesisOnlyChain();
        std::cout << "PASS testPassesForGenesisOnlyChain" << std::endl;

        testPassesForValidTwoBlockChain();
        std::cout << "PASS testPassesForValidTwoBlockChain" << std::endl;

        testFailsForBlockWithTamperedStateRoot();
        std::cout << "PASS testFailsForBlockWithTamperedStateRoot" << std::endl;

        testFailsForBlockWithTamperedReceiptsRoot();
        std::cout << "PASS testFailsForBlockWithTamperedReceiptsRoot" << std::endl;

        testReportSerializesCorrectly();
        std::cout << "PASS testReportSerializesCorrectly" << std::endl;

        std::cout << "All BlockchainLoaderCommitmentTests passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
