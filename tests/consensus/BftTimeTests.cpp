#include "../common/TestFramework.hpp"
#include "consensus/BftTime.hpp"

#include "consensus/QuorumCertificate.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "consensus/ValidatorVoteBuilder.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "economics/ValidationWorkRecord.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using namespace nodo;
using nodo::test::require;

core::LedgerRecord record(const std::string &suffix) {
  return core::LedgerRecord::fromValidationWorkRecord(
      economics::ValidationWorkRecord(
          "bft-time-test", 1, economics::ValidationWorkType::VALIDATE_BLOCK,
          economics::ValidationWorkResult::ACCEPTED,
          "bft-time-target-" + suffix, suffix, 1, 100),
      100);
}

struct Fixture {
  core::Block parent;
  core::ValidatorRegistry validators;
  core::ValidatorSetHistory history;
  consensus::QuorumCertificate certificate;
  crypto::Bls12381SignatureProvider provider;
  crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
};

Fixture fixture(const std::array<std::uint64_t, 4> &weights,
                const std::array<std::int64_t, 4> &times) {
  const core::Block genesis =
      core::Block::createGenesisBlock({record("genesis")}, 100);
  Fixture result{core::Block(1, genesis.hash(), {record("parent")}, 200,
                             std::string(64, 'a'), std::string(64, 'b')),
                 {}, {}, {}, {}, crypto::CryptoPolicy::developmentPolicy()};
  std::vector<consensus::ValidatorVoteRecord> votes;
  for (std::size_t i = 0; i < weights.size(); ++i) {
    const auto key = crypto::KeyPair::createDeterministicBls12381KeyPair(
        "bft-time-key-" + std::to_string(i));
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(key.publicKey()).value();
    require(result.validators.registerValidator(core::ValidatorRegistrationRecord(
                address, key.publicKey(), weights[i],
                "bft-time-validator-" + std::to_string(i), 100),
                weights[i] * core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
                address)
                .accepted(),
            "validator fixture registration must succeed");
    votes.push_back(consensus::ValidatorVoteRecord::createVote(
        address, key.publicKey(), key.privateKeyForSigningOnly(),
        result.parent.index(), result.parent.hash(), result.parent.previousHash(),
        1, consensus::ValidatorVoteDecision::PRECOMMIT, "NONE", times[i],
        result.provider));
  }
  const auto built = consensus::QuorumCertificateBuilder::buildFromVotes(
      result.parent.index(), result.parent.hash(), result.parent.previousHash(),
      1, votes, result.validators, result.policy, result.provider);
  require(built.certified(), "signed parent QC must certify");
  result.certificate = built.certificate();
  require(result.history.recordSet(result.parent.index(), result.validators),
          "historical validator set must be recorded");
  return result;
}

void testGenesisTimeAndOverflow() {
  const core::Block genesis =
      core::Block::createGenesisBlock({record("genesis")}, 100);
  const crypto::Bls12381SignatureProvider provider;
  const auto policy = crypto::CryptoPolicy::developmentPolicy();
  require(consensus::BftTime::expectedChildTime(
              genesis, 10, nullptr, nullptr, policy, provider) == 110,
          "height 1 must use genesis time plus target seconds");
  require(!consensus::BftTime::expectedChildTime(
               genesis, 0, nullptr, nullptr, policy, provider),
          "zero target seconds must fail closed");
  const core::Block nearLimit = core::Block::createGenesisBlock(
      {record("limit")}, std::numeric_limits<std::int64_t>::max() - 1);
  require(!consensus::BftTime::expectedChildTime(
               nearLimit, 2, nullptr, nullptr, policy, provider),
          "timestamp overflow must fail closed");
}

