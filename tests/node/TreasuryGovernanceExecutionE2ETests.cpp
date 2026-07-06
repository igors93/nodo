// Single-process end-to-end test (roadmap item 4.2): a treasury-spend
// governance proposal is approved, automatically queued for execution as
// part of ordinary block finalization (no manual decision step), then
// actually moves funds only once an explicit, permissionless
// GOVERNANCE_EXECUTE transaction is finalized. The resulting finalized
// artifact carries real treasury execution evidence, and both `chain audit`
// and `governance audit` must pass after a completely fresh reload — the
// same reload path a node that synced from scratch would take.
//
// This drives the real RuntimeBlockPipeline/GovernanceExecutor/
// ProtocolTransactionDomainExecutor stack with a single bootstrap validator
// (see RuntimeBlockCanonicalCommitTests.cpp for the same single-process
// pattern); it does not need a multi-validator split vote to exercise the
// 2/3 approval bar, since a lone validator's YES vote already clears it.

#include "config/NetworkParameters.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "economics/GovernanceLifecycleVerifier.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "node/ChainAuditResult.hpp"
#include "node/ChainAuditor.hpp"
#include "node/EpochTreasuryReportStore.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/FinalizedTreasuryAudit.hpp"
#include "node/GovernanceLifecycleRecordBuilder.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeMonetaryReportService.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "utils/Amount.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000LL;
const std::string kRecipient = "treasury-grant-recipient";
const std::string kFillerRecipient = "treasury-e2e-filler-recipient";

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::filesystem::path tempPath(const std::string &suffix) {
  return std::filesystem::temp_directory_path() /
         ("nodo-treasury-governance-execution-e2e-" + suffix);
}

void clean(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove_all(path, error);
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::in | std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

void writeFile(const std::filesystem::path &path, const std::string &contents) {
  std::ofstream output(path,
                       std::ios::out | std::ios::binary | std::ios::trunc);
  output << contents;
}

crypto::KeyPair validatorKey() {
  return crypto::KeyPair::createDeterministicBls12381KeyPair(
      "treasury-execution-e2e-validator");
}

crypto::KeyPair userKey() {
  return crypto::KeyPair::createDeterministicEd25519KeyPair(
      "treasury-execution-e2e-user");
}

config::GenesisConfig genesisConfig() {
  // The funded user account owns the bootstrap validator, matching how a
  // real validator separates its consensus (BLS) key from the regular
  // (Ed25519) identity that signs ordinary transactions like governance
  // votes on its behalf — the validator's own BLS key never signs mempool
  // transactions, only consensus votes and quorum certificates.
  return config::GenesisConfig(
      config::NetworkParameters::developmentLocal(), kTimestamp,
      {config::BootstrapValidatorConfig(validatorKey().publicKey(), 1, 1,
                                        "treasury-execution-e2e-validator",
                                        userKey().address().value())},
      {config::GenesisAccountConfig(userKey().address().value(),
                                    utils::Amount::fromRawUnits(1000000000000),
                                    0)},
      "treasury-execution-e2e-genesis");
}

crypto::Signer validatorSigner() {
  static const crypto::Bls12381SignatureProvider provider;
  return crypto::Signer(validatorKey(), provider);
}

crypto::Signer userSigner() {
  static const crypto::Ed25519SignatureProvider provider;
  return crypto::Signer(userKey(), provider);
}

p2p::PeerInfo localPeer() {
  return p2p::PeerInfo("treasury-execution-e2e-peer", "127.0.0.1:29996",
                       "nodo/test", 0, kTimestamp);
}

node::NodeRuntime startRuntime() {
  const node::NodeRuntimeStartResult started =
      node::NodeRuntimeFactory::startFromGenesis(
          node::NodeRuntimeConfig(genesisConfig(), localPeer(), 16));
  require(started.started(), "Runtime must start from genesis.");
  return started.runtime();
}

core::Transaction signedTransfer(const std::string &to, std::uint64_t nonce,
                                 std::int64_t timestamp) {
  return core::TransactionBuilder::buildSignedTransfer(
      core::TransactionBuildRequest(to, utils::Amount::fromRawUnits(1000),
                                    utils::Amount::fromRawUnits(100), nonce,
                                    timestamp),
      userSigner(), genesisConfig().networkParameters().chainId());
}

void admit(node::NodeRuntime &runtime, const core::Transaction &tx,
           std::int64_t timestamp,
           crypto::SecurityContext securityContext =
               crypto::SecurityContext::USER_TRANSACTION) {
  const auto result = runtime.mutableMempool().admitTransaction(
      tx, crypto::CryptoPolicy::developmentPolicy(), securityContext,
      timestamp);
  require(result.accepted(),
          "Transaction must enter the mempool: " + result.reason());
}

node::RuntimeBlockPipelineResult
produceBlock(node::NodeRuntime &runtime, std::int64_t timestamp,
             const node::NodeDataDirectoryConfig &directoryConfig) {
  const node::RuntimeBlockPipelineResult result =
      node::RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
          runtime, node::RuntimeBlockPipelineConfig(10, 1, 1, timestamp),
          validatorSigner(), &directoryConfig);
  require(result.finalized(), "Block must finalize: " + result.reason());
  return result;
}

