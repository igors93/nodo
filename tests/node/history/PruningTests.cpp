#include "../../common/HistoryChainFixture.hpp"
#include "../../common/TestFramework.hpp"

#include "config/HistoryParameters.hpp"
#include "node/NodePruningManifest.hpp"
#include "node/NodePruningService.hpp"
#include "node/history/CheckpointService.hpp"
#include "node/history/HistoryPruningEngine.hpp"
#include "node/history/HistoryRetentionPolicy.hpp"
#include "node/history/HistoryStore.hpp"
#include "node/history/StorageMigration.hpp"
#include "storage/AtomicFile.hpp"
#include "storage/StorageSchemaVersion.hpp"

#include <algorithm>
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

std::size_t blockFileCount(const NodeDataDirectoryConfig &directory) {
  std::size_t count = 0;
  for (const auto &entry :
       std::filesystem::directory_iterator(directory.blocksDirectoryPath())) {
    if (entry.path().extension() == ".nodo") {
      ++count;
    }
  }
  return count;
}

// Independent copy of a prepared chain directory for destructive scenarios.
class DirectoryCopy {
public:
  DirectoryCopy(const HistoryChainFixture &source, const std::string &name)
      : m_path(std::filesystem::temp_directory_path() /
               ("nodo-history-copy-" + name)) {
    std::error_code ec;
    std::filesystem::remove_all(m_path, ec);
    std::filesystem::copy(source.path(), m_path,
                          std::filesystem::copy_options::recursive);
  }
  ~DirectoryCopy() {
    std::error_code ec;
    std::filesystem::remove_all(m_path, ec);
  }
  NodeDataDirectoryConfig directory() const {
    return NodeDataDirectoryConfig(m_path);
  }
  const std::filesystem::path &path() const { return m_path; }

private:
  std::filesystem::path m_path;
};

PruningOptions normalOptions() {
  PruningOptions options;
  options.mode = NodeStorageMode::NORMAL;
  options.retentionBlocks = parameters().minimumPruningRetentionBlocks();
  options.retainedCheckpointSnapshots = 2;
  return options;
}

void testRetentionCategories() {
  RetentionInputs inputs;
  inputs.mode = NodeStorageMode::ARCHIVE;
  inputs.tipHeight = 100000;
  inputs.verifiedCheckpointHeights = {99984, 99992};
  inputs.checkpointSnapshotHeights = {99976, 99984, 99992};
  inputs.legacyFastSyncSnapshotHeights = {99998, 99999, 100000};
  inputs.retentionBlocks = 16;
  inputs.retainedCheckpointSnapshots = 2;

  RetentionPlan archive = HistoryRetentionPolicy::plan(parameters(), inputs);
  require(!archive.blockBodyPruningAllowed &&
              archive.checkpointSnapshotsToPrune.empty() &&
              archive.legacySnapshotsToPrune.empty(),
          "archive mode never prunes");

  inputs.mode = NodeStorageMode::NORMAL;
  RetentionPlan normal = HistoryRetentionPolicy::plan(parameters(), inputs);
  require(normal.baseCheckpointHeight == 99992,
          "the newest confirmed checkpoint with a snapshot is the base");
  // Window floor 99985, replay floor 99993, but the pending epoch-1..3
  // reward settlement still needs every block of epoch 3 (86401..).
  require(normal.blockBodySafeBelowHeight == 86401,
          "reward settlement holds the whole current epoch");
  require(!normal.blockBodyPruningAllowed &&
              std::any_of(normal.blockBodyBlockers.begin(),
                          normal.blockBodyBlockers.end(),
                          [](const std::string &b) {
                            return b.find("3.17") != std::string::npos;
                          }),
          "body pruning stays blocked until checkpoint-base reload exists");
  require(normal.checkpointSnapshotsToPrune ==
              std::vector<std::uint64_t>({99976}),
          "keep the newest two checkpoint snapshots");
  require(normal.legacySnapshotsToPrune ==
              std::vector<std::uint64_t>({99998, 99999}),
          "keep only the newest legacy snapshot");
  for (const RetentionRule &rule : normal.rules) {
    if (rule.category == RetentionCategory::GENESIS_AND_IDENTITY ||
        rule.category == RetentionCategory::CHECKPOINT_COMMITMENTS ||
        rule.category == RetentionCategory::HEADERS_AND_QCS) {
      require(rule.permanent, retentionCategoryToString(rule.category) +
                                  " is permanent");
    }
  }

  inputs.checkpointBaseReloadSupported = true;
  require(HistoryRetentionPolicy::plan(parameters(), inputs)
              .blockBodyPruningAllowed,
          "with every prerequisite in place bodies may be pruned");

  inputs.earliestOpenGovernanceHeight = 50000;
  require(HistoryRetentionPolicy::plan(parameters(), inputs)
                  .blockBodySafeBelowHeight == 50000,
          "an open proposal holds its blocks");
  inputs.earliestOpenGovernanceHeight.reset();
  inputs.earliestTimelockedTreasuryHeight = 40000;
  require(HistoryRetentionPolicy::plan(parameters(), inputs)
                  .blockBodySafeBelowHeight == 40000,
          "a queued treasury action holds its blocks");
  inputs.earliestTimelockedTreasuryHeight.reset();

  config::HistoryParameterValues strict = parameters().values();
  strict.archiveMinProvenReplicasBeforePrune = 2;
  const config::HistoryParameters strictParameters(strict);
  require(strictParameters.isValid(), strictParameters.validationError());
  inputs.provenReplicasBySegment.assign(30000, 3);
  inputs.provenReplicasBySegment[5] = 1;
  require(HistoryRetentionPolicy::plan(strictParameters, inputs)
                  .blockBodySafeBelowHeight ==
              strictParameters.segmentFirstHeight(5),
          "an under-replicated segment is never pruned");
  inputs.provenReplicasBySegment.clear();
  require(HistoryRetentionPolicy::plan(strictParameters, inputs)
                  .blockBodySafeBelowHeight == 1,
          "unmeasured replication blocks pruning entirely");

  inputs.verifiedCheckpointHeights = {99999};
  inputs.checkpointSnapshotHeights = {99999};
  require(!HistoryRetentionPolicy::plan(parameters(), inputs)
               .baseCheckpointHeight.has_value(),
          "a checkpoint inside the confirmation depth is not a base");

  require(nodeStorageModeFromString("full") == NodeStorageMode::NORMAL &&
              nodeStorageModeFromString("ARCHIVE") == NodeStorageMode::ARCHIVE &&
              !nodeStorageModeFromString("prune-everything").has_value(),
          "storage mode names");
}