void testWeightedMedianAndLowerTie() {
  const auto weighted = fixture({1, 1, 2, 1}, {211, 212, 213, 214});
  require(consensus::BftTime::expectedChildTime(
              weighted.parent, 10, &weighted.certificate,
              &weighted.history, weighted.policy, weighted.provider) == 213,
          "historical voting weight must determine the median");
  auto reorderedVotes = weighted.certificate.votes();
  std::reverse(reorderedVotes.begin(), reorderedVotes.end());
  const consensus::QuorumCertificate reordered(
      weighted.certificate.blockIndex(), weighted.certificate.blockHash(),
      weighted.certificate.previousHash(), weighted.certificate.round(),
      weighted.certificate.requiredVotingWeight(),
      weighted.certificate.totalVotingWeight(),
      weighted.certificate.signedVotingWeight(),
      weighted.certificate.validatorSetRoot(), reorderedVotes);
  require(consensus::BftTime::expectedChildTime(
              weighted.parent, 10, &reordered, &weighted.history,
              weighted.policy, weighted.provider) == 213,
          "the median must not depend on QC vote insertion order");
  const auto tied = fixture({1, 1, 1, 1}, {211, 212, 213, 214});
  require(consensus::BftTime::expectedChildTime(
              tied.parent, 10, &tied.certificate, &tied.history,
              tied.policy, tied.provider) == 212,
          "even-weight median must select the lower timestamp");
  require(consensus::BftTime::expectedChildTime(
              tied.parent, 20, &tied.certificate, &tied.history,
              tied.policy, tied.provider) == 220,
          "parent time plus target is the minimum");
}

void testRejectsMissingOrWrongCertificateAndPastVotes() {
  const auto good = fixture({1, 1, 1, 1}, {211, 212, 213, 214});
  require(!consensus::BftTime::expectedChildTime(
               good.parent, 10, nullptr, &good.history,
               good.policy, good.provider),
          "non-genesis time requires a parent QC");
  const core::ValidatorSetHistory missingHistory;
  require(!consensus::BftTime::expectedChildTime(
               good.parent, 10, &good.certificate, &missingHistory,
               good.policy, good.provider),
          "time calculation must not use an absent historical set");
  const auto other = fixture({1, 1, 1, 1}, {211, 212, 213, 214});
  const core::Block differentParent(1, good.parent.previousHash(),
                                    {record("different")}, 200);
  require(!consensus::BftTime::expectedChildTime(
               differentParent, 10, &other.certificate, &other.history,
               other.policy, other.provider),
          "a valid QC for another parent must not determine child time");
  const auto pastVote = fixture({1, 1, 1, 1}, {199, 212, 213, 214});
  require(!consensus::BftTime::expectedChildTime(
               pastVote.parent, 10, &pastVote.certificate,
               &pastVote.history, pastVote.policy, pastVote.provider),
          "a PRECOMMIT timestamp before its block must invalidate the time proof");
  const consensus::FinalizedBlockRecord pastRecord(
      pastVote.parent.index(), pastVote.parent.hash(),
      pastVote.parent.previousHash(), 1, 215, pastVote.certificate);
  require(!pastRecord.matchesBlock(pastVote.parent),
          "imported finality must reject votes timestamped before the block");
  core::Blockchain chain;
  chain.addGenesisBlock(
      core::Block::createGenesisBlock({record("genesis")}, 100));
  consensus::BlockFinalizationRegistry finality;
  const auto outcome = consensus::BlockFinalizer::finalizeBlock(
      chain, pastVote.parent, pastVote.certificate, pastVote.validators,
      finality, pastVote.policy, pastVote.provider, 215);
  require(!outcome.finalized() && chain.latestBlock().index() == 0,
          "finalization must reject a validly signed QC with a past vote");
  const auto wrongSet = fixture({1, 1, 2, 1}, {211, 212, 213, 214});
  require(!consensus::BftTime::expectedChildTime(
               good.parent, 10, &good.certificate, &wrongSet.history,
               good.policy, good.provider),
          "a QC cannot be reweighted under another historical validator set");
}

void testLocalPrecommitCannotPrecedeBlockTime() {
  const auto sample = fixture({1, 1, 1, 1}, {211, 212, 213, 214});
  const auto key = crypto::KeyPair::createDeterministicBls12381KeyPair(
      "bft-time-key-0");
  const crypto::Signer signer(key, sample.provider);
  bool rejected = false;
  try {
    (void)consensus::ValidatorVoteBuilder::buildPrecommit(
        sample.validators, sample.parent, 1, 199, signer);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "local signer must refuse a PRECOMMIT before block time");
  const auto valid = consensus::ValidatorVoteBuilder::buildPrecommit(
      sample.validators, sample.parent, 1, 200, signer);
  require(valid.createdAt() == 200,
          "a PRECOMMIT at the block time remains valid");
}

} // namespace

int main() {
  try {
    testGenesisTimeAndOverflow();
    testWeightedMedianAndLowerTie();
    testRejectsMissingOrWrongCertificateAndPastVotes();
    testLocalPrecommitCannotPrecedeBlockTime();
    std::cout << "BFT time tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "BFT time tests failed: " << error.what() << '\n';
    return 1;
  }
}
