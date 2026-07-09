#include "node/LightClientProtocol.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/MerkleTree.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "economics/ValidationWorkRecord.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;
using nodo::node::LightClientHeader;
using nodo::node::LightClientProtocolVerifier;

constexpr std::int64_t kTimestamp = 1900000000;

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

crypto::KeyPair keyPairFor(const std::string &suffix) {
  return crypto::KeyPair::createDeterministicBls12381KeyPair(
      "light-client-validator-" + suffix);
}

crypto::PublicKey publicKeyFor(const std::string &suffix) {
  return keyPairFor(suffix).publicKey();
}

crypto::PrivateKey privateKeyFor(const std::string &suffix) {
  return keyPairFor(suffix).privateKeyForSigningOnly();
}

std::string addressFor(const crypto::PublicKey &key) {
  return crypto::AddressDerivation::deriveFromPublicKey(key).value();
}

core::ValidatorRegistrationRecord registrationFor(const crypto::PublicKey &key,
                                                  std::int64_t timestamp) {
  return core::ValidatorRegistrationRecord(
      addressFor(key), key, 1,
      "light-client-metadata-" + key.fingerprint().substr(0, 12), timestamp);
}

core::ValidatorRegistry registryWith(const std::vector<std::string> &suffixes) {
  core::ValidatorRegistry registry;
  for (const std::string &suffix : suffixes) {
    require(registry.registerValidator(registrationFor(publicKeyFor(suffix),
                                                        kTimestamp + 1))
                .accepted(),
            "validator " + suffix + " should register");
  }
  return registry;
}

economics::ValidationWorkRecord validationWork(const std::string &evidence) {
  return economics::ValidationWorkRecord(
      "bootstrap", 1, economics::ValidationWorkType::VALIDATE_BLOCK,
      economics::ValidationWorkResult::ACCEPTED,
      "light-client-target-" + evidence, evidence, 1, kTimestamp);
}

core::Blockchain blockchainWithGenesis() {
  const core::Block genesis = core::Block::createGenesisBlock(
      {core::LedgerRecord::fromValidationWorkRecord(validationWork("genesis"),
                                                     kTimestamp + 2)},
      kTimestamp + 3);
  core::Blockchain blockchain;
  blockchain.addGenesisBlock(genesis);
  return blockchain;
}

core::Block candidateBlock(const core::Blockchain &blockchain,
                           const std::string &evidence,
                           std::int64_t timestamp) {
  return core::Block(
      blockchain.size(), blockchain.latestBlock().hash(),
      {core::LedgerRecord::fromValidationWorkRecord(validationWork(evidence),
                                                     timestamp)},
      timestamp);
}

consensus::ValidatorVoteRecord
voteFor(const std::string &suffix, const core::Block &block, std::uint64_t round,
       std::int64_t timestamp, const crypto::Bls12381SignatureProvider &provider) {
  const crypto::PublicKey key = publicKeyFor(suffix);
  return consensus::ValidatorVoteRecord::createVote(
      addressFor(key), key, privateKeyFor(suffix), block.index(), block.hash(),
      block.previousHash(), round, consensus::ValidatorVoteDecision::PRECOMMIT,
      "NONE", timestamp, provider);
}

// A vote that *claims* to be validator `suffix` (correct, self-consistent
// address + public key — isStructurallyValid()'s address<->key binding check
// passes) but whose signature bytes were genuinely produced by a different
// signer over a different payload, then relabeled. Bls12381SignatureProvider
// refuses to sign with a public/private key pair that don't match (so a
// mismatched-keypair forgery can't even be constructed), so this is the
// realistic forgery shape: a real signature, reattributed. Only the
// cryptographic check in ValidatorVoteRecord::verify() (which recomputes the
// signing payload from the claimed identity and checks it against the
// signature) can catch this.
consensus::ValidatorVoteRecord
forgedVoteFor(const std::string &suffix, const core::Block &block,
             std::uint64_t round, std::int64_t timestamp,
             const crypto::Bls12381SignatureProvider &provider) {
  const consensus::ValidatorVoteRecord decoy =
      consensus::ValidatorVoteRecord::createVote(
          addressFor(publicKeyFor("forged-decoy")), publicKeyFor("forged-decoy"),
          privateKeyFor("forged-decoy"), block.index(), block.hash(),
          block.previousHash(), round, consensus::ValidatorVoteDecision::PRECOMMIT,
          "NONE", timestamp, provider);

  const crypto::PublicKey key = publicKeyFor(suffix);
  return consensus::ValidatorVoteRecord(
      addressFor(key), key, block.index(), block.hash(), block.previousHash(),
      round, consensus::ValidatorVoteDecision::PRECOMMIT, "NONE", timestamp,
      decoy.signatureBundle());
}

