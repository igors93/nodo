#include "../../common/HistoryChainFixture.hpp"
#include "../../common/TestFramework.hpp"

#include "config/HistoryParameters.hpp"
#include "core/StateRootCalculator.hpp"
#include "crypto/KeyPair.hpp"
#include "node/FastSyncSnapshotVerifier.hpp"
#include "node/PersistentBlockStateSync.hpp"
#include "node/ProtocolDomainCodec.hpp"
#include "node/history/FullProtocolStateSnapshot.hpp"
#include "node/history/HistoryStore.hpp"
#include "utils/Amount.hpp"

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

FullProtocolStateSnapshot loadSnapshot(const HistoryChainFixture &chain,
                                       std::uint64_t height) {
  const auto snapshot = HistoryStore(chain.directory())
                            .loadSnapshot(height, parameters().maxSnapshotBytes());
  require(snapshot.has_value(), "snapshot must exist");
  return *snapshot;
}

FinalizedStateCheckpoint loadCheckpoint(const HistoryChainFixture &chain,
                                        std::uint64_t height) {
  const auto checkpoint = HistoryStore(chain.directory()).loadCheckpoint(height);
  require(checkpoint.has_value(), "checkpoint must exist");
  return *checkpoint;
}

// Rebuilds the inner state with a different account set but keeps the
// claimed (blockHash, stateRoot): a classic snapshot-poisoning attempt.
FastSyncSnapshot withAccounts(const FastSyncSnapshot &state,
                              std::vector<core::AccountState> accounts) {
  core::AccountStateView view;
  for (const auto &account : accounts) {
    require(view.putAccount(account), "poisoned account must be well formed");
  }
  return FastSyncSnapshot(state.genesisConfigId(), state.chainId(),
                          state.networkName(), state.blockHeight(),
                          state.blockHash(), state.stateRoot(),
                          core::StateRootCalculator::calculateAccountStateRoot(view),
                          state.protocolDomainDigest(), view.accounts(),
                          state.createdAt(), state.protocolDomains());
}

void requireRejected(const CheckpointVerificationResult &result,
                     CheckpointVerificationStatus expected,
                     const std::string &what) {
  require(result.status() == expected,
          what + ": got " + checkpointVerificationStatusToString(result.status()) +
              " (" + result.reason() + ")");
}

void testRoundTrip(const HistoryChainFixture &chain) {
  const FullProtocolStateSnapshot snapshot = loadSnapshot(chain, 16);
  const std::vector<unsigned char> encoded = snapshot.encode();
  const FullProtocolStateSnapshot decoded =
      FullProtocolStateSnapshot::decode(encoded, parameters().maxSnapshotBytes());
  require(decoded.encode() == encoded, "snapshot must round-trip exactly");
  require(decoded.digest(parameters().snapshotChunkBytes()) ==
              loadCheckpoint(chain, 16).fields().snapshotDigest,
          "snapshot digest must match the checkpoint");
  require(decoded.state().verifiesProtocolStateRoot(),
          "an honest snapshot recomputes its state root");

  const ProtocolReplayState replay = decoded.toReplayState();
  require(replay.stateRoot == chain.artifact(16).block().stateRoot(),
          "restored state root equals the finalized header");
  require(replay.validatorSetHistory.hasSet(16) &&
              replay.validatorSetHistory.hasSet(17) &&
              replay.validatorSetHistory.firstRecordedHeight() ==
                  FullProtocolStateSnapshot::consensusWindowStart(16),
          "restored validator window covers the evidence age");
  require(core::StateRootCalculator::calculateProtocolStateRoot(
              replay.accounts, protocolExecutionDomains(replay.execution)) ==
              replay.stateRoot,
          "re-encoding the restored domains reproduces the state root");

  bool rejected = false;
  try {
    (void)FullProtocolStateSnapshot::decode(encoded, encoded.size() - 1);
  } catch (const std::exception &) {
    rejected = true;
  }
  require(rejected, "a snapshot above the size cap is refused before decoding");
}

