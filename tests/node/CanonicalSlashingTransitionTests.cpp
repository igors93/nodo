#include "economics/StakeAccount.hpp"
#include "node/CanonicalSlashingTransition.hpp"
#include "node/SignedBlockProposalMessage.hpp"
#include "node/StakingRegistry.hpp"

#include "config/NetworkParameters.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureProvider.hpp"
#include "serialization/LedgerRecordCodec.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000LL;

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

class TestBlsSignatureProvider final : public crypto::SignatureProvider {
public:
  crypto::CryptoAlgorithm algorithm() const override {
    return crypto::CryptoAlgorithm::BLS12_381;
  }

  crypto::Signature sign(const std::string &,
                         const crypto::PublicKey &publicKey,
                         const crypto::PrivateKey &, std::int64_t timestamp,
                         crypto::SigningDomain domain) const override {
    return crypto::Signature(crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
                             domain, algorithm(), publicKey,
                             std::string(192, 'c'), timestamp);
  }

  crypto::SignatureVerificationResult
  verify(const std::string &message,
         const crypto::Signature &signature) const override {
    return !message.empty() && signature.isValid() &&
                   signature.algorithm() == algorithm()
               ? crypto::SignatureVerificationResult::valid()
               : crypto::SignatureVerificationResult::invalid(
                     "Invalid deterministic test signature.");
  }
};

crypto::KeyPair validatorKey() {
  return crypto::KeyPair(crypto::PublicKey(crypto::CryptoAlgorithm::BLS12_381,
                                           std::string(96, 'a')),
                         crypto::PrivateKey(crypto::CryptoAlgorithm::BLS12_381,
                                            std::string(64, 'b')));
}

consensus::DoubleVoteEvidence
evidenceFor(const crypto::KeyPair &key,
            const TestBlsSignatureProvider &provider) {
  const auto makeVote = [&](const std::string &blockHash) {
    return consensus::ValidatorVoteRecord::createVote(
        key.address().value(), key.publicKey(), key.privateKeyForSigningOnly(),
        1, blockHash, "previous-block", 1,
        consensus::ValidatorVoteDecision::PRECOMMIT, "double-vote-test",
        kTimestamp + 1, provider);
  };
  return consensus::DoubleVoteEvidence(makeVote("block-a"), makeVote("block-b"),
                                       kTimestamp + 2);
}

void testAppliesVerifiedEvidenceOnce() {
  const crypto::KeyPair key = validatorKey();
  const TestBlsSignatureProvider provider;
  const consensus::DoubleVoteEvidence evidence = evidenceFor(key, provider);

  core::ValidatorRegistry validators;
  const core::ValidatorRegistrationRecord registration(
      key.address().value(), key.publicKey(), 1, "canonical-slashing-validator",
      kTimestamp);
  require(validators.registerValidator(registration).success(),
          "Test validator must register.");

  core::ValidatorSetHistory history;
  require(history.recordSet(1, validators),
          "Historical validator set must be recorded.");

  consensus::ValidatorPenaltyLedger ledger;
  node::StakingRegistry staking;
  staking.setAccount(
      key.address().value(),
      economics::StakeAccount(
          key.address().value(),
          utils::Amount::fromRawUnits(static_cast<std::int64_t>(
              core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS))));
  const core::LedgerRecord record =
      node::CanonicalSlashingTransition::buildEvidenceRecord(evidence,
                                                             kTimestamp + 3);
  const core::LedgerRecord restored =
      serialization::LedgerRecordCodec::deserialize(record.serialize());
  require(restored.serialize() == record.serialize(),
          "Slashing evidence ledger records must round-trip canonically.");
  node::CanonicalSlashingTransition::applyEvidenceRecords(
      {record}, 2, kTimestamp + 3, history,
      config::NetworkParameters::developmentLocal(),
      crypto::CryptoPolicy::developmentPolicy(), provider, ledger, validators,
      staking);

  require(ledger.size() == 1 && ledger.containsEvidence(evidence.evidenceId()),
          "Verified evidence must create one canonical penalty.");
  const core::ValidatorRegistryEntry *entry =
      validators.entryForAddress(key.address().value());
  require(entry != nullptr && entry->jailed(),
          "Double-voting validator must be jailed.");
  const economics::StakeAccount *stakeAccount =
      staking.accountFor(key.address().value());
  require(stakeAccount != nullptr && stakeAccount->jailed() &&
              stakeAccount->slashedAmount().rawUnits() == 50000,
          "Double-voting validator must be slashed and jailed in staking.");

  bool duplicateRejected = false;
  try {
    node::CanonicalSlashingTransition::applyEvidenceRecords(
        {record}, 3, kTimestamp + 4, history,
        config::NetworkParameters::developmentLocal(),
        crypto::CryptoPolicy::developmentPolicy(), provider, ledger, validators,
        staking);
  } catch (const std::invalid_argument &) {
    duplicateRejected = true;
  }
  require(duplicateRejected,
          "The same evidence must not produce a second penalty.");
}

