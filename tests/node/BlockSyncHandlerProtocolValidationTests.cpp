#include "node/BlockSyncHandler.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
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
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "p2p/Peer.hpp"
#include "utils/Amount.hpp"
#include "crypto/AddressDerivation.hpp"
#include "core/TransactionExecutionContext.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;

class TestProtocolDomainExecutor final : public core::TransactionDomainExecutor {
public:
    core::TransactionDomainExecutionResult applyBurn(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyStakeDeposit(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyStakeUnlock(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyStakeWithdraw(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyStakeTopUp(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyValidatorRegister(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyValidatorExitRequest(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyValidatorUnjailRequest(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyGovernanceProposal(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult applyGovernanceVote(
        const core::Transaction&, const core::AccountStateView& accounts,
        std::uint64_t, std::int64_t
    ) override { return accepted(accounts); }

    core::TransactionDomainExecutionResult finalizeBlock(
        const core::AccountStateView& accounts,
        utils::Amount,
        const std::vector<core::LedgerRecord>&,
        std::uint64_t,
        std::int64_t
    ) override {
        return accepted(accounts);
    }

    const std::map<std::string, std::string>& domains() const override {
        return m_domains;
    }

private:
    std::map<std::string, std::string> m_domains{{"test_domain", "stable"}};

    core::TransactionDomainExecutionResult accepted(
        const core::AccountStateView& accounts
    ) {
        return core::TransactionDomainExecutionResult::accepted(accounts, m_domains);
    }
};

static const consensus::BlockFinalizationRegistry kEmptyRegistry;

// Canonical 64-char hex strings with wrong economic values.
static const std::string kWrongStateRoot =
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
static const std::string kWrongReceiptsRoot =
    "cafebabecafebabecafebabecafebabecafebabecafebabecafebabecafebabe";

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static std::string getSyncSenderAddress() {
    const crypto::KeyPair kp = crypto::KeyPair::createDeterministicEd25519KeyPair("sync-key");
    return crypto::AddressDerivation::deriveFromPublicKey(kp.publicKey()).value();
}

core::Transaction testTx(std::uint64_t nonce) {
    const std::string senderAddress = getSyncSenderAddress();
    core::Transaction tx(
        core::TransactionType::TRANSFER,
        senderAddress,
        "sync-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(10),
        nonce,
        kTimestamp
    );
    tx.withChainId("test-chain");
    const crypto::KeyPair kp = crypto::KeyPair::createDeterministicEd25519KeyPair("sync-key");
    const crypto::Ed25519SignatureProvider provider;
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

core::LedgerRecord record(const core::Transaction& tx) {
    return core::LedgerRecord::fromTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        kTimestamp
    );
}

core::Blockchain chainWithGenesis() {
    core::Blockchain blockchain;
    blockchain.addGenesisBlock(
        core::Block::createGenesisBlock({record(testTx(1))}, kTimestamp)
    );
    return blockchain;
}

core::StateTransitionPreviewContext senderContext() {
    core::AccountStateView view;
    view.putAccount(core::AccountState(
        getSyncSenderAddress(),
        utils::Amount::fromRawUnits(1000),
        0
    ));
    return core::StateTransitionPreviewContext(
        10,
        view,
        false,
        true,
        "",
        0,
        "test-chain",
        "localnet",
        {},
        {},
        []() {
            return std::make_unique<TestProtocolDomainExecutor>();
        },
        true
    );
}

core::Block blockWithRealRoots(const core::Blockchain& blockchain, std::uint64_t nonce) {
    const core::Transaction tx = testTx(nonce);
    const core::Block draft(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1,
        "",
        ""
    );
    const core::StateTransitionPreviewResult preview =
        core::StateTransitionPreview::previewBlock(draft, senderContext());
    if (!preview.accepted()) {
        throw std::runtime_error("blockWithRealRoots: preview failed: " + preview.reason());
    }
    return core::Block(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1,
        preview.stateRoot(),
        preview.receiptsRoot()
    );
}

struct GossipPair {
    std::shared_ptr<p2p::LoopbackTransportBus> bus;
    p2p::LoopbackTransport transportA;
    p2p::LoopbackTransport transportB;
    p2p::GossipMesh meshA;
    p2p::GossipMesh meshB;

    GossipPair()
        : bus(std::make_shared<p2p::LoopbackTransportBus>())
        , transportA(bus)
        , transportB(bus)
        , meshA(p2p::GossipMeshConfig("node-a", "localnet", "chain-sync-test", "1", "genesis-id-sync", 60, 5), transportA)
        , meshB(p2p::GossipMeshConfig("node-b", "localnet", "chain-sync-test", "1", "genesis-id-sync", 60, 5), transportB)
    {
        meshA.registerPeer(p2p::PeerMetadata("node-b", p2p::PeerEndpoint("127.0.0.1", 19100), "fp-b", kTimestamp, kTimestamp, 0, false));
        meshB.registerPeer(p2p::PeerMetadata("node-a", p2p::PeerEndpoint("127.0.0.1", 19101), "fp-a", kTimestamp, kTimestamp, 0, false));
        meshA.connectPeer("node-b");
        meshB.connectPeer("node-a");
    }

    // Broadcast a BLOCK_RESPONSE from node-a and deliver it to node-b's inbox.
    void deliverBlockResponse(const std::vector<core::Block>& blocks) {
        const std::string payload = node::BlockSyncHandler::serializeBlockList(blocks);
        meshA.broadcast(p2p::NetworkMessageType::BLOCK_RESPONSE, payload, kTimestamp);
        meshA.flushOutbound(kTimestamp);
        meshB.receiveAvailable(kTimestamp + 1);
    }
};

// Context builder that always returns the same static sender context.
core::StateTransitionPreviewContext buildContext(const core::Blockchain& /*chain*/) {
    return senderContext();
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testApplyResponsesRejectsBlockWithWrongStateRoot() {
    core::Blockchain blockchain = chainWithGenesis();
    GossipPair gp;

    const core::Block badBlock(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(2))},
        kTimestamp + 1,
        kWrongStateRoot,
        kWrongReceiptsRoot
    );

    gp.deliverBlockResponse({badBlock});

    const std::size_t applied = node::BlockSyncHandler::applyResponses(
        gp.meshB, blockchain, buildContext,
        kEmptyRegistry, node::BlockSyncQcMode::STATE_ROOT_ONLY,
        kTimestamp + 2
    );

    requireCondition(
        applied == 0,
        "No blocks must be applied when state root is wrong."
    );
    requireCondition(
        blockchain.size() == 1U,
        "Blockchain must not grow after wrong-root block rejection."
    );
}

void testApplyResponsesRejectsBlockWithWrongReceiptsRoot() {
    core::Blockchain blockchain = chainWithGenesis();
    GossipPair gp;

    const core::Transaction tx = testTx(1);
    const core::Block draft(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1,
        "",
        ""
    );
    const core::StateTransitionPreviewResult preview =
        core::StateTransitionPreview::previewBlock(draft, senderContext());

    // Correct stateRoot but wrong receiptsRoot.
    const core::Block badBlock(
        1,
        blockchain.latestBlock().hash(),
        {record(tx)},
        kTimestamp + 1,
        preview.stateRoot(),
        kWrongReceiptsRoot
    );

    gp.deliverBlockResponse({badBlock});

    const std::size_t applied = node::BlockSyncHandler::applyResponses(
        gp.meshB, blockchain, buildContext,
        kEmptyRegistry, node::BlockSyncQcMode::STATE_ROOT_ONLY,
        kTimestamp + 2
    );

    requireCondition(
        applied == 0,
        "No blocks must be applied when receipts root is wrong."
    );
    requireCondition(
        blockchain.size() == 1U,
        "Blockchain must not grow after wrong-receipts-root block rejection."
    );
}

void testApplyResponsesAcceptsBlockWithCorrectRoots() {
    core::Blockchain blockchain = chainWithGenesis();
    GossipPair gp;

    const core::Block goodBlock = blockWithRealRoots(blockchain, 1);

    gp.deliverBlockResponse({goodBlock});

    const std::size_t applied = node::BlockSyncHandler::applyResponses(
        gp.meshB, blockchain, buildContext,
        kEmptyRegistry, node::BlockSyncQcMode::STATE_ROOT_ONLY,
        kTimestamp + 2
    );

    requireCondition(
        applied == 1,
        "Block with correct roots must be applied."
    );
    requireCondition(
        blockchain.size() == 2U,
        "Blockchain must grow by one after correct-root block sync."
    );
}

void testApplyResponsesStopsAtFirstInvalidBlock() {
    core::Blockchain blockchain = chainWithGenesis();
    GossipPair gp;

    // First block is valid (will be applied), second has wrong roots (must stop).
    const core::Block goodBlock = blockWithRealRoots(blockchain, 1);
    const core::Block badBlock(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(3))},
        kTimestamp + 2,
        kWrongStateRoot,
        kWrongReceiptsRoot
    );

    gp.deliverBlockResponse({badBlock, goodBlock});

    const std::size_t applied = node::BlockSyncHandler::applyResponses(
        gp.meshB, blockchain, buildContext,
        kEmptyRegistry, node::BlockSyncQcMode::STATE_ROOT_ONLY,
        kTimestamp + 3
    );

    // First block in the batch has wrong roots → stops at 0 applied.
    requireCondition(
        applied == 0,
        "Sync must stop at the first invalid block without applying later blocks."
    );
}

void testApplyResponsesRejectsNonCanonicalRoots() {
    core::Blockchain blockchain = chainWithGenesis();
    GossipPair gp;

    const core::Block badBlock(
        1,
        blockchain.latestBlock().hash(),
        {record(testTx(2))},
        kTimestamp + 1,
        "DRAFT_STATE_ROOT",
        "DRAFT_RECEIPTS_ROOT"
    );

    gp.deliverBlockResponse({badBlock});

    const std::size_t applied = node::BlockSyncHandler::applyResponses(
        gp.meshB, blockchain, buildContext,
        kEmptyRegistry, node::BlockSyncQcMode::STATE_ROOT_ONLY,
        kTimestamp + 2
    );

    requireCondition(
        applied == 0,
        "Block with non-canonical roots must not be applied."
    );
}

} // namespace

int main() {
    try {
        testApplyResponsesRejectsBlockWithWrongStateRoot();
        testApplyResponsesRejectsBlockWithWrongReceiptsRoot();
        testApplyResponsesAcceptsBlockWithCorrectRoots();
        testApplyResponsesStopsAtFirstInvalidBlock();
        testApplyResponsesRejectsNonCanonicalRoots();

        std::cout << "BlockSyncHandler protocol validation tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