void testPoisonedSnapshotsAreRejected(const HistoryChainFixture &chain) {
  const config::GenesisConfig genesis = HistoryChainFixture::genesis();
  const FinalizedStateCheckpoint checkpoint = loadCheckpoint(chain, 16);
  const FullProtocolStateSnapshot honest = loadSnapshot(chain, 16);
  require(FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
              honest, checkpoint, genesis, parameters())
              .isAccepted(),
          "honest snapshot verifies");

  // 1. Fabricated balance: account root is self-consistent, state root lies.
  std::vector<core::AccountState> accounts = honest.state().accounts();
  require(!accounts.empty(), "fixture has accounts");
  accounts.front() = core::AccountState(
      accounts.front().address(),
      accounts.front().balance() + utils::Amount::fromRawUnits(1),
      accounts.front().nonce());
  const FastSyncSnapshot poisonedState = withAccounts(honest.state(), accounts);
  require(poisonedState.isValid() && !poisonedState.verifiesProtocolStateRoot(),
          "a forged balance passes structure but not the state root");
  requireRejected(FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
                      FullProtocolStateSnapshot(poisonedState,
                                                honest.consensusWindow()),
                      checkpoint, genesis, parameters()),
                  CheckpointVerificationStatus::STATE_MISMATCH,
                  "forged balance");

  // 2. Extra account minted out of thin air.
  accounts = honest.state().accounts();
  accounts.emplace_back(
      crypto::KeyPair::createDeterministicEd25519KeyPair("minted-from-nothing")
          .address()
          .value(),
      utils::Amount::fromNodo(1000000), 0);
  bool extraRejected = true;
  try {
    extraRejected = !FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
                         FullProtocolStateSnapshot(
                             withAccounts(honest.state(), accounts),
                             honest.consensusWindow()),
                         checkpoint, genesis, parameters())
                         .isAccepted();
  } catch (const std::exception &) {
    extraRejected = true;
  }
  require(extraRejected, "an injected account is rejected");

  // 3. Tampered domain payload (supply inflated).
  std::map<std::string, std::string> domains = honest.state().protocolDomains();
  domains["supply"] = SupplyDomainCodec::encode(
      SupplyDomainCodec::decode(domains.at("supply")) +
      utils::Amount::fromNodo(1));
  const FastSyncSnapshot tamperedDomains(
      honest.state().genesisConfigId(), honest.state().chainId(),
      honest.state().networkName(), honest.state().blockHeight(),
      honest.state().blockHash(), honest.state().stateRoot(),
      honest.state().accountRoot(), honest.state().protocolDomainDigest(),
      honest.state().accounts(), honest.state().createdAt(), domains);
  requireRejected(FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
                      FullProtocolStateSnapshot(tamperedDomains,
                                                honest.consensusWindow()),
                      checkpoint, genesis, parameters()),
                  CheckpointVerificationStatus::STATE_MISMATCH,
                  "inflated supply domain");

  // 4. Validator window that no longer covers the evidence age.
  const core::ValidatorSetHistory &window = honest.consensusWindow();
  const core::ValidatorSetHistory shortened =
      window.windowFrom(window.firstRecordedHeight() + 1);
  requireRejected(FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
                      FullProtocolStateSnapshot(honest.state(), shortened),
                      checkpoint, genesis, parameters()),
                  CheckpointVerificationStatus::CONSENSUS_CONTEXT_MISMATCH,
                  "shortened validator window");

  // 5. Snapshot from another checkpoint.
  requireRejected(FullProtocolStateSnapshotVerifier::verifyAgainstCheckpoint(
                      loadSnapshot(chain, 8), checkpoint, genesis, parameters()),
                  CheckpointVerificationStatus::BLOCK_MISMATCH,
                  "snapshot of another height");
}

void testLegacyFastSyncVerificationRecomputesStateRoot(
    const HistoryChainFixture &chain) {
  // Regression: verifyAgainstManifest used to accept any accounts whose own
  // account root matched, as long as a peer-supplied manifest repeated the
  // genuine block hash and state root.
  const FullProtocolStateSnapshot honest = loadSnapshot(chain, 16);
  std::vector<core::AccountState> accounts = honest.state().accounts();
  accounts.front() = core::AccountState(
      accounts.front().address(),
      accounts.front().balance() + utils::Amount::fromNodo(5),
      accounts.front().nonce());
  const FastSyncSnapshot poisoned = withAccounts(honest.state(), accounts);
  const PersistentSnapshotSyncManifest manifest(
      "malicious-peer", poisoned.blockHeight(), poisoned.blockHash(),
      poisoned.stateRoot(), poisoned.digest(), poisoned.createdAt());
  const FastSyncSnapshotVerificationResult result =
      FastSyncSnapshotVerifier::verifyAgainstManifest(
          poisoned, HistoryChainFixture::genesis(), manifest);
  require(!result.accepted() &&
              result.status() ==
                  FastSyncSnapshotVerificationStatus::STATE_ROOT_MISMATCH,
          "poisoned fast-sync snapshot must be rejected by state root");

  const PersistentSnapshotSyncManifest honestManifest(
      "honest-peer", honest.state().blockHeight(), honest.state().blockHash(),
      honest.state().stateRoot(), honest.state().digest(),
      honest.state().createdAt());
  require(FastSyncSnapshotVerifier::verifyAgainstManifest(
              honest.state(), HistoryChainFixture::genesis(), honestManifest)
              .accepted(),
          "honest fast-sync snapshot still verifies");
}

void testSnapshotsAreDeterministic(const HistoryChainFixture &chain) {
  // Two independent encodings of the same finalized state are identical, so
  // every honest node derives the same snapshot digest.
  const FullProtocolStateSnapshot first = loadSnapshot(chain, 8);
  const FullProtocolStateSnapshot rebuilt(first.state(), first.consensusWindow());
  require(rebuilt.encode() == first.encode() &&
              rebuilt.consensusContextDigest() == first.consensusContextDigest(),
          "snapshot encoding is deterministic");
}

} // namespace

int main() {
  try {
    HistoryChainFixture chain("snapshot-tests");
    chain.produce(16);
    testRoundTrip(chain);
    testPoisonedSnapshotsAreRejected(chain);
    testLegacyFastSyncVerificationRecomputesStateRoot(chain);
    testSnapshotsAreDeterministic(chain);
    std::cout << "State snapshot tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "State snapshot tests failed: " << error.what() << "\n";
    return 1;
  }
}