consensus::QuorumCertificate
certificateFrom(const core::Block &block,
                const std::vector<consensus::ValidatorVoteRecord> &votes,
                const core::ValidatorRegistry &registry, std::uint64_t round = 1) {
  const crypto::Bls12381SignatureProvider provider;
  const auto result = consensus::QuorumCertificateBuilder::buildFromVotes(
      block.index(), block.hash(), block.previousHash(), round, votes, registry,
      crypto::CryptoPolicy::developmentPolicy(), provider);
  require(result.certified(), "quorum certificate fixture should certify: " +
                                   result.reason());
  return result.certificate();
}

LightClientHeader headerFor(const core::Block &block,
                            const consensus::FinalizedBlockRecord &record) {
  return LightClientHeader(
      "localnet", "light-client-chain", "light-client-genesis", block.index(),
      block.hash(), block.previousHash(), block.stateRoot(),
      block.receiptsRoot(), block.timestamp(), block.headerPayload(),
      record.quorumCertificate().validatorSetRoot(), record.serialize(),
      record.quorumCertificate().serialize());
}

// Builds a genuine 2-block, QC-backed chain where the validator set changes
// between block 1 and block 2 (a 4th validator, "d", is added), each block's
// FinalizedBlockRecord/QuorumCertificate is real and cryptographically
// valid, and a ValidatorSetHistory records the exact registry active at
// each height — the fixture every positive/negative test below builds on.
struct ChainFixture {
  core::Block block1;
  core::Block block2;
  consensus::FinalizedBlockRecord record1;
  consensus::FinalizedBlockRecord record2;
  core::ValidatorRegistry registry1;
  core::ValidatorRegistry registry2;
  core::ValidatorSetHistory history;
  LightClientHeader header1;
  LightClientHeader header2;
  crypto::CryptoPolicy policy;
};

ChainFixture buildChainFixture() {
  crypto::Bls12381SignatureProvider provider;
  core::Blockchain blockchain = blockchainWithGenesis();

  const core::ValidatorRegistry registry1 = registryWith({"a", "b", "c"});

  const core::Block block1 =
      candidateBlock(blockchain, "block-1", kTimestamp + 10);
  const consensus::QuorumCertificate qc1 = certificateFrom(
      block1,
      {voteFor("a", block1, 1, kTimestamp + 11, provider),
       voteFor("b", block1, 1, kTimestamp + 12, provider)},
      registry1);
  const consensus::FinalizedBlockRecord record1(
      block1.index(), block1.hash(), block1.previousHash(), 1,
      kTimestamp + 13, qc1);
  blockchain.addBlock(block1);

  // Validator set changes for height 2: a 4th validator joins. This is the
  // validator-set transition the light client must handle correctly —
  // block 2's QC is only valid against registry2, never registry1.
  core::ValidatorRegistry registry2 = registry1;
  require(registry2.registerValidator(registrationFor(publicKeyFor("d"),
                                                       kTimestamp + 14))
              .accepted(),
          "validator d should register for height 2");
  require(registry2.validatorSetRoot() != registry1.validatorSetRoot(),
          "registry must actually change between height 1 and 2");

  const core::Block block2 =
      candidateBlock(blockchain, "block-2", kTimestamp + 20);
  const consensus::QuorumCertificate qc2 = certificateFrom(
      block2,
      {voteFor("a", block2, 1, kTimestamp + 21, provider),
       voteFor("b", block2, 1, kTimestamp + 22, provider),
       voteFor("c", block2, 1, kTimestamp + 23, provider)},
      registry2);
  const consensus::FinalizedBlockRecord record2(
      block2.index(), block2.hash(), block2.previousHash(), 1,
      kTimestamp + 24, qc2);
  blockchain.addBlock(block2);

  core::ValidatorSetHistory history;
  require(history.recordSet(1, registry1), "history must record height 1");
  require(history.recordSet(2, registry2), "history must record height 2");

  return ChainFixture{block1,
                      block2,
                      record1,
                      record2,
                      registry1,
                      registry2,
                      history,
                      headerFor(block1, record1),
                      headerFor(block2, record2),
                      crypto::CryptoPolicy::developmentPolicy()};
}

void testRejectsBrokenHeaderChain() {
  LightClientHeader a("localnet", "chain", "genesis", 1, "hash-a",
                      "genesis-hash", "state", "receipts", 10, "payload-a",
                      "validators", "record", "qc");
  LightClientHeader b("localnet", "chain", "genesis", 3, "hash-b", "not-hash-a",
                      "state", "receipts", 11, "payload-b", "validators",
                      "record", "qc");
  std::string reason;
  assert(!LightClientProtocolVerifier::verifyHeaderChain({a, b}, &reason));
  assert(!reason.empty());
}

