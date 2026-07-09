// Real tests for the BFT lock/unlock (Proof-of-Lock) safety rule implemented
// in ConsensusEventLoop::tick() and factored out into
// ProposalJustification::permitsUnlock (see include/consensus/ProposalJustification.hpp).
//
// The rule: a validator that already PRECOMMITted (locked) on a block may
// only PREVOTE for a different block in a later round if the proposal
// carries a justification — a real QuorumCertificate proving 2/3+ voting
// weight already precommitted to that exact block in a round at least as
// recent as the lock. Without this, two conflicting blocks could each
// gather enough votes to finalize at the same height.
//
// Covers, with a real ConsensusEventLoop/NodeRuntime (no mock consensus):
//   1. The permitsUnlock rule itself, table-tested in isolation.
//   2. A locked validator does not vote for a different block with no
//      justification at all.
//   3. A locked validator rejects an invalid justification (QC that
//      certifies the wrong block).
//   4. A locked validator accepts a safe justification (QC proving quorum
//      already precommitted the new block).
//   5. Two conflicting blocks are never both finalized at the same height,
//      even when the second is independently backed by its own valid QC.

#include "../common/ConsensusPhaseTestFixtures.hpp"
#include "config/NetworkParameters.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "consensus/BlockProductionPhase.hpp"
#include "consensus/BlockProposalPhase.hpp"
#include "consensus/ConsensusEventLoop.hpp"
#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposalJustification.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/SignedBlockProposalMessage.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1901000000;

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

const crypto::CryptoPolicy &developmentPolicy() {
  static const crypto::CryptoPolicy policy =
      crypto::CryptoPolicy::developmentPolicy();
  return policy;
}

crypto::KeyPair userKey() {
  return test::consensusTestUserKey("consensus-lock-unlock-user");
}

struct ValidatorFixture {
  crypto::KeyPair keyPair;
};

std::vector<ValidatorFixture> makeValidators(std::size_t count) {
  std::vector<ValidatorFixture> validators;
  validators.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    validators.push_back({crypto::KeyPair::createDeterministicBls12381KeyPair(
        "consensus-lock-unlock-validator-" + std::to_string(index))});
  }
  return validators;
}

config::GenesisConfig
makeGenesis(const std::vector<ValidatorFixture> &validators,
            const std::string &seed) {
  std::vector<config::BootstrapValidatorConfig> bootstrap;
  bootstrap.reserve(validators.size());
  for (std::size_t index = 0; index < validators.size(); ++index) {
    bootstrap.emplace_back(validators[index].keyPair.publicKey(), 1, 1,
                           "lock-unlock-validator-" + std::to_string(index));
  }
  return config::GenesisConfig(
      config::NetworkParameters::developmentLocal(), kTimestamp, bootstrap,
      {test::fundedConsensusTestAccount(userKey())}, seed);
}

node::NodeRuntime startRuntime(const config::GenesisConfig &genesis) {
  const p2p::PeerInfo peer("lock-unlock-test-node", "127.0.0.1:29994",
                           "nodo/test", 0, kTimestamp);
  const auto result = node::NodeRuntimeFactory::startFromGenesis(
      node::NodeRuntimeConfig(genesis, peer, 16));
  require(result.started(), "Runtime start failed: " + result.reason());
  return result.runtime();
}

p2p::PeerMetadata peerMetadata(const std::string &nodeId, std::uint16_t port) {
  return p2p::PeerMetadata(nodeId, p2p::PeerEndpoint("127.0.0.1", port),
                           nodeId + "-identity", kTimestamp, kTimestamp, 0,
                           false);
}

class TestNetwork {
private:
  std::shared_ptr<p2p::LoopbackTransportBus> m_bus;
  p2p::LoopbackTransport m_transport;

public:
  explicit TestNetwork(const config::GenesisConfig &genesis)
      : m_bus(std::make_shared<p2p::LoopbackTransportBus>()),
        m_transport(m_bus),
        mesh(p2p::GossipMeshConfig("lock-unlock-test-node",
                                   genesis.networkParameters().networkName(),
                                   genesis.networkParameters().chainId(),
                                   "nodo/test", genesis.deterministicId(), 60,
                                   3, 100, 50),
             m_transport) {}

