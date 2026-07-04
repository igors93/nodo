#include "node/BlockSyncHandler.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "p2p/Peer.hpp"
#include "utils/Amount.hpp"
#include "core/TransactionExecutionContext.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;
constexpr std::uint64_t kRound = 1;

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

void requireCondition(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

static std::string getQcSenderAddress() {
    const crypto::KeyPair kp = crypto::KeyPair::createDeterministicEd25519KeyPair("qc-test-key");
    return crypto::AddressDerivation::deriveFromPublicKey(kp.publicKey()).value();
}

// Minimal signed transaction for building ledger records.
core::Transaction testTx(std::uint64_t nonce) {
    const std::string senderAddress = getQcSenderAddress();
    core::Transaction tx(
        core::TransactionType::TRANSFER,
        senderAddress,
        "qc-recipient",
        utils::Amount::fromRawUnits(100),
        utils::Amount::fromRawUnits(10),
        nonce,
        kTimestamp
    );
    tx.withChainId("test-chain");
    const crypto::KeyPair kp = crypto::KeyPair::createDeterministicEd25519KeyPair("qc-test-key");
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

core::LedgerRecord txRecord(const core::Transaction& tx) {
    return core::LedgerRecord::fromTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        kTimestamp
    );
}

core::AccountStateView senderView() {
    core::AccountStateView view;
    view.putAccount(core::AccountState(
        getQcSenderAddress(),
        utils::Amount::fromRawUnits(1000),
        0
    ));
    return view;
}

core::StateTransitionPreviewContext buildContext(const core::Blockchain&) {
    return core::StateTransitionPreviewContext(
        10,
        senderView(),
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

core::Blockchain chainWithGenesis() {
    core::Blockchain blockchain;
    blockchain.addGenesisBlock(
        core::Block::createGenesisBlock({txRecord(testTx(1))}, kTimestamp)
    );
    return blockchain;
}

// Build a valid block (correct stateRoot + receiptsRoot) for the given chain.
core::Block validBlockFor(const core::Blockchain& blockchain, std::uint64_t nonce) {
    const core::Transaction tx = testTx(nonce);
    const core::Block draft(
        blockchain.latestBlock().index() + 1,
        blockchain.latestBlock().hash(),
        {txRecord(tx)},
        kTimestamp + 1,
        "",
        ""
    );
    const core::StateTransitionPreviewContext ctx = buildContext(blockchain);
    const core::StateTransitionPreviewResult preview =
        core::StateTransitionPreview::previewBlock(draft, ctx);
    if (!preview.accepted()) {
        throw std::runtime_error("validBlockFor: preview failed: " + preview.reason());
    }
    return core::Block(
        blockchain.latestBlock().index() + 1,
        blockchain.latestBlock().hash(),
        {txRecord(tx)},
        kTimestamp + 1,
        preview.stateRoot(),
        preview.receiptsRoot()
    );
}

// Register a BLS validator in the registry and return its address.
std::string registerValidator(
    core::ValidatorRegistry& registry,
    const crypto::KeyPair& kp,
    const std::string& seed
) {
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(kp.publicKey()).value();
    core::ValidatorRegistrationRecord rec(
        address, kp.publicKey(), /*activationEpoch=*/1, "meta-" + seed, kTimestamp
    );
    requireCondition(
        registry.registerValidator(rec).accepted(),
        "registerValidator failed for seed: " + seed
    );
    return address;
}

// Build a structurally valid + cryptographically valid FinalizedBlockRecord for `block`.
// Uses `validatorKp` to cast a PRECOMMIT vote, then assembles a QC and wraps it.
consensus::FinalizedBlockRecord buildFinalizedRecord(
    const core::Block& block,
    const crypto::KeyPair& validatorKp,
    const core::ValidatorRegistry& registry
) {
    const crypto::Bls12381SignatureProvider blsProvider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(validatorKp.publicKey()).value();

    const consensus::ValidatorVoteRecord vote = consensus::ValidatorVoteRecord::createVote(
        address,
        validatorKp.publicKey(),
        validatorKp.privateKeyForSigningOnly(),
        block.index(),
        block.hash(),
        block.previousHash(),
        kRound,
        consensus::ValidatorVoteDecision::PRECOMMIT,
        "reason-" + block.hash(),
        kTimestamp,
        blsProvider
    );

    const consensus::QuorumCertificateBuildResult qcResult =
        consensus::QuorumCertificateBuilder::buildFromVotes(
            block.index(),
            block.hash(),
            block.previousHash(),
            kRound,
            {vote},
            registry,
            policy,
            blsProvider
        );

    requireCondition(qcResult.certified(), "QC build failed: " + qcResult.reason());

    return consensus::FinalizedBlockRecord(
        block.index(),
        block.hash(),
        block.previousHash(),
        kRound,
        kTimestamp,
        qcResult.certificate()
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
        , meshA(p2p::GossipMeshConfig("node-a", "localnet", "qc-test", "1", "genesis-id-qc", 60, 5, 100, 50), transportA)
        , meshB(p2p::GossipMeshConfig("node-b", "localnet", "qc-test", "1", "genesis-id-qc", 60, 5, 100, 50), transportB)
    {
        meshA.registerPeer(p2p::PeerMetadata("node-b", p2p::PeerEndpoint("127.0.0.1", 19200), "fp-b", kTimestamp, kTimestamp, 0, false));
        meshB.registerPeer(p2p::PeerMetadata("node-a", p2p::PeerEndpoint("127.0.0.1", 19201), "fp-a", kTimestamp, kTimestamp, 0, false));
        meshA.connectPeer("node-b");
        meshB.connectPeer("node-a");
    }

    void deliverBlockResponse(const std::vector<core::Block>& blocks) {
        const std::string payload = node::BlockSyncHandler::serializeBlockList(blocks);
        meshA.broadcast(p2p::NetworkMessageType::BLOCK_RESPONSE, payload, kTimestamp);
        meshA.flushOutbound(kTimestamp);
        meshB.receiveAvailable(kTimestamp + 1);
    }
};

// ── 1. STATE_ROOT_ONLY: accepts block without QC in registry ─────────────────

void testStateRootOnlyAcceptsWithoutQc() {
    core::Blockchain blockchain = chainWithGenesis();
    GossipPair gp;

    const core::Block block = validBlockFor(blockchain, 1);
    gp.deliverBlockResponse({block});

    consensus::BlockFinalizationRegistry emptyRegistry;
    const std::size_t applied = node::BlockSyncHandler::applyResponses(
        gp.meshB, blockchain, buildContext,
        emptyRegistry, node::BlockSyncQcMode::STATE_ROOT_ONLY,
        kTimestamp + 2
    );

    requireCondition(applied == 1, "STATE_ROOT_ONLY must accept a block without a QC in registry.");
    requireCondition(blockchain.size() == 2U, "Blockchain must grow by 1.");
}

// ── 2. QC_REQUIRED: rejects block not in finalization registry ───────────────

void testQcRequiredRejectsBlockWithoutFinalizationRecord() {
    core::Blockchain blockchain = chainWithGenesis();
    GossipPair gp;

    const core::Block block = validBlockFor(blockchain, 1);
    gp.deliverBlockResponse({block});

    consensus::BlockFinalizationRegistry emptyRegistry;
    const std::size_t applied = node::BlockSyncHandler::applyResponses(
        gp.meshB, blockchain, buildContext,
        emptyRegistry, node::BlockSyncQcMode::QC_REQUIRED,
        kTimestamp + 2
    );

    requireCondition(applied == 0, "QC_REQUIRED must reject a block not in the finalization registry.");
    requireCondition(blockchain.size() == 1U, "Blockchain must not grow when QC is missing.");
}

// ── 3. QC_REQUIRED: accepts block present in finalization registry ────────────

void testQcRequiredAcceptsBlockWithFinalizationRecord() {
    const crypto::KeyPair validatorKp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("qc-validator-accept");
    core::ValidatorRegistry registry;
    registerValidator(registry, validatorKp, "qc-validator-accept");

    core::Blockchain blockchain = chainWithGenesis();
    GossipPair gp;

    const core::Block block = validBlockFor(blockchain, 1);
    gp.deliverBlockResponse({block});

    consensus::BlockFinalizationRegistry finalizationRegistry;
    const auto record = buildFinalizedRecord(block, validatorKp, registry);
    requireCondition(
        finalizationRegistry.registerFinalizedBlock(record).success(),
        "Finalization record registration failed."
    );

    const std::size_t applied = node::BlockSyncHandler::applyResponses(
        gp.meshB, blockchain, buildContext,
        finalizationRegistry, node::BlockSyncQcMode::QC_REQUIRED,
        kTimestamp + 2
    );

    requireCondition(applied == 1, "QC_REQUIRED must accept block when QC record is present.");
    requireCondition(blockchain.size() == 2U, "Blockchain must grow by 1.");
}

// ── 4. QC_REQUIRED: stops at first block missing QC in a batch ───────────────

void testQcRequiredStopsAtFirstBlockMissingQc() {
    const crypto::KeyPair validatorKp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("qc-validator-batch");
    core::ValidatorRegistry registry;
    registerValidator(registry, validatorKp, "qc-validator-batch");

    core::Blockchain blockchain = chainWithGenesis();
    GossipPair gp;

    const core::Block block1 = validBlockFor(blockchain, 1);
    core::Blockchain extended = blockchain;
    extended.addBlock(block1);
    // Context always starts at nonce=0, so tx nonce=1 is valid for both blocks.
    const core::Block block2 = validBlockFor(extended, 1);

    gp.deliverBlockResponse({block1, block2});

    // Only block1 is finalized; block2 is not.
    consensus::BlockFinalizationRegistry finalizationRegistry;
    const auto record1 = buildFinalizedRecord(block1, validatorKp, registry);
    requireCondition(
        finalizationRegistry.registerFinalizedBlock(record1).success(),
        "block1 finalization record registration failed."
    );

    const std::size_t applied = node::BlockSyncHandler::applyResponses(
        gp.meshB, blockchain, buildContext,
        finalizationRegistry, node::BlockSyncQcMode::QC_REQUIRED,
        kTimestamp + 2
    );

    requireCondition(applied == 1, "QC_REQUIRED must stop after the first block whose QC is missing.");
    requireCondition(blockchain.size() == 2U, "Only the finalized block must be applied.");
}

} // namespace

int main() {
    try {
        testStateRootOnlyAcceptsWithoutQc();
        testQcRequiredRejectsBlockWithoutFinalizationRecord();
        testQcRequiredAcceptsBlockWithFinalizationRecord();
        testQcRequiredStopsAtFirstBlockMissingQc();
        std::cout << "BlockSyncHandler QC verification tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