void testProofBundleJsonIsStable() {
  const std::string root = nodo::core::MerkleTree::buildRoot({"a", "b"});
  assert(!root.empty());
}

void testVerifiesRealFinalizedHeader() {
  const crypto::Bls12381SignatureProvider provider;
  const ChainFixture fixture = buildChainFixture();
  std::string reason;
  require(LightClientProtocolVerifier::verifyFinalizedHeader(
              fixture.header1, fixture.registry1, fixture.policy, provider,
              &reason),
          "valid header 1 should verify: " + reason);
  require(LightClientProtocolVerifier::verifyFinalizedHeader(
              fixture.header2, fixture.registry2, fixture.policy, provider,
              &reason),
          "valid header 2 should verify against its post-transition "
          "registry: " +
              reason);
}

void testVerifiesChainAcrossValidatorSetTransition() {
  const crypto::Bls12381SignatureProvider provider;
  const ChainFixture fixture = buildChainFixture();
  std::string reason;
  require(LightClientProtocolVerifier::verifyFinalizedHeaderChain(
              {fixture.header1, fixture.header2}, fixture.history,
              fixture.policy, provider, &reason),
          "chain spanning a validator-set change should verify: " + reason);
}

void testRejectsHeaderVerifiedAgainstStaleRegistry() {
  const crypto::Bls12381SignatureProvider provider;
  const ChainFixture fixture = buildChainFixture();
  std::string reason;
  // The naive mistake this design exists to prevent: reusing height 1's
  // registry (or any single shared registry) to verify height 2's header
  // after the validator set changed.
  require(!LightClientProtocolVerifier::verifyFinalizedHeader(
              fixture.header2, fixture.registry1, fixture.policy, provider,
              &reason),
          "header 2 must not verify against the pre-transition registry");
}

void testRejectsForgedVoteSignature() {
  const crypto::Bls12381SignatureProvider provider;
  const ChainFixture fixture = buildChainFixture();

  const consensus::ValidatorVoteRecord forgedA =
      forgedVoteFor("a", fixture.block1, 1, kTimestamp + 11, provider);
  const consensus::ValidatorVoteRecord realB =
      voteFor("b", fixture.block1, 1, kTimestamp + 12, provider);

  const std::uint64_t totalWeight = fixture.registry1.totalConsensusWeight();
  const std::uint64_t requiredWeight =
      consensus::QuorumCertificateBuilder::requiredVotingWeight(totalWeight, 2,
                                                                 3);
  const std::uint64_t signedWeight =
      fixture.registry1.consensusWeightFor(forgedA.validatorAddress()) +
      fixture.registry1.consensusWeightFor(realB.validatorAddress());
  require(signedWeight >= requiredWeight,
          "forged-vote fixture must still meet the weight threshold so only "
          "the signature check can reject it");

  const consensus::QuorumCertificate forgedCertificate(
      fixture.block1.index(), fixture.block1.hash(),
      fixture.block1.previousHash(), 1, requiredWeight, totalWeight,
      signedWeight, fixture.registry1.validatorSetRoot(), {forgedA, realB});
  require(forgedCertificate.isStructurallyValid(),
          "forged certificate must still be structurally well-formed");

  const consensus::FinalizedBlockRecord forgedRecord(
      fixture.block1.index(), fixture.block1.hash(),
      fixture.block1.previousHash(), 1, kTimestamp + 13, forgedCertificate);
  const LightClientHeader forgedHeader = headerFor(fixture.block1, forgedRecord);

  std::string reason;
  require(!LightClientProtocolVerifier::verifyFinalizedHeader(
              forgedHeader, fixture.registry1, fixture.policy, provider,
              &reason),
          "a header backed by a forged vote signature must not verify");
}