  p2p::GossipMesh mesh;
  p2p::GossipInbox validatedInbox;

  void routeMessages() {
    for (const auto &msg : mesh.drainAllInbox()) {
      validatedInbox.add(msg);
    }
  }
};

void configureProducer(consensus::ConsensusEventLoop &loop,
                       const crypto::Signer &signer) {
  loop.setLocalValidatorAddress(signer.address());
  loop.setLocalSigner(&signer);
}

std::size_t indexOfValidator(const std::vector<ValidatorFixture> &validators,
                             const std::string &address) {
  for (std::size_t index = 0; index < validators.size(); ++index) {
    if (validators[index].keyPair.address().value() == address) {
      return index;
    }
  }
  throw std::runtime_error("Scheduled validator key was not found: " + address);
}

consensus::ValidatorVoteRecord
voteFor(const ValidatorFixture &validator, const core::Block &block,
       std::uint64_t round, consensus::ValidatorVoteDecision decision,
       std::int64_t timestamp, const crypto::Bls12381SignatureProvider &provider) {
  return consensus::ValidatorVoteRecord::createVote(
      validator.keyPair.address().value(), validator.keyPair.publicKey(),
      validator.keyPair.privateKeyForSigningOnly(), block.index(), block.hash(),
      block.previousHash(), round, decision, "nil", timestamp, provider);
}

// A vote that *claims* to be `validator` (correct, self-consistent address +
// public key) but whose signature bytes were genuinely produced by a
// different signer over a different payload, then relabeled.
// Bls12381SignatureProvider refuses to sign with a mismatched public/private
// keypair, so this is the realistic forgery shape: a real signature,
// reattributed. Only cryptographic verification (QuorumCertificate::verify,
// via ProposalJustification::permitsUnlock) can catch it.
consensus::ValidatorVoteRecord
forgedVoteFor(const ValidatorFixture &validator, const core::Block &block,
             std::uint64_t round, std::int64_t timestamp,
             const crypto::Bls12381SignatureProvider &provider) {
  const crypto::KeyPair decoy = crypto::KeyPair::createDeterministicBls12381KeyPair(
      "consensus-lock-unlock-forged-decoy");
  const consensus::ValidatorVoteRecord decoyVote =
      consensus::ValidatorVoteRecord::createVote(
          decoy.address().value(), decoy.publicKey(),
          decoy.privateKeyForSigningOnly(), block.index(), block.hash(),
          block.previousHash(), round, consensus::ValidatorVoteDecision::PRECOMMIT,
          "nil", timestamp, provider);
  return consensus::ValidatorVoteRecord(
      validator.keyPair.address().value(), validator.keyPair.publicKey(),
      block.index(), block.hash(), block.previousHash(), round,
      consensus::ValidatorVoteDecision::PRECOMMIT, "nil", timestamp,
      decoyVote.signatureBundle());
}

consensus::QuorumCertificate
buildPrecommitQC(const std::vector<ValidatorFixture> &validators,
                 const core::Block &block, std::uint64_t round,
                 std::int64_t voteTimestamp, const node::NodeRuntime &runtime,
                 const crypto::Bls12381SignatureProvider &provider) {
  std::vector<consensus::ValidatorVoteRecord> votes;
  votes.reserve(validators.size());
  for (const ValidatorFixture &validator : validators) {
    votes.push_back(voteFor(validator, block, round,
                            consensus::ValidatorVoteDecision::PRECOMMIT,
                            voteTimestamp, provider));
  }
  const auto result = consensus::QuorumCertificateBuilder::buildFromVotes(
      block.index(), block.hash(), block.previousHash(), round, votes,
      runtime.validatorRegistry(), developmentPolicy(), provider);
  require(result.certified(), "QC fixture must certify: " + result.reason());
  return result.certificate();
}