void testAllowList() {
  const std::string hash(64, 'a');
  for (const std::string ok :
       {std::string("history/snapshots/8.snapshot"),
        std::string("runtime/fast_sync_snapshots/12.fastsnap"),
        "blocks/block_3_" + hash + ".nodo"}) {
    require(HistoryPruningEngine::isAllowedTarget(ok), "allowed: " + ok);
  }
  for (const std::string bad :
       {std::string("../manifest.nodo"), std::string("/etc/passwd"),
        std::string("history/snapshots/../../manifest.nodo"),
        std::string("history/snapshots/08.snapshot"),
        std::string("history/checkpoints/8.checkpoint"),
        std::string("genesis.nodo"), std::string("history/snapshots/8.snapshot/x"),
        "blocks/block_3_" + std::string(63, 'a') + ".nodo",
        std::string("blocks/../genesis.nodo"), std::string("")}) {
    require(!HistoryPruningEngine::isAllowedTarget(bad), "forbidden: " + bad);
  }
}

void testSafetyKeepsEveryBlock(const HistoryChainFixture &chain) {
  const DirectoryCopy copy(chain, "safety");
  const std::size_t blocks = blockFileCount(copy.directory());
  const PruningRunResult run = HistoryPruningEngine::run(
      copy.directory(), HistoryChainFixture::genesis(), normalOptions(),
      HistoryChainFixture::kBaseTimestamp + 10000);
  require(run.status == PruningRunStatus::APPLIED,
          "pruning must apply: " + run.reason);
  require(run.plan.baseCheckpointHeight == 16,
          "checkpoint 24 is inside the confirmation depth; 16 is the base");
  const HistoryStore store(copy.directory());
  require(store.snapshotHeights() == std::vector<std::uint64_t>({16, 24}),
          "only checkpoint snapshot 8 is removed");
  require(store.checkpointHeights() == std::vector<std::uint64_t>({8, 16, 24}),
          "checkpoint records are permanent");
  require(blockFileCount(copy.directory()) == blocks,
          "no finalized block file is ever removed");
  const auto manifest = HistoryPruningEngine::loadManifest(copy.directory());
  require(manifest && manifest->prunedCheckpointSnapshots == 1 &&
              manifest->lastPrunedHeight == 0 &&
              manifest->lastFinalizedHeight == 24 &&
              manifest->lastCheckpointHeight == 24,
          "the committed manifest records exactly what happened");
  require(CheckpointService::verifyStored(copy.directory(),
                                          HistoryChainFixture::genesis(), 16)
              .isAccepted(),
          "the base checkpoint still verifies");

  const app::CommandLineResult reload = chain.cli(
      {"node", "reload", "--data-dir", copy.path().string(), "--peer-id",
       HistoryChainFixture::peerId(), "--endpoint", "127.0.0.1:9911"});
  require(reload.success(), "reload after pruning: " + reload.message());

  const PruningRunResult again = HistoryPruningEngine::run(
      copy.directory(), HistoryChainFixture::genesis(), normalOptions(),
      HistoryChainFixture::kBaseTimestamp + 10001);
  require(again.status == PruningRunStatus::NOOP, "a second run is a no-op");
}