void testRejectsInsufficientSignedWeight() {
  const crypto::Bls12381SignatureProvider provider;
  const ChainFixture fixture = buildChainFixture();

  const consensus::ValidatorVoteRecord realA =
      voteFor("a", fixture.block1, 1, kTimestamp + 11, provider);
  const std::uint64_t totalWeight = fixture.registry1.totalConsensusWeight();
  const std::uint64_t requiredWeight =
      consensus::QuorumCertificateBuilder::requiredVotingWeight(totalWeight, 2,
                                                                 3);
  const std::uint64_t oneVoterWeight =
      fixture.registry1.consensusWeightFor(realA.validatorAddress());
  require(oneVoterWeight < requiredWeight,
          "a single validator's weight must be below the quorum threshold "
          "for this fixture to be meaningful");

  // A single real (correctly signed) vote is not enough validator weight to
  // reach quorum, so a hand-crafted QC claiming a single vote is enough must
  // be rejected. A malicious peer is not bound by
  // QuorumCertificateBuilder::buildFromVotes's own safety checks, so the
  // light client must independently enforce the threshold.
  const consensus::QuorumCertificate underWeightCertificate(
      fixture.block1.index(), fixture.block1.hash(),
      fixture.block1.previousHash(), 1, requiredWeight, totalWeight,
      oneVoterWeight, fixture.registry1.validatorSetRoot(), {realA});
  require(!underWeightCertificate.isStructurallyValid(),
          "a QC with signedVotingWeight below requiredVotingWeight must "
          "already be structurally invalid");

  const consensus::FinalizedBlockRecord underWeightRecord(
      fixture.block1.index(), fixture.block1.hash(),
      fixture.block1.previousHash(), 1, kTimestamp + 13,
      underWeightCertificate);
  const LightClientHeader underWeightHeader =
      headerFor(fixture.block1, underWeightRecord);

  std::string reason;
  require(!LightClientProtocolVerifier::verifyFinalizedHeader(
              underWeightHeader, fixture.registry1, fixture.policy, provider,
              &reason),
          "a header with insufficient signed validator weight must not "
          "verify");
}

void testRejectsInconsistentStandaloneQuorumCertificateField() {
  const crypto::Bls12381SignatureProvider provider;
  const ChainFixture fixture = buildChainFixture();

  // Swap in block 2's (also valid, but different) certificate as the
  // header's standalone quorumCertificate field, leaving finalizedRecord
  // (still block 1's) untouched. Each field is independently well-formed
  // and deserializable; only the cross-check between them can catch this.
  const LightClientHeader tampered(
      fixture.header1.networkName(), fixture.header1.chainId(),
      fixture.header1.genesisConfigId(), fixture.header1.height(),
      fixture.header1.blockHash(), fixture.header1.previousHash(),
      fixture.header1.stateRoot(), fixture.header1.receiptsRoot(),
      fixture.header1.timestamp(), fixture.header1.headerPayload(),
      fixture.header1.validatorSetRoot(), fixture.header1.finalizedRecord(),
      fixture.record2.quorumCertificate().serialize());

  std::string reason;
  require(!LightClientProtocolVerifier::verifyFinalizedHeader(
              tampered, fixture.registry1, fixture.policy, provider, &reason),
          "a header whose standalone quorumCertificate field disagrees with "
          "its finalizedRecord must not verify");
}

void testRejectsMalformedEmbeddedFields() {
  const crypto::Bls12381SignatureProvider provider;
  const ChainFixture fixture = buildChainFixture();

  const LightClientHeader malformedRecord(
      fixture.header1.networkName(), fixture.header1.chainId(),
      fixture.header1.genesisConfigId(), fixture.header1.height(),
      fixture.header1.blockHash(), fixture.header1.previousHash(),
      fixture.header1.stateRoot(), fixture.header1.receiptsRoot(),
      fixture.header1.timestamp(), fixture.header1.headerPayload(),
      fixture.header1.validatorSetRoot(), "not-a-real-finalized-record",
      fixture.header1.quorumCertificate());

  std::string reason;
  require(!LightClientProtocolVerifier::verifyFinalizedHeader(
              malformedRecord, fixture.registry1, fixture.policy, provider,
              &reason),
          "a garbage finalizedRecord field must be rejected, not thrown");
  require(!reason.empty(), "rejection must carry a reason");
}

void testChainRejectsMissingValidatorSetForHeight() {
  const crypto::Bls12381SignatureProvider provider;
  const ChainFixture fixture = buildChainFixture();

  core::ValidatorSetHistory partialHistory;
  require(partialHistory.recordSet(1, fixture.registry1),
          "partial history must record height 1");
  // Height 2 is deliberately missing.

  std::string reason;
  require(!LightClientProtocolVerifier::verifyFinalizedHeaderChain(
              {fixture.header1, fixture.header2}, partialHistory,
              fixture.policy, provider, &reason),
          "a chain missing a recorded validator set for one of its heights "
          "must not verify");
  require(!reason.empty(), "rejection must carry a reason");
}

} // namespace

int main() {
  try {
    testRejectsBrokenHeaderChain();
    testProofBundleJsonIsStable();
    testVerifiesRealFinalizedHeader();
    testVerifiesChainAcrossValidatorSetTransition();
    testRejectsHeaderVerifiedAgainstStaleRegistry();
    testRejectsForgedVoteSignature();
    testRejectsInsufficientSignedWeight();
    testRejectsInconsistentStandaloneQuorumCertificateField();
    testRejectsMalformedEmbeddedFields();
    testChainRejectsMissingValidatorSetForHeight();
    std::cout << "Nodo LightClientProtocol tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo LightClientProtocol tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