core::Block produceBlock(node::NodeRuntime &runtime, std::uint64_t round,
                         std::uint64_t txNonce, std::int64_t txTimestamp,
                         std::int64_t produceTimestamp) {
  test::admitConsensusTestTransfer(runtime, userKey(), txNonce, txTimestamp);
  const consensus::BlockCandidateResult candidate =
      consensus::BlockProductionPhase::produce(
          runtime, node::RuntimeBlockPipelineConfig(16, 1, round, produceTimestamp));
  require(candidate.produced(), "Candidate block must be produced.");
  return candidate.block();
}

// ---------------------------------------------------------------------------
// Group 1: ProposalJustification::permitsUnlock, table-tested in isolation.
// ---------------------------------------------------------------------------

void testPermitsUnlockTable() {
  const auto validators = makeValidators(4);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "permits-unlock-table");
  node::NodeRuntime runtime = startRuntime(genesis);
  const crypto::Bls12381SignatureProvider provider;

  const core::Block blockA =
      produceBlock(runtime, 1, 1, kTimestamp + 1, kTimestamp + 2);
  const core::ValidatorRegistry &registry = runtime.validatorRegistry();
  std::string reason;

  // Positive: a real QC at round 2 (>= lockedRound 1) certifying the exact
  // candidate block, in a round strictly greater than the lock.
  const consensus::QuorumCertificate qcAtRound2 =
      buildPrecommitQC(validators, blockA, 2, kTimestamp + 3, runtime, provider);
  const consensus::ProposalJustification justificationRound2 =
      consensus::ProposalJustification::unlockQuorumCertificate(qcAtRound2);
  require(consensus::ProposalJustification::permitsUnlock(
              justificationRound2, /*lockedRound=*/1, blockA.hash(),
              /*round=*/3, registry, developmentPolicy(), provider, &reason),
          "a safe QC (round >= lockedRound, correct block) must permit "
          "unlock: " +
              reason);

  // Negative: no justification at all.
  require(!consensus::ProposalJustification::permitsUnlock(
              consensus::ProposalJustification::none(), 1, blockA.hash(), 3,
              registry, developmentPolicy(), provider, &reason),
          "an absent justification must never permit unlock");

  // Negative: round does not exceed the lock's round, even with an
  // otherwise-valid QC.
  require(!consensus::ProposalJustification::permitsUnlock(
              justificationRound2, /*lockedRound=*/3, blockA.hash(),
              /*round=*/3, registry, developmentPolicy(), provider, &reason),
          "a proposal round that does not exceed the lock's round must "
          "never permit unlock");

  // Negative: the QC certifies a different block than the one being voted
  // for.
  require(!consensus::ProposalJustification::permitsUnlock(
              justificationRound2, 1, "some-other-candidate-block-hash", 3,
              registry, developmentPolicy(), provider, &reason),
          "a QC certifying a different block must never permit unlock");

  // Negative: the QC's own round is older than the current lock.
  const consensus::QuorumCertificate qcAtRound1 =
      buildPrecommitQC(validators, blockA, 1, kTimestamp + 4, runtime, provider);
  const consensus::ProposalJustification justificationRound1 =
      consensus::ProposalJustification::unlockQuorumCertificate(qcAtRound1);
  require(!consensus::ProposalJustification::permitsUnlock(
              justificationRound1, /*lockedRound=*/2, blockA.hash(),
              /*round=*/3, registry, developmentPolicy(), provider, &reason),
          "a QC round older than the current lock must never permit unlock");

  // Negative: a hand-built QC (bypassing QuorumCertificateBuilder's own
  // safety checks, since a malicious proposer is not bound by them) with
  // signed weight below the required threshold.
  const consensus::ValidatorVoteRecord singleVote =
      voteFor(validators.front(), blockA, 2,
             consensus::ValidatorVoteDecision::PRECOMMIT, kTimestamp + 5,
             provider);
  const std::uint64_t totalWeight = registry.totalConsensusWeight();
  const std::uint64_t requiredWeight =
      consensus::QuorumCertificateBuilder::requiredVotingWeight(totalWeight, 2,
                                                                 3);
  const std::uint64_t oneVoterWeight =
      registry.consensusWeightFor(singleVote.validatorAddress());
  require(oneVoterWeight < requiredWeight,
          "one validator's weight must be below quorum for this fixture to "
          "be meaningful");
  const consensus::QuorumCertificate underweightQc(
      blockA.index(), blockA.hash(), blockA.previousHash(), 2, requiredWeight,
      totalWeight, oneVoterWeight, registry.validatorSetRoot(), {singleVote});
  require(!underweightQc.isStructurallyValid(),
          "an underweight QC must already be structurally invalid");
  const consensus::ProposalJustification underweightJustification =
      consensus::ProposalJustification::unlockQuorumCertificate(underweightQc);
  require(!consensus::ProposalJustification::permitsUnlock(
              underweightJustification, 1, blockA.hash(), 3, registry,
              developmentPolicy(), provider, &reason),
          "a QC with insufficient signed weight must never permit unlock");

  // Negative: a QC containing one forged vote signature. Weight accounting
  // alone cannot tell the votes apart — only cryptographic verification can.
  std::vector<consensus::ValidatorVoteRecord> forgedVotes;
  forgedVotes.push_back(
      forgedVoteFor(validators[0], blockA, 2, kTimestamp + 6, provider));
  for (std::size_t index = 1; index < validators.size(); ++index) {
    forgedVotes.push_back(voteFor(validators[index], blockA, 2,
                                  consensus::ValidatorVoteDecision::PRECOMMIT,
                                  kTimestamp + 6, provider));
  }
  const consensus::QuorumCertificate forgedQc(
      blockA.index(), blockA.hash(), blockA.previousHash(), 2, requiredWeight,
      totalWeight, totalWeight, registry.validatorSetRoot(), forgedVotes);
  require(forgedQc.isStructurallyValid(),
          "a QC with a forged-but-consistent vote must still be "
          "structurally well-formed");
  const consensus::ProposalJustification forgedJustification =
      consensus::ProposalJustification::unlockQuorumCertificate(forgedQc);
  require(!consensus::ProposalJustification::permitsUnlock(
              forgedJustification, 1, blockA.hash(), 3, registry,
              developmentPolicy(), provider, &reason),
          "a QC containing a forged vote signature must never permit "
          "unlock");
}