// Refreshes the persisted epoch monetary/treasury reports from the current
// chain tip, exactly as `nodo block produce` does after every block
// (CommandLineInterface::executeProduceBlock). `chain audit`/`governance
// audit` require these reports to exist and agree with the finalized chain;
// this test drives blocks directly through RuntimeBlockPipeline rather than
// through that CLI command, so it must refresh them itself before auditing.
void persistMonetaryAndTreasuryReports(
    node::NodeRuntime &runtime,
    const node::NodeDataDirectoryConfig &directoryConfig,
    std::uint64_t latestBlockIndex) {
  const utils::Amount genesisSupply =
      node::MonetaryFirewall::genesisSupply(genesisConfig());
  const economics::MonetaryPolicy reportPolicy =
      economics::MonetaryPolicy::localnetDefault(
          genesisConfig().networkParameters().chainId(), genesisSupply);
  const auto monetaryResult =
      node::RuntimeMonetaryReportService::buildAndPersist(
          reportPolicy, runtime.supplyState().finalizedDeltas(), 0,
          directoryConfig.epochMonetaryReportPath());
  require(monetaryResult.succeeded(),
          "Failed to persist epoch monetary report: " +
              monetaryResult.reason());

  const node::FinalizedBlockArtifact persistedArtifact =
      node::FinalizedBlockArtifactCodec::readBlockArtifactFile(
          node::FinalizedBlockStore::blockFilePath(directoryConfig,
                                                   latestBlockIndex));
  const node::FinalizedTreasuryAuditResult treasuryAudit =
      node::FinalizedTreasuryAudit::auditArtifacts(0, {persistedArtifact});
  require(treasuryAudit.passed(),
          "Failed to rebuild epoch treasury report: " + treasuryAudit.reason());
  node::EpochTreasuryReportStore::write(
      directoryConfig.epochTreasuryReportPath(), treasuryAudit.rebuiltReport());
}

