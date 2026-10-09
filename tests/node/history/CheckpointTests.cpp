#include "../../common/HistoryChainFixture.hpp"
#include "../../common/TestFramework.hpp"

#include "config/HistoryParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/history/CheckpointService.hpp"
#include "node/history/FinalizedStateCheckpoint.hpp"
#include "node/history/FullProtocolStateSnapshot.hpp"
#include "node/history/HistoryStore.hpp"

#include <iostream>

using nodo::test::HistoryChainFixture;
using nodo::test::require;
using namespace nodo;
using namespace nodo::node;

namespace {

const config::HistoryParameters &parameters() {
  static const config::HistoryParameters params =
      config::HistoryParameters::developmentLocal();
  return params;
}

FinalizedStateCheckpoint load(const HistoryChainFixture &chain,
                              std::uint64_t height) {
  const auto checkpoint = HistoryStore(chain.directory()).loadCheckpoint(height);
  require(checkpoint.has_value(),
          "checkpoint " + std::to_string(height) + " must exist");
  return *checkpoint;
}

FinalizedStateCheckpoint withFields(const FinalizedStateCheckpoint &original,
                                    void (*mutate)(FinalizedStateCheckpointFields &)) {
  FinalizedStateCheckpointFields fields = original.fields();
  mutate(fields);
  return FinalizedStateCheckpoint(fields, original.quorumCertificate());
}

void requireStatus(const CheckpointVerificationResult &result,
                   CheckpointVerificationStatus expected,
                   const std::string &what) {
  require(result.status() == expected,
          what + ": expected " + checkpointVerificationStatusToString(expected) +
              " but got " + checkpointVerificationStatusToString(result.status()) +
              " (" + result.reason() + ")");
}

void testParametersAreValidAndBound() {
  require(parameters().isValid(), parameters().validationError());
  require(config::HistoryParameters::developmentSoak().isValid(),
          "soak history parameters must be valid");
  require(config::HistoryParameters::testnetCandidate().isValid(),
          config::HistoryParameters::testnetCandidate().validationError());
  require(parameters().deterministicId() !=
              config::HistoryParameters::testnetCandidate().deterministicId(),
          "different networks must not share a parameter id");

  bool mainnetRejected = false;
  try {
    (void)config::HistoryParameters::forNetwork("mainnet");
  } catch (const std::invalid_argument &) {
    mainnetRejected = true;
  }
  require(mainnetRejected, "mainnet has no history profile yet");

  config::HistoryParameterValues values = parameters().values();
  values.archiveRewardShareBasisPoints = 9000;
  require(!config::HistoryParameters(values).isValid(),
          "archival reward share above the protocol bound must be rejected");
  values = parameters().values();
  values.checkpointIntervalBlocks = 6; // not a multiple of the segment size
  require(!config::HistoryParameters(values).isValid(),
          "checkpoints must seal whole archive segments");
  values = parameters().values();
  values.archiveScarcityTiers = {{3, 10000}, {2, 9000}, {1, 25000}};
  require(!config::HistoryParameters(values).isValid(),
          "scarcity multipliers may never fall below 1.0x");
  values = parameters().values();
  values.weakSubjectivitySeconds = 30 * 24 * 60 * 60;
  require(!config::HistoryParameters(values).isValid(),
          "weak-subjectivity window above ADR 0004 must be rejected");

  // Schedule arithmetic.
  require(parameters().isCheckpointHeight(8) &&
              !parameters().isCheckpointHeight(7) &&
              !parameters().isCheckpointHeight(0),
          "checkpoint schedule");
  require(parameters().segmentIndexForHeight(1) == 0 &&
              parameters().segmentIndexForHeight(4) == 0 &&
              parameters().segmentIndexForHeight(5) == 1 &&
              parameters().segmentFirstHeight(2) == 9 &&
              parameters().segmentLastHeight(2) == 12 &&
              parameters().sealedSegmentCount(16) == 4,
          "segment arithmetic");
  require(parameters().scarcityMultiplierBasisPoints(5) == 10000 &&
              parameters().scarcityMultiplierBasisPoints(2) == 15000 &&
              parameters().scarcityMultiplierBasisPoints(1) == 25000 &&
              parameters().scarcityMultiplierBasisPoints(0) == 0,
          "scarcity tiers");
}

void testCheckpointsAreCreatedOnFinalization(HistoryChainFixture &chain) {
  const HistoryStore store(chain.directory());
  require(store.checkpointHeights() == std::vector<std::uint64_t>({8, 16}),
          "finalization must create checkpoints exactly at heights 8 and 16");
  require(store.snapshotHeights() == std::vector<std::uint64_t>({8, 16}),
          "every checkpoint must have its snapshot");
  require(store.contiguousSegmentCommitmentCount() == 4,
          "checkpoint 16 seals archive segments 0..3");

  const FinalizedStateCheckpoint first = load(chain, 8);
  const FinalizedStateCheckpoint second = load(chain, 16);
  require(first.fields().previousCheckpointHeight == 0 &&
              first.fields().previousCheckpointId.empty(),
          "the first checkpoint has no predecessor");
  require(second.fields().previousCheckpointHeight == 8 &&
              second.fields().previousCheckpointId == first.checkpointId(),
          "checkpoints form a chain");
  const auto block16 = chain.artifact(16).block();
  require(second.fields().blockHash == block16.hash() &&
              second.fields().stateRoot == block16.stateRoot() &&
              second.fields().blockTimestamp == block16.timestamp(),
          "the checkpoint commits the finalized header");
}

void testSerialization(const HistoryChainFixture &chain) {
  const FinalizedStateCheckpoint checkpoint = load(chain, 16);
  const std::vector<unsigned char> encoded = checkpoint.encode();
  const FinalizedStateCheckpoint decoded = FinalizedStateCheckpoint::decode(encoded);
  require(decoded.encode() == encoded, "checkpoint must round-trip exactly");
  require(decoded.checkpointId() == checkpoint.checkpointId(),
          "checkpoint id must survive serialization");

  std::vector<unsigned char> trailing = encoded;
  trailing.push_back(0);
  std::vector<unsigned char> truncated(encoded.begin(), encoded.end() - 1);
  std::vector<unsigned char> flipped = encoded;
  flipped[6] ^= 0x01; // inside the file schema tag
  for (const auto &bad : {trailing, truncated, flipped}) {
    bool rejected = false;
    try {
      (void)FinalizedStateCheckpoint::decode(bad);
    } catch (const std::exception &) {
      rejected = true;
    }
    require(rejected, "malformed checkpoint bytes must be rejected");
  }
  bool oversized = false;
  try {
    (void)FinalizedStateCheckpoint::decode(std::vector<unsigned char>(
        FinalizedStateCheckpoint::kMaxEncodedBytes + 1, 0));
  } catch (const std::exception &) {
    oversized = true;
  }
  require(oversized, "oversized checkpoint input must be rejected before parsing");

  // The id excludes the QC so honest nodes with different vote subsets agree.
  const FinalizedStateCheckpoint other(checkpoint.fields(),
                                       checkpoint.quorumCertificate());
  require(other.checkpointId() == checkpoint.checkpointId(),
          "checkpoint id depends only on committed fields");
}

void testValidation(const HistoryChainFixture &chain) {
  const config::GenesisConfig genesis = HistoryChainFixture::genesis();
  const FinalizedStateCheckpoint checkpoint = load(chain, 16);
  requireStatus(FinalizedStateCheckpointVerifier::verifyIdentity(
                    checkpoint, genesis, parameters()),
                CheckpointVerificationStatus::ACCEPTED, "valid checkpoint");

  requireStatus(FinalizedStateCheckpointVerifier::verifyIdentity(
                    withFields(checkpoint,
                               [](FinalizedStateCheckpointFields &f) {
                                 f.chainId = "another-chain";
                               }),
                    genesis, parameters()),
                CheckpointVerificationStatus::IDENTITY_MISMATCH,
                "foreign chain");
  requireStatus(FinalizedStateCheckpointVerifier::verifyIdentity(
                    withFields(checkpoint,
                               [](FinalizedStateCheckpointFields &f) {
                                 f.historyParametersId = std::string(64, 'a');
                               }),
                    genesis, parameters()),
                CheckpointVerificationStatus::PARAMETERS_MISMATCH,
                "different parameters");
  requireStatus(FinalizedStateCheckpointVerifier::verifyIdentity(
                    withFields(checkpoint,
                               [](FinalizedStateCheckpointFields &f) {
                                 f.previousCheckpointHeight = 4;
                               }),
                    genesis, parameters()),
                CheckpointVerificationStatus::LINK_MISMATCH,
                "off-schedule predecessor");
  requireStatus(FinalizedStateCheckpointVerifier::verifyIdentity(
                    withFields(checkpoint,
                               [](FinalizedStateCheckpointFields &f) {
                                 f.archiveSealedSegmentCount = 3;
                               }),
                    genesis, parameters()),
                CheckpointVerificationStatus::MALFORMED,
                "archive index missing a sealed segment");
  require(!withFields(checkpoint,
                      [](FinalizedStateCheckpointFields &f) {
                        f.blockHash = std::string(64, 'b');
                      })
               .isStructurallyValid(),
          "a QC for a different block makes the checkpoint invalid");

  const FinalizedStateCheckpoint first = load(chain, 8);
  requireStatus(FinalizedStateCheckpointVerifier::verifyLink(checkpoint, first),
                CheckpointVerificationStatus::ACCEPTED, "valid link");
  requireStatus(FinalizedStateCheckpointVerifier::verifyLink(
                    withFields(checkpoint,
                               [](FinalizedStateCheckpointFields &f) {
                                 f.previousCheckpointId = std::string(64, 'c');
                               }),
                    first),
                CheckpointVerificationStatus::LINK_MISMATCH, "broken link");
}

void testQuorumCertificate(const HistoryChainFixture &chain) {
  const FinalizedStateCheckpoint checkpoint = load(chain, 16);
  const auto snapshot = HistoryStore(chain.directory())
                            .loadSnapshot(16, parameters().maxSnapshotBytes());
  require(snapshot.has_value(), "snapshot 16 must load");
  const crypto::ProtocolCryptoContext crypto =
      crypto::ProtocolCryptoContext::fromNetworkName("localnet");
  const core::ValidatorRegistry &set = snapshot->consensusWindow().setAt(16);
  requireStatus(FinalizedStateCheckpointVerifier::verifyCertificate(
                    checkpoint, set, crypto),
                CheckpointVerificationStatus::ACCEPTED,
                "QC signed by the committed set");

  core::ValidatorRegistry empty;
  requireStatus(FinalizedStateCheckpointVerifier::verifyCertificate(
                    checkpoint, empty, crypto),
                CheckpointVerificationStatus::VALIDATOR_SET_MISMATCH,
                "QC checked against another set");

  // A QC from another height can never certify this checkpoint.
  const FinalizedStateCheckpoint mixed(checkpoint.fields(),
                                       load(chain, 8).quorumCertificate());
  require(!mixed.isStructurallyValid(), "QC for another block is rejected");
}

void testStoreConflictsAreKept(const HistoryChainFixture &chain) {
  const HistoryStore store(chain.directory());
  const FinalizedStateCheckpoint checkpoint = load(chain, 16);
  require(store.saveCheckpoint(checkpoint).status ==
              HistoryWriteStatus::ALREADY_PRESENT,
          "re-saving the same checkpoint is idempotent");
  const FinalizedStateCheckpoint conflicting =
      withFields(checkpoint, [](FinalizedStateCheckpointFields &f) {
        f.stateRoot = std::string(64, 'd');
      });
  require(conflicting.isStructurallyValid(), "conflict fixture must be valid");
  require(store.saveCheckpoint(conflicting).status ==
              HistoryWriteStatus::CONFLICT,
          "a different checkpoint at a stored height is a conflict");
  require(store.conflictCount() == 1, "the conflicting checkpoint is preserved");
  require(load(chain, 16).checkpointId() == checkpoint.checkpointId(),
          "the original checkpoint is never overwritten");
}

void testStoredCheckpointsVerify(const HistoryChainFixture &chain) {
  const config::GenesisConfig genesis = HistoryChainFixture::genesis();
  for (const std::uint64_t height : {8ULL, 16ULL}) {
    requireStatus(CheckpointService::verifyStored(chain.directory(), genesis,
                                                  height),
                  CheckpointVerificationStatus::ACCEPTED,
                  "stored checkpoint " + std::to_string(height));
  }
}

void testStoredCheckpointRequiresLocalCommitments(const HistoryChainFixture &chain) {
  const HistoryStore store(chain.directory());
  const config::GenesisConfig genesis = HistoryChainFixture::genesis();
  const auto previous = store.checkpointPath(8);
  const auto previousAway = previous.string() + ".held";
  std::filesystem::rename(previous, previousAway);
  requireStatus(CheckpointService::verifyStored(chain.directory(), genesis, 16),
                CheckpointVerificationStatus::LINK_MISMATCH,
                "a missing previous checkpoint cannot verify a local checkpoint");
  std::filesystem::rename(previousAway, previous);

  const auto segment = store.segmentCommitmentPath(0);
  const auto segmentAway = segment.string() + ".held";
  std::filesystem::rename(segment, segmentAway);
  requireStatus(CheckpointService::verifyStored(chain.directory(), genesis, 16),
                CheckpointVerificationStatus::MALFORMED,
                "a missing archive commitment cannot verify a local checkpoint");
  std::filesystem::rename(segmentAway, segment);
  requireStatus(CheckpointService::verifyStored(chain.directory(), genesis, 16),
                CheckpointVerificationStatus::ACCEPTED,
                "restoring local commitments restores verification");
}

void testBackfillRecreatesIdenticalCheckpoints(const HistoryChainFixture &chain) {
  const HistoryChainFixture copy("checkpoint-backfill");
  // Rebuild from the source chain's block files into a second directory.
  const auto blocks = chain.directory().blocksDirectoryPath();
  std::filesystem::remove_all(copy.directory().blocksDirectoryPath());
  std::filesystem::copy(blocks, copy.directory().blocksDirectoryPath(),
                        std::filesystem::copy_options::recursive);
  std::filesystem::copy_file(chain.directory().manifestPath(),
                             copy.directory().manifestPath(),
                             std::filesystem::copy_options::overwrite_existing);
  const CheckpointBackfillResult backfill =
      CheckpointService::backfill(copy.directory(), HistoryChainFixture::genesis());
  require(backfill.success, "backfill must succeed: " + backfill.reason);
  require(backfill.createdCheckpoints == 2 && backfill.replayedHeight == 16,
          "backfill replays the chain and creates both checkpoints");
  for (const std::uint64_t height : {8ULL, 16ULL}) {
    require(load(copy, height).checkpointId() == load(chain, height).checkpointId(),
            "an independent replay derives the same checkpoint id");
  }
}

} // namespace

int main() {
  try {
    testParametersAreValidAndBound();
    HistoryChainFixture chain("checkpoint-tests");
    chain.produce(16);
    testCheckpointsAreCreatedOnFinalization(chain);
    testSerialization(chain);
    testValidation(chain);
    testQuorumCertificate(chain);
    testStoredCheckpointsVerify(chain);
    testStoredCheckpointRequiresLocalCommitments(chain);
    testBackfillRecreatesIdenticalCheckpoints(chain);
    testStoreConflictsAreKept(chain);

    // Checkpoint creation must never disturb the replay-based reload.
    chain.run({"node", "reload", "--data-dir", chain.path().string(),
               "--peer-id", HistoryChainFixture::peerId(), "--endpoint",
               "127.0.0.1:9911"},
              "reload after checkpoints");
    std::cout << "Checkpoint tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Checkpoint tests failed: " << error.what() << "\n";
    return 1;
  }
}