// ---------------------------------------------------------------------------
// Shared fixture for groups 2-5: lock the local validator on block A at
// round 1, then advance to round 2.
// ---------------------------------------------------------------------------

struct LockedFixture {
  core::Block blockA;
  std::size_t proposerBIndex;
};

LockedFixture lockLocalValidatorOnBlockA(
    node::NodeRuntime &runtime, TestNetwork &network,
    consensus::ConsensusEventLoop &loop,
    const std::vector<ValidatorFixture> &validators,
    const crypto::Bls12381SignatureProvider &provider) {
  const std::uint64_t round1 = 1;
  const std::string chainId =
      runtime.config().genesisConfig().networkParameters().chainId();

  const std::string expectedProposerA = consensus::ProposerSchedule::selectProposer(
      runtime.validatorRegistry(), chainId, 1, round1);
  const std::size_t proposerAIndex = indexOfValidator(validators, expectedProposerA);

  const core::Block blockA =
      produceBlock(runtime, round1, 1, kTimestamp + 1, kTimestamp + 2);

  const consensus::BlockProposalResult proposalA = consensus::BlockProposalPhase::propose(
      blockA, validators[proposerAIndex].keyPair.address().value(), round1,
      kTimestamp + 3, crypto::Signer(validators[proposerAIndex].keyPair, provider),
      network.mesh, provider);
  require(proposalA.proposed(), "Proposal A must succeed.");

  network.mesh.injectLocalMessage(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                                  proposalA.serializedProposal(), kTimestamp + 3);
  network.routeMessages();
  const consensus::ConsensusTickResult resultA = loop.tick(kTimestamp + 4);
  require(!resultA.hasError(), "No error expected on tick A: " + resultA.errorMessage);

  // Enough PREVOTEs for block A to reach quorum, so the local validator
  // PRECOMMITs and locks on it.
  for (const ValidatorFixture &validator : validators) {
    const consensus::ValidatorVoteRecord vote =
        voteFor(validator, blockA, round1, consensus::ValidatorVoteDecision::PREVOTE,
               kTimestamp + 5, provider);
    network.mesh.injectLocalMessage(p2p::NetworkMessageType::VALIDATOR_VOTE,
                                    vote.serialize(), kTimestamp + 5);
  }
  network.routeMessages();
  loop.tick(kTimestamp + 6);

  const auto timeout = runtime.consensusRoundManager().roundTimeout().expiresAt();
  loop.tick(timeout + 1);
  require(runtime.consensusRoundManager().currentState().round() == 2,
          "Round must advance to 2 after the round-1 proposer timeout.");

  const std::string expectedProposerB = consensus::ProposerSchedule::selectProposer(
      runtime.validatorRegistry(), chainId, 1, 2);
  const std::size_t proposerBIndex = indexOfValidator(validators, expectedProposerB);

  return LockedFixture{blockA, proposerBIndex};
}