core::Block
signedProposalBlock(const consensus::DoubleVoteEvidence &seedEvidence,
                    std::int64_t timestamp, char stateRootChar) {
  const core::LedgerRecord record =
      node::CanonicalSlashingTransition::buildEvidenceRecord(seedEvidence,
                                                             timestamp);
  return core::Block(1, "previous-proposal-block", {record}, timestamp,
                     std::string(64, stateRootChar),
                     std::string(64, stateRootChar == 'a' ? 'b' : 'c'));
}

consensus::ProposerEquivocationEvidence
proposerEvidenceFor(const crypto::KeyPair &key,
                    const TestBlsSignatureProvider &provider) {
  const consensus::DoubleVoteEvidence seedEvidence = evidenceFor(key, provider);
  const core::Block firstBlock =
      signedProposalBlock(seedEvidence, kTimestamp + 4, 'a');
  const core::Block secondBlock =
      signedProposalBlock(seedEvidence, kTimestamp + 5, 'd');
  const node::SignedBlockProposalMessage firstProposal =
      node::SignedBlockProposalMessage::sign(
          firstBlock, key.address().value(), key.publicKey(),
          key.privateKeyForSigningOnly(), 1, kTimestamp + 6, provider);
  const node::SignedBlockProposalMessage secondProposal =
      node::SignedBlockProposalMessage::sign(
          secondBlock, key.address().value(), key.publicKey(),
          key.privateKeyForSigningOnly(), 1, kTimestamp + 7, provider);
  return consensus::ProposerEquivocationEvidence(
      firstProposal.serialize(), secondProposal.serialize(),
      key.address().value(), 1, 1, firstBlock.hash(), secondBlock.hash(),
      kTimestamp + 8);
}

void testAppliesProposerEquivocationAsTombstoneAndStakeSlash() {
  const crypto::KeyPair key = validatorKey();
  const TestBlsSignatureProvider provider;
  const consensus::ProposerEquivocationEvidence evidence =
      proposerEvidenceFor(key, provider);

  core::ValidatorRegistry validators;
  const core::ValidatorRegistrationRecord registration(
      key.address().value(), key.publicKey(), 1,
      "canonical-proposer-equivocation-validator", kTimestamp);
  require(validators.registerValidator(registration).success(),
          "Test proposer must register.");

  core::ValidatorSetHistory history;
  require(history.recordSet(1, validators),
          "Historical proposer validator set must be recorded.");

  consensus::ValidatorPenaltyLedger ledger;
  node::StakingRegistry staking;
  staking.setAccount(
      key.address().value(),
      economics::StakeAccount(
          key.address().value(),
          utils::Amount::fromRawUnits(static_cast<std::int64_t>(
              core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS))));

  const core::LedgerRecord record =
      node::CanonicalSlashingTransition::buildEvidenceRecord(evidence,
                                                             kTimestamp + 9);
  node::CanonicalSlashingTransition::applyEvidenceRecords(
      {record}, 2, kTimestamp + 9, history,
      config::NetworkParameters::developmentLocal(),
      crypto::CryptoPolicy::developmentPolicy(), provider, ledger, validators,
      staking);

  require(ledger.size() == 1 &&
              ledger.validatorIsTombstoned(key.address().value()),
          "Proposer equivocation must create one tombstone penalty.");
  const core::ValidatorRegistryEntry *entry =
      validators.entryForAddress(key.address().value());
  require(entry != nullptr && entry->exited(),
          "Equivocating proposer must be removed from consensus.");
  const economics::StakeAccount *stakeAccount =
      staking.accountFor(key.address().value());
  require(stakeAccount != nullptr && stakeAccount->tombstoned() &&
              stakeAccount->slashedAmount().rawUnits() == 100000,
          "Equivocating proposer must be tombstoned and partially slashed.");
}

} // namespace

int main() {
  try {
    testAppliesVerifiedEvidenceOnce();
    testAppliesProposerEquivocationAsTombstoneAndStakeSlash();
    std::cout << "Canonical slashing transition tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Canonical slashing transition tests FAILED: " << error.what()
              << "\n";
    return 1;
  }
}