void testCrashRecovery(const HistoryChainFixture &chain) {
  for (const PruningCrashPoint crash :
       {PruningCrashPoint::AFTER_PREPARE, PruningCrashPoint::AFTER_FIRST_DELETE,
        PruningCrashPoint::BEFORE_COMMIT}) {
    const DirectoryCopy copy(chain, "crash-" + std::to_string(
                                                  static_cast<int>(crash)));
    PruningOptions options = normalOptions();
    options.crashPoint = crash;
    const PruningRunResult crashed = HistoryPruningEngine::run(
        copy.directory(), HistoryChainFixture::genesis(), options,
        HistoryChainFixture::kBaseTimestamp + 20000);
    require(!crashed.success() &&
                crashed.reason.find("simulated crash") != std::string::npos,
            "the crash must interrupt the run");
    require(std::filesystem::exists(copy.directory().pruningJournalPath()),
            "an interrupted run leaves its journal");
    const auto beforeRecovery =
        HistoryPruningEngine::loadManifest(copy.directory());
    require(!beforeRecovery || beforeRecovery->prunedCheckpointSnapshots == 0,
            "the manifest never claims an uncommitted run");

    // Restart: the runtime loader rolls the run forward.
    const app::CommandLineResult reload = chain.cli(
        {"node", "reload", "--data-dir", copy.path().string(), "--peer-id",
         HistoryChainFixture::peerId(), "--endpoint", "127.0.0.1:9911"});
    require(reload.success(), "reload after a crash: " + reload.message());
    require(!std::filesystem::exists(copy.directory().pruningJournalPath()),
            "recovery removes the journal");
    const auto manifest = HistoryPruningEngine::loadManifest(copy.directory());
    require(manifest && manifest->prunedCheckpointSnapshots == 1,
            "recovery commits the planned manifest");
    require(HistoryStore(copy.directory()).snapshotHeights() ==
                std::vector<std::uint64_t>({16, 24}),
            "recovery finishes every planned removal");
  }
}

void testTamperedJournalIsQuarantined(const HistoryChainFixture &chain) {
  const DirectoryCopy copy(chain, "journal");
  PruningOptions options = normalOptions();
  options.crashPoint = PruningCrashPoint::AFTER_PREPARE;
  (void)HistoryPruningEngine::run(copy.directory(),
                                  HistoryChainFixture::genesis(), options,
                                  HistoryChainFixture::kBaseTimestamp + 30000);
  const auto journalPath = copy.directory().pruningJournalPath();
  std::string journal = storage::AtomicFile::readTextFile(journalPath);
  const std::string target = "history/snapshots/8.snapshot";
  const std::size_t at = journal.find(target);
  require(at != std::string::npos, "journal names the planned target");
  journal.replace(at, target.size(), "genesis.nodo");
  storage::AtomicFile::writeTextFile(journalPath, journal);

  const app::CommandLineResult reload = chain.cli(
      {"node", "reload", "--data-dir", copy.path().string(), "--peer-id",
       HistoryChainFixture::peerId(), "--endpoint", "127.0.0.1:9911"});
  require(!reload.success() &&
              reload.message().find("pruning journal rejected") !=
                  std::string::npos,
          "startup fails closed when a pruning journal is quarantined");
  require(std::filesystem::exists(copy.directory().genesisConfigPath()) &&
              HistoryStore(copy.directory()).snapshotHeights().size() == 3,
          "nothing is deleted from a quarantined journal");
  require(!std::filesystem::exists(journalPath),
          "the quarantined journal is moved aside");
}