bool persistedPrevoteMatches(const std::filesystem::path &recoveryPath,
                             const std::string &blockHash) {
  const auto stored = consensus::ConsensusRecoveryStore::load(recoveryPath);
  return stored.has_value() && stored->persistedPrevote().has_value() &&
        stored->persistedPrevote()->blockHash() == blockHash;
}

// ---------------------------------------------------------------------------
// Group 2: locked validator does not vote for a different block without any
// justification.
// ---------------------------------------------------------------------------

void testLockedValidatorDoesNotVoteForDifferentBlockWithoutJustification() {
  const auto validators = makeValidators(4);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "lock-unlock-no-justification");
  node::NodeRuntime runtime = startRuntime(genesis);
  TestNetwork network(genesis);
  const crypto::Bls12381SignatureProvider provider;
  const crypto::Signer signer(validators.front().keyPair, provider);

  const std::filesystem::path recoveryPath =
      std::filesystem::temp_directory_path() /
      "nodo-consensus-lock-unlock-no-justification.state";
  std::error_code cleanupError;
  std::filesystem::remove(recoveryPath, cleanupError);

  consensus::ConsensusEventLoop loop(runtime, network.mesh, network.validatedInbox,
                                     developmentPolicy(), provider);
  loop.setRecoveryPath(recoveryPath);
  configureProducer(loop, signer);

  const LockedFixture locked =
      lockLocalValidatorOnBlockA(runtime, network, loop, validators, provider);

  const core::Block blockB =
      produceBlock(runtime, 2, 2, kTimestamp + 10, kTimestamp + 11);
  require(locked.blockA.hash() != blockB.hash(),
          "Block B must differ from block A.");

  const node::SignedBlockProposalMessage proposalB = node::SignedBlockProposalMessage::sign(
      blockB, validators[locked.proposerBIndex].keyPair.address().value(),
      validators[locked.proposerBIndex].keyPair.publicKey(),
      validators[locked.proposerBIndex].keyPair.privateKeyForSigningOnly(), 2,
      kTimestamp + 13, provider); // default justification = none()

  network.mesh.injectLocalMessage(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                                  proposalB.serialize(), kTimestamp + 14);
  network.mesh.drainInbox(p2p::NetworkMessageType::VALIDATOR_VOTE);
  network.routeMessages();
  const consensus::ConsensusTickResult resultB = loop.tick(kTimestamp + 15);
  require(!resultB.hasError(), "No error expected on tick B: " + resultB.errorMessage);

  require(!persistedPrevoteMatches(recoveryPath, blockB.hash()),
          "A locked validator must not prevote for a conflicting block with "
          "no unlock justification.");

  std::filesystem::remove(recoveryPath, cleanupError);
}

