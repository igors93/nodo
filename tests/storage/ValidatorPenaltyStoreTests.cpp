#include "consensus/ValidatorPenaltyApplication.hpp"
#include "storage/AtomicFile.hpp"
#include "storage/ValidatorPenaltyStore.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace nodo::consensus;
using namespace nodo::storage;

namespace {

std::string hash64(char value) { return std::string(64, value); }

SlashingEvidenceRecord makeEvidence(char value) {
  return SlashingEvidenceRecord(
      hash64(value), SlashingEvidenceType::DOUBLE_VOTE, "validator-store",
      hash64('9'), SlashingEvidenceSeverity::SLASHABLE, 1700000000);
}

} // namespace

int main() {
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() /
      "nodo_validator_penalty_store_test";

  std::filesystem::remove_all(directory);

  ValidatorPenaltyStore store(directory);
  const ValidatorPenaltyPolicy policy =
      ValidatorPenaltyPolicy::conservativeTestnetPolicy();
  const auto decision =
      ValidatorPenaltyDecision::create(makeEvidence('e'), policy, 1800000000);

  store.save(decision);
  assert(store.containsPenalty(decision.penaltyId()));
  assert(store.count() == 1);

  const auto loaded = store.load(decision.penaltyId());
  assert(loaded.isValid());
  assert(loaded.penaltyId() == decision.penaltyId());
  assert(loaded.evidenceId() == decision.evidenceId());
  assert(loaded.validatorAddress() == "validator-store");

  const auto all = store.loadAll();
  assert(all.size() == 1);
  assert(all.front().penaltyId() == decision.penaltyId());

  const std::string wrongPenaltyId = hash64('f');
  AtomicFile::writeTextFile(directory / (wrongPenaltyId + ".penalty"),
                            decision.serialize());

  bool rejectedMismatchedFile = false;
  try {
    (void)store.load(wrongPenaltyId);
  } catch (const std::runtime_error &) {
    rejectedMismatchedFile = true;
  }
  assert(rejectedMismatchedFile);

  std::filesystem::remove_all(directory);

  std::cout << "Validator penalty store tests passed.\n";
  return 0;
}