void testLegacyLightPolicyIsNeutralized(const HistoryChainFixture &chain) {
  const DirectoryCopy copy(chain, "legacy");
  const NodeDataDirectoryReadResult runtime =
      NodeDataDirectory::loadManifest(copy.directory());
  require(runtime.loaded(), "manifest");
  const NodePruningManifest legacy(
      NodePruningConfig::lightMode(), runtime.manifest().chainId(),
      runtime.manifest().genesisConfigId(),
      runtime.manifest().latestBlockHeight(),
      runtime.manifest().latestBlockHash(),
      runtime.manifest().latestStateRoot(), 24, 24, std::string(64, 'd'), 0, 0,
      HistoryChainFixture::kBaseTimestamp);
  storage::AtomicFile::writeTextFile(copy.directory().pruningManifestPath(),
                                     legacy.toFileContents());
  const std::size_t blocks = blockFileCount(copy.directory());

  const NodePruningResult configured = NodePruningService::applyConfiguredPolicy(
      copy.directory(), runtime.manifest(), HistoryChainFixture::kBaseTimestamp + 1);
  require(configured.status() == NodePruningStatus::NOOP,
          "a legacy policy is never re-applied");
  const NodePruningResult light = NodePruningService::apply(
      copy.directory(), runtime.manifest(), NodePruningConfig::lightMode(),
      HistoryChainFixture::kBaseTimestamp + 2);
  require(light.success(), "legacy light request runs the safe engine: " +
                               light.reason());
  require(blockFileCount(copy.directory()) == blocks,
          "LIGHT pruning no longer deletes finalized block files");
}

void testExplicitMigration(const HistoryChainFixture &chain) {
  const DirectoryCopy copy(chain, "migration");
  storage::AtomicFile::writeTextFile(
      copy.directory().storageSchemaVersionPath(),
      storage::StorageSchemaVersion(
          storage::StorageSchemaVersion::nodeDataDirectorySchemaId(), 1, 1)
          .toFileContents());
  require(StorageMigration::schemaVersion(copy.directory()) == 1 &&
              !StorageMigration::supportsHistoryLayout(copy.directory()),
          "a version 1 directory is recognised");
  require(HistoryPruningEngine::run(copy.directory(),
                                    HistoryChainFixture::genesis(),
                                    normalOptions(),
                                    HistoryChainFixture::kBaseTimestamp + 1)
                  .status == PruningRunStatus::REJECTED,
          "history writes require an explicit migration");

  const NodeDataDirectoryReadResult runtime =
      NodeDataDirectory::loadManifest(copy.directory());
  require(runtime.loaded(), "a v1 directory still loads");
  const NodePruningManifest legacy(
      NodePruningConfig::fullMode(2), runtime.manifest().chainId(),
      runtime.manifest().genesisConfigId(),
      runtime.manifest().latestBlockHeight(),
      runtime.manifest().latestBlockHash(),
      runtime.manifest().latestStateRoot(), 0,
      runtime.manifest().latestBlockHeight(), std::string(64, 'd'), 0, 3,
      HistoryChainFixture::kBaseTimestamp);
  storage::AtomicFile::writeTextFile(copy.directory().pruningManifestPath(),
                                     legacy.toFileContents());

  const StorageMigrationResult migrated = StorageMigration::migrate(copy.directory());
  require(migrated.success && migrated.fromVersion == 1 &&
              migrated.toVersion == 2,
          "migration upgrades v1 to v2: " + migrated.reason);
  require(!std::filesystem::exists(copy.directory().pruningManifestPath()),
          "the legacy manifest is retired");
  const auto converted = HistoryPruningEngine::loadManifest(copy.directory());
  require(converted && converted->mode == NodeStorageMode::NORMAL &&
              converted->prunedLegacySnapshots == 3,
          "the legacy manifest is converted");
  const StorageMigrationResult again = StorageMigration::migrate(copy.directory());
  require(again.success && again.actions.empty(), "migration is idempotent");
}

} // namespace

int main() {
  try {
    testRetentionCategories();
    testAllowList();
    HistoryChainFixture chain("pruning-tests");
    chain.produce(24);
    require(HistoryStore(chain.directory()).checkpointHeights() ==
                std::vector<std::uint64_t>({8, 16, 24}),
            "fixture has three checkpoints");
    testSafetyKeepsEveryBlock(chain);
    testCrashRecovery(chain);
    testTamperedJournalIsQuarantined(chain);
    testLegacyLightPolicyIsNeutralized(chain);
    testExplicitMigration(chain);
    std::cout << "Pruning tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Pruning tests failed: " << error.what() << "\n";
    return 1;
  }
}