// ---------------------------------------------------------------------------
// Group 3: locked validator rejects an invalid justification (a real QC
// that certifies the wrong block).
// ---------------------------------------------------------------------------

void testLockedValidatorRejectsInvalidQCJustification() {
  const auto validators = makeValidators(4);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "lock-unlock-invalid-justification");
  node::NodeRuntime runtime = startRuntime(genesis);
  TestNetwork network(genesis);
  const crypto::Bls12381SignatureProvider provider;
  const crypto::Signer signer(validators.front().keyPair, provider);

  const std::filesystem::path recoveryPath =
      std::filesystem::temp_directory_path() /
      "nodo-consensus-lock-unlock-invalid-justification.state";
  std::error_code cleanupError;
  std::filesystem::remove(recoveryPath, cleanupError);

  consensus::ConsensusEventLoop loop(runtime, network.mesh, network.validatedInbox,
                                     developmentPolicy(), provider);
  loop.setRecoveryPath(recoveryPath);
  configureProducer(loop, signer);

  const LockedFixture locked =
      lockLocalValidatorOnBlockA(runtime, network, loop, validators, provider);

  const core::Block blockB =
      produceBlock(runtime, 2, 2, kTimestamp + 10, kTimestamp + 11);
  require(locked.blockA.hash() != blockB.hash(),
          "Block B must differ from block A.");

  // A real, cryptographically valid QC — but it certifies block A, not the
  // block actually being proposed. An honest unlock justification for block
  // B can never be built from a QC that certifies a different block.
  const consensus::QuorumCertificate qcForWrongBlock =
      buildPrecommitQC(validators, locked.blockA, 1, kTimestamp + 12, runtime,
                       provider);
  const consensus::ProposalJustification invalidJustification =
      consensus::ProposalJustification::unlockQuorumCertificate(qcForWrongBlock);

  const node::SignedBlockProposalMessage proposalB = node::SignedBlockProposalMessage::sign(
      blockB, validators[locked.proposerBIndex].keyPair.address().value(),
      validators[locked.proposerBIndex].keyPair.publicKey(),
      validators[locked.proposerBIndex].keyPair.privateKeyForSigningOnly(), 2,
      kTimestamp + 13, provider, invalidJustification);

  network.mesh.injectLocalMessage(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                                  proposalB.serialize(), kTimestamp + 14);
  network.mesh.drainInbox(p2p::NetworkMessageType::VALIDATOR_VOTE);
  network.routeMessages();
  const consensus::ConsensusTickResult resultB = loop.tick(kTimestamp + 15);
  require(!resultB.hasError(), "No error expected on tick B: " + resultB.errorMessage);

  require(!persistedPrevoteMatches(recoveryPath, blockB.hash()),
          "A locked validator must not prevote for a conflicting block "
          "justified by a QC that certifies a different block.");

  std::filesystem::remove(recoveryPath, cleanupError);
}

// ---------------------------------------------------------------------------
// Group 4: locked validator accepts a safe justification.
// ---------------------------------------------------------------------------

