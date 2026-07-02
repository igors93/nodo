#include "consensus/SlashingEvidence.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"

namespace {

nodo::crypto::PublicKey testValidatorPublicKey() {
  return nodo::crypto::PublicKey(nodo::crypto::CryptoAlgorithm::BLS12_381,
                                 std::string(96, 'a'));
}

nodo::crypto::SignatureBundle testSignatureBundle() {
  nodo::crypto::SignatureBundle bundle;
  bundle.addSignature(nodo::crypto::Signature(
      nodo::crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
      nodo::crypto::SigningDomain::VALIDATOR_VOTE,
      nodo::crypto::CryptoAlgorithm::BLS12_381, testValidatorPublicKey(),
      std::string(96, 'b'), 100));
  return bundle;
}

nodo::consensus::ValidatorVoteRecord
makeVote(const std::string &blockHash, const std::string &previousHash,
         nodo::consensus::ValidatorVoteDecision decision) {
  return nodo::consensus::ValidatorVoteRecord(
      "validator-alpha", testValidatorPublicKey(), 7, blockHash, previousHash,
      2, decision, "reason-hash-alpha", 100, testSignatureBundle());
}

} // namespace

#include "storage/AtomicFile.hpp"
#include "storage/SlashingEvidenceStore.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main() {
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() /
      "nodo_slashing_evidence_store_test";

  std::filesystem::remove_all(directory);

  const auto first =
      makeVote("block-hash-a", "previous-hash",
               nodo::consensus::ValidatorVoteDecision::PRECOMMIT);

  const auto second =
      makeVote("block-hash-b", "previous-hash",
               nodo::consensus::ValidatorVoteDecision::PRECOMMIT);

  const nodo::consensus::DoubleVoteEvidence evidence(first, second, 200);
  const auto record = evidence.toRecord();

  nodo::storage::SlashingEvidenceStore store(directory);
  store.persist(evidence);
  store.persist(evidence);

  assert(store.contains(record.evidenceId()));
  assert(store.count() == 1);

  const auto loaded = store.load(record.evidenceId());
  assert(loaded.serialize() == evidence.serialize());

  const auto all = store.loadAll();
  assert(all.size() == 1);

  const std::string wrongEvidenceId = std::string(64, 'f');
  nodo::storage::AtomicFile::writeTextFile(
      directory / (wrongEvidenceId + ".evidence"), evidence.serialize());

  bool rejectedMismatchedFile = false;
  try {
    (void)store.load(wrongEvidenceId);
  } catch (const std::runtime_error &) {
    rejectedMismatchedFile = true;
  }
  assert(rejectedMismatchedFile);

  const nodo::consensus::ProposerEquivocationEvidence proposerEvidence(
      "SignedBlockProposalMessage{schema=NODO_BLOCK_PROPOSAL_V1;blockHash="
      "proposal-hash-a}\nBlock{hash=proposal-hash-a}",
      "SignedBlockProposalMessage{schema=NODO_BLOCK_PROPOSAL_V1;blockHash="
      "proposal-hash-b}\nBlock{hash=proposal-hash-b}",
      "validator-alpha", 7, 2, "proposal-hash-a", "proposal-hash-b", 300);
  store.persist(proposerEvidence);
  assert(store.contains(proposerEvidence.evidenceId()));
  assert(store.loadAllProposerEquivocation().size() == 1);
  assert(store.loadProposerEquivocation(proposerEvidence.evidenceId())
             .serialize() == proposerEvidence.serialize());
  assert(store.erase(proposerEvidence.evidenceId()));

  assert(store.erase(record.evidenceId()));
  assert(!store.contains(record.evidenceId()));
  assert(store.erase(record.evidenceId()));

  std::filesystem::remove_all(directory);

  std::cout << "slashing evidence store tests passed\n";
  return 0;
}