// Drives a treasury-spend proposal from submission through approval,
// automatic queueing, and explicit execution across 5 real finalized blocks,
// persisting every one of them to directoryConfig. Returns the proposal id.
std::string runTreasuryExecutionScenario(
    node::NodeRuntime &runtime,
    const node::NodeDataDirectoryConfig &directoryConfig,
    utils::Amount spendAmount) {
  // Block 1: submit the treasury-spend proposal. Block finalization advances
  // governance state one height ahead of the block just finalized (so that
  // mempool admission for the next block sees an up-to-date view), so this
  // proposal (createdHeight=1, votingStartDelayBlocks=1) is already open for
  // voting as of height 2 the moment this block finalizes.
  const core::Transaction proposal =
      core::TransactionBuilder::buildSignedGovernanceProposal(
          core::GovernanceProposalPayload::treasurySpend(
              "Fund contributor", "Grant for ecosystem work", kRecipient,
              spendAmount.rawUnits(), /*votingStartDelayBlocks=*/1,
              /*votingPeriodBlocks=*/3)
              .serialize(),
          utils::Amount::fromRawUnits(100), 1, kTimestamp + 1, userSigner(),
          genesisConfig().networkParameters().chainId());
  const std::string proposalId = proposal.id();
  admit(runtime, proposal, kTimestamp + 1);
  produceBlock(runtime, kTimestamp + 2, directoryConfig);
  require(runtime.governanceExecutor().proposalStatus(proposalId) ==
              node::GovernanceProposalStatus::ACTIVE,
          "Proposal must already be open for voting once its start height is "
          "reached by the block that decided it.");

  // Block 2: the sole validator's owner votes YES on its behalf, clearing
  // the 2/3 approval bar. The validator's own BLS consensus key never signs
  // mempool transactions — only its owner (an ordinary funded account) can.
  const core::GovernanceVotePayload votePayload(
      proposalId, validatorKey().address().value(),
      core::GovernanceVoteChoice::YES);
  const core::Transaction vote =
      core::TransactionBuilder::buildSignedGovernanceVote(
          proposalId, votePayload, utils::Amount::fromRawUnits(100), 2,
          kTimestamp + 3, userSigner(),
          genesisConfig().networkParameters().chainId());
  admit(runtime, vote, kTimestamp + 3);
  produceBlock(runtime, kTimestamp + 4, directoryConfig);

  // Block 3: filler — the voting period (ending at height 4) has not yet
  // fully elapsed.
  admit(runtime, signedTransfer(kFillerRecipient, 3, kTimestamp + 5),
        kTimestamp + 5);
  produceBlock(runtime, kTimestamp + 6, directoryConfig);
  require(runtime.governanceExecutor().proposalStatus(proposalId) ==
              node::GovernanceProposalStatus::ACTIVE,
          "Proposal must remain active through the end of its voting period.");

  // Block 4: filler — the voting period has now elapsed. The decision closes
  // automatically as part of ordinary block finalization (no manual action),
  // and the approved treasury spend is queued rather than executed.
  admit(runtime, signedTransfer(kFillerRecipient, 4, kTimestamp + 7),
        kTimestamp + 7);
  produceBlock(runtime, kTimestamp + 8, directoryConfig);
  require(runtime.governanceExecutor().proposalStatus(proposalId) ==
              node::GovernanceProposalStatus::QUEUED_FOR_EXECUTION,
          "A treasury spend approved by the required majority must be queued "
          "for explicit execution, not executed automatically.");

  // Block 5: anyone (here, the same funded user account) submits the
  // permissionless GOVERNANCE_EXECUTE transaction that actually moves funds.
  const core::Transaction execute =
      core::TransactionBuilder::buildSignedGovernanceExecute(
          proposalId, utils::Amount::fromRawUnits(100), 5, kTimestamp + 9,
          userSigner(), genesisConfig().networkParameters().chainId());
  admit(runtime, execute, kTimestamp + 9);
  produceBlock(runtime, kTimestamp + 10, directoryConfig);
  require(runtime.governanceExecutor().proposalStatus(proposalId) ==
              node::GovernanceProposalStatus::EXECUTED,
          "Governance execute transaction must move the proposal to EXECUTED.");

  persistMonetaryAndTreasuryReports(runtime, directoryConfig,
                                    runtime.blockchain().latestBlock().index());

  return proposalId;
}

void testTreasurySpendApprovedExecutedAndAuditedAfterFreshReload() {
  const std::filesystem::path path = tempPath("main");
  clean(path);
  const node::NodeDataDirectoryConfig directoryConfig(path);
  require(node::NodeDataDirectory::initialize(directoryConfig, genesisConfig(),
                                              localPeer(), kTimestamp + 1)
              .initialized(),
          "Data directory must initialize.");

  node::NodeRuntime runtime = startRuntime();
  const utils::Amount spendAmount = utils::Amount::fromRawUnits(15);

  const utils::Amount treasuryBalanceBeforeExecution =
      runtime
          .cachedAccountStateAtTip(
              static_cast<std::int64_t>(runtime.effectiveMinimumFeeRawUnits()))
          .accountOrDefault(node::ProtectionTreasury::TREASURY_ADDRESS)
          .balance();

  const std::string proposalId =
      runTreasuryExecutionScenario(runtime, directoryConfig, spendAmount);

  const core::AccountStateView finalAccounts = runtime.cachedAccountStateAtTip(
      static_cast<std::int64_t>(runtime.effectiveMinimumFeeRawUnits()));
  require(finalAccounts.accountOrDefault(kRecipient).balance() == spendAmount,
          "Recipient must be credited with exactly the approved spend amount.");
  require(
      (treasuryBalanceBeforeExecution +
       finalAccounts
           .accountOrDefault(node::ProtectionTreasury::TREASURY_ADDRESS)
           .balance()) >= spendAmount,
      "Treasury must have accumulated enough fee revenue to cover the spend.");

  // A brand-new NodeRuntime reloaded purely from the persisted finalized
  // artifacts on disk is indistinguishable from a node that synced from
  // scratch: RuntimeStateLoader replays the canonical chain the same way
  // regardless of how the artifacts arrived.
  const node::RuntimeStateLoadResult reloaded =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesisConfig(), localPeer());
  require(reloaded.loaded(), "Fresh reload must succeed: " + reloaded.reason());
  require(reloaded.runtime().governanceExecutor().proposalStatus(proposalId) ==
              node::GovernanceProposalStatus::EXECUTED,
          "Reloaded runtime must independently replay the same decision and "
          "execution.");
  require(reloaded.runtime()
                  .cachedAccountStateAtTip(static_cast<std::int64_t>(
                      reloaded.runtime().effectiveMinimumFeeRawUnits()))
                  .accountOrDefault(kRecipient)
                  .balance() == spendAmount,
          "Reloaded runtime must independently replay the treasury balance "
          "change.");

  // `nodo chain audit`: FinalizedTreasuryAudit (run inside auditLoadedRuntime)
  // must accept the real treasury evidence now embedded in the finalized
  // artifact for the execution block.
  const node::ChainAuditResult chainAudit =
      node::ChainAuditor::auditLoadedRuntime(
          reloaded, directoryConfig.epochMonetaryReportPath(),
          directoryConfig.epochTreasuryReportPath());
  require(chainAudit.passed(), "Chain audit must pass on a freshly reloaded "
                               "node: " +
                                   chainAudit.toHumanReadableString());

  // `nodo governance audit`: the treasury-spend proposal's decision and
  // execution must independently re-verify from the reloaded
  // GovernanceExecutor state alone, with no separately persisted lifecycle
  // store required.
  const node::GovernanceExecutor &governance =
      reloaded.runtime().governanceExecutor();
  const node::GovernanceExecutor::GovernanceProposalSnapshot snapshot =
      governance.proposalSnapshot(proposalId);
  const std::optional<economics::GovernanceLifecycleRecord> decided =
      node::GovernanceLifecycleRecordBuilder::buildDecided(snapshot);
  require(decided.has_value(), "Reloaded state must produce a decided "
                               "lifecycle record for the executed treasury "
                               "proposal.");
  const economics::GovernanceLifecycleRecord executedLifecycle =
      node::GovernanceLifecycleRecordBuilder::buildExecuted(
          *decided, snapshot.executedAtHeight);
  require(economics::GovernanceLifecycleVerifier::verify(executedLifecycle)
              .verified(),
          "Governance audit must independently verify the executed treasury "
          "lifecycle record after a fresh reload.");

  clean(path);
}