void testLockedValidatorAcceptsSafeQCJustification() {
  const auto validators = makeValidators(4);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "lock-unlock-safe-justification");
  node::NodeRuntime runtime = startRuntime(genesis);
  TestNetwork network(genesis);
  const crypto::Bls12381SignatureProvider provider;
  const crypto::Signer signer(validators.front().keyPair, provider);

  const std::filesystem::path recoveryPath =
      std::filesystem::temp_directory_path() /
      "nodo-consensus-lock-unlock-safe-justification.state";
  std::error_code cleanupError;
  std::filesystem::remove(recoveryPath, cleanupError);

  consensus::ConsensusEventLoop loop(runtime, network.mesh, network.validatedInbox,
                                     developmentPolicy(), provider);
  loop.setRecoveryPath(recoveryPath);
  configureProducer(loop, signer);

  const LockedFixture locked =
      lockLocalValidatorOnBlockA(runtime, network, loop, validators, provider);

  const core::Block blockB =
      produceBlock(runtime, 2, 2, kTimestamp + 10, kTimestamp + 11);
  require(locked.blockA.hash() != blockB.hash(),
          "Block B must differ from block A.");

  // A real QC proving 2/3+ precommitted block B at round 1 (>= lockedRound).
  const consensus::QuorumCertificate qcForBlockB =
      buildPrecommitQC(validators, blockB, 1, kTimestamp + 12, runtime, provider);
  const consensus::ProposalJustification safeJustification =
      consensus::ProposalJustification::unlockQuorumCertificate(qcForBlockB);

  const node::SignedBlockProposalMessage proposalB = node::SignedBlockProposalMessage::sign(
      blockB, validators[locked.proposerBIndex].keyPair.address().value(),
      validators[locked.proposerBIndex].keyPair.publicKey(),
      validators[locked.proposerBIndex].keyPair.privateKeyForSigningOnly(), 2,
      kTimestamp + 13, provider, safeJustification);

  network.mesh.injectLocalMessage(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                                  proposalB.serialize(), kTimestamp + 14);
  network.mesh.drainInbox(p2p::NetworkMessageType::VALIDATOR_VOTE);
  network.routeMessages();
  const consensus::ConsensusTickResult resultB = loop.tick(kTimestamp + 15);
  require(!resultB.hasError(), "No error expected on tick B: " + resultB.errorMessage);

  require(persistedPrevoteMatches(recoveryPath, blockB.hash()),
          "A locked validator must prevote for a conflicting block when a "
          "safe unlock QC is provided.");

  std::filesystem::remove(recoveryPath, cleanupError);
}

// ---------------------------------------------------------------------------
// Group 5: two conflicting blocks are never both finalized at the same
// height.
// ---------------------------------------------------------------------------

void testNeverFinalizesConflictingBlocksAtSameHeight() {
  const auto validators = makeValidators(4);
  const config::GenesisConfig genesis =
      makeGenesis(validators, "lock-unlock-no-double-finalize");
  node::NodeRuntime runtime = startRuntime(genesis);
  TestNetwork network(genesis);
  const crypto::Bls12381SignatureProvider provider;
  const crypto::Signer signer(validators.front().keyPair, provider);

  const std::filesystem::path recoveryPath =
      std::filesystem::temp_directory_path() /
      "nodo-consensus-lock-unlock-no-double-finalize.state";
  std::error_code cleanupError;
  std::filesystem::remove(recoveryPath, cleanupError);

  consensus::ConsensusEventLoop loop(runtime, network.mesh, network.validatedInbox,
                                     developmentPolicy(), provider);
  loop.setRecoveryPath(recoveryPath);
  configureProducer(loop, signer);

  const LockedFixture locked =
      lockLocalValidatorOnBlockA(runtime, network, loop, validators, provider);

  const core::Block blockB =
      produceBlock(runtime, 2, 2, kTimestamp + 10, kTimestamp + 11);

  const consensus::QuorumCertificate qcForBlockB =
      buildPrecommitQC(validators, blockB, 1, kTimestamp + 12, runtime, provider);
  const consensus::ProposalJustification safeJustification =
      consensus::ProposalJustification::unlockQuorumCertificate(qcForBlockB);

  const node::SignedBlockProposalMessage proposalB = node::SignedBlockProposalMessage::sign(
      blockB, validators[locked.proposerBIndex].keyPair.address().value(),
      validators[locked.proposerBIndex].keyPair.publicKey(),
      validators[locked.proposerBIndex].keyPair.privateKeyForSigningOnly(), 2,
      kTimestamp + 13, provider, safeJustification);

  network.mesh.injectLocalMessage(p2p::NetworkMessageType::BLOCK_PROPOSAL,
                                  proposalB.serialize(), kTimestamp + 14);
  network.mesh.drainInbox(p2p::NetworkMessageType::VALIDATOR_VOTE);
  network.routeMessages();
  loop.tick(kTimestamp + 15); // local validator unlocks and PREVOTEs for B

  for (const ValidatorFixture &validator : validators) {
    const consensus::ValidatorVoteRecord vote =
        voteFor(validator, blockB, 2, consensus::ValidatorVoteDecision::PREVOTE,
               kTimestamp + 16, provider);
    network.mesh.injectLocalMessage(p2p::NetworkMessageType::VALIDATOR_VOTE,
                                    vote.serialize(), kTimestamp + 16);
  }
  network.routeMessages();
  loop.tick(kTimestamp + 17); // local validator PRECOMMITs for B

  for (const ValidatorFixture &validator : validators) {
    const consensus::ValidatorVoteRecord vote =
        voteFor(validator, blockB, 2, consensus::ValidatorVoteDecision::PRECOMMIT,
               kTimestamp + 18, provider);
    network.mesh.injectLocalMessage(p2p::NetworkMessageType::VALIDATOR_VOTE,
                                    vote.serialize(), kTimestamp + 18);
  }
  network.routeMessages();
  const consensus::ConsensusTickResult finalizeResult = loop.tick(kTimestamp + 19);

  require(finalizeResult.blockFinalized,
          "Block B must finalize honestly via the unlock flow: " +
              finalizeResult.errorMessage);
  require(runtime.finalizationRegistry().hasFinalizedHeight(1),
          "Height 1 must be finalized.");
  require(runtime.finalizationRegistry().isFinalizedBlock(1, blockB.hash()),
          "Block B must be the finalized block at height 1.");

  // Now attempt to also finalize a THIRD, conflicting block C at the same
  // height, independently backed by its own genuinely valid quorum
  // certificate — this is exactly the situation the lock/unlock rule exists
  // to make unreachable for honest validators. Assert the finalization
  // machinery itself refuses it as defense in depth, using the very same
  // runtime the honest flow just finalized B on.
  const std::string genesisHash = runtime.blockchain().blockByHeight(0)->hash();
  const core::Block blockC(
      1, genesisHash,
      {core::LedgerRecord::fromValidationWorkRecord(
          economics::ValidationWorkRecord(
              "bootstrap", 1, economics::ValidationWorkType::VALIDATE_BLOCK,
              economics::ValidationWorkResult::ACCEPTED,
              "lock-unlock-conflicting-block-c", "conflicting-evidence-c", 1,
              kTimestamp),
          kTimestamp + 20)},
      kTimestamp + 20);
  require(blockC.hash() != blockB.hash() && blockC.hash() != locked.blockA.hash(),
          "Block C must be a genuinely distinct third block.");

  const consensus::QuorumCertificate qcForBlockC =
      buildPrecommitQC(validators, blockC, 3, kTimestamp + 21, runtime, provider);

  const consensus::BlockFinalizationResult conflictResult =
      consensus::BlockFinalizer::finalizeBlock(
          runtime.mutableBlockchain(), blockC, qcForBlockC,
          runtime.validatorRegistry(), runtime.mutableFinalizationRegistry(),
          developmentPolicy(), provider, kTimestamp + 22);

  require(!conflictResult.finalized(),
          "A conflicting block at an already-finalized height must never "
          "finalize, even with its own valid quorum certificate.");
  require(runtime.blockchain().size() == 2,
          "The conflicting block must never be appended to the chain.");
  require(runtime.finalizationRegistry().isFinalizedBlock(1, blockB.hash()),
          "Height 1 must still resolve to block B only.");
  require(!runtime.finalizationRegistry().isFinalizedBlock(1, blockC.hash()),
          "Block C must never be recorded as finalized at height 1.");

  std::filesystem::remove(recoveryPath, cleanupError);
}

} // namespace

int main() {
  try {
    testPermitsUnlockTable();
    testLockedValidatorDoesNotVoteForDifferentBlockWithoutJustification();
    testLockedValidatorRejectsInvalidQCJustification();
    testLockedValidatorAcceptsSafeQCJustification();
    testNeverFinalizesConflictingBlocksAtSameHeight();
    std::cout << "Consensus lock/unlock tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Consensus lock/unlock tests FAILED: " << error.what() << "\n";
    return 1;
  }
}