// Ticket 4.2 item 4: execution evidence that does not correspond to a
// genuinely valid, verifiable governance decision must be rejected at the
// finalization/artifact-validation layer, not silently accepted on reload.
// This tampers with the declared lifecycle state embedded in an otherwise
// real, correctly-produced finalized artifact and confirms a fresh reload
// refuses to load it.
void testTamperedExecutionEvidenceRejectedAtReload() {
  const std::filesystem::path path = tempPath("tampered");
  clean(path);
  const node::NodeDataDirectoryConfig directoryConfig(path);
  require(node::NodeDataDirectory::initialize(directoryConfig, genesisConfig(),
                                              localPeer(), kTimestamp + 1)
              .initialized(),
          "Data directory must initialize.");

  node::NodeRuntime runtime = startRuntime();
  const utils::Amount spendAmount = utils::Amount::fromRawUnits(15);
  runTreasuryExecutionScenario(runtime, directoryConfig, spendAmount);

  const std::filesystem::path executionBlockPath =
      node::FinalizedBlockStore::blockFilePath(directoryConfig,
                                               runtime.blockchain().size() - 1);
  const std::string original = readFile(executionBlockPath);
  require(original.find(
              "governanceLifecycle.declaredCurrentState=DECIDED_APPROVED") !=
              std::string::npos,
          "Execution block artifact must embed the approving lifecycle "
          "decision verbatim before it can be tampered with.");

  std::string tampered = original;
  const std::string needle =
      "governanceLifecycle.declaredCurrentState=DECIDED_APPROVED";
  const std::string replacement =
      "governanceLifecycle.declaredCurrentState=DECIDED_REJECTED";
  tampered.replace(tampered.find(needle), needle.size(), replacement);
  writeFile(executionBlockPath, tampered);

  const node::RuntimeStateLoadResult reloaded =
      node::RuntimeStateLoader::loadFromDataDirectory(
          directoryConfig, genesisConfig(), localPeer());
  require(!reloaded.loaded(),
          "A finalized artifact whose treasury evidence declares a lifecycle "
          "decision inconsistent with its own transition history must be "
          "rejected at reload, not silently accepted.");

  clean(path);
}

} // namespace

int main() {
  try {
    testTreasurySpendApprovedExecutedAndAuditedAfterFreshReload();
    testTamperedExecutionEvidenceRejectedAtReload();
    std::cout << "Treasury governance execution end-to-end tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Treasury governance execution end-to-end tests FAILED: "
              << error.what() << '\n';
    return 1;
  }
}
