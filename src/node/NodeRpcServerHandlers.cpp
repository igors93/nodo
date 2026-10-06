#include "utils/Logger.hpp"
#include "node/NodeRpcServer.hpp"

#include "node/HealthCheckService.hpp"
#include "node/NodeMetrics.hpp"
#include "node/PrometheusExporter.hpp"

#include "core/LedgerRecord.hpp"
#include "core/StateRootCalculator.hpp"
#include "core/Transaction.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/Address.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "mempool/Mempool.hpp"
#include "node/LightClientService.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "node/WebSocketFrameCodec.hpp"
#include "p2p/EncryptedPeerTransport.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "utils/Amount.hpp"
#include "utils/JsonText.hpp"

#include <asio.hpp>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

using utils::jsonString;

const std::string kRpcSubmitSchemaId = "NODO_RPC_TRANSACTION_SUBMISSION_V1";
const std::set<std::string> kRpcSubmitFields = {"transaction"};

bool parseUint64Strict(const std::string &value, std::uint64_t &out) {
  if (value.empty()) {
    return false;
  }

  for (const char c : value) {
    if (c < '0' || c > '9') {
      return false;
    }
  }

  try {
    std::size_t parsedCharacters = 0;
    const unsigned long long parsed = std::stoull(value, &parsedCharacters);
    if (parsedCharacters != value.size()) {
      return false;
    }
    out = static_cast<std::uint64_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

std::string jsonStakePosition(const StakePositionView &position) {
  std::ostringstream oss;
  oss << "{"
      << "\"positionId\":" << jsonString(position.positionId)
      << ",\"ownerAddress\":" << jsonString(position.ownerAddress)
      << ",\"validatorAddress\":" << jsonString(position.validatorAddress)
      << ",\"status\":"
      << jsonString(stakePositionStatusToString(position.status))
      << ",\"activeRawUnits\":" << position.activeAmount.rawUnits()
      << ",\"pendingActivationRawUnits\":"
      << position.pendingActivationAmount.rawUnits()
      << ",\"pendingUnbondingRawUnits\":"
      << position.pendingUnbondingAmount.rawUnits()
      << ",\"withdrawnRawUnits\":" << position.withdrawnAmount.rawUnits()
      << ",\"slashedRawUnits\":" << position.slashedAmount.rawUnits()
      << ",\"rewardsPendingRawUnits\":" << position.rewardsPending.rawUnits()
      << ",\"lockHeight\":" << position.lockHeight
      << ",\"activationHeight\":" << position.activationHeight
      << ",\"unbondingStartHeight\":" << position.unbondingStartHeight
      << ",\"withdrawableHeight\":" << position.withdrawableHeight << "}";
  return oss.str();
}

core::Transaction parseSignedTransactionSubmission(const std::string &body) {
  const serialization::KeyValueFileDocument fields =
      serialization::KeyValueFileCodec::parse(body, kRpcSubmitSchemaId);

  fields.requireOnlyFields(kRpcSubmitFields);

  const std::string transactionText = fields.requireField("transaction");

  return core::Transaction::deserialize(transactionText);
}

} // namespace

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------

std::string NodeRpcServer::handleStatus() const {
  const auto &chain = m_runtime.blockchain();
  const auto &mgr = m_runtime.consensusRoundManager();
  const auto &peers = m_runtime.peerManager();
  const auto &mempool = m_runtime.mempool();

  const std::uint64_t height = chain.empty() ? 0 : chain.latestBlock().index();
  const std::uint64_t round = mgr.currentState().round();
  const bool running = m_runtime.isRunning();
  const std::uint64_t finalizedHeight =
      m_runtime.finalizationRegistry().highestFinalizedHeight();
  const auto *encryptedTransport =
      m_gossip != nullptr ? dynamic_cast<const p2p::EncryptedPeerTransport *>(
                                &m_gossip->transport())
                          : nullptr;
  const std::size_t authenticatedSessionCount =
      encryptedTransport != nullptr ? encryptedTransport->sessionCount() : 0;
  const std::string latestHash =
      chain.empty() ? "" : chain.latestBlock().hash();

  std::ostringstream oss;
  oss << "{"
      << "\"height\":" << height << ",\"finalizedHeight\":" << finalizedHeight
      << ",\"latestHash\":" << jsonString(latestHash) << ",\"round\":" << round
      << ",\"peerCount\":" << peers.size()
      << ",\"authenticatedPeerCount\":" << authenticatedSessionCount
      << ",\"encryptedSessionCount\":" << authenticatedSessionCount
      << ",\"mempoolSize\":" << mempool.size()
      << ",\"running\":" << (running ? "true" : "false") << "}";
  return oss.str();
}

std::string NodeRpcServer::handleBlock(const std::string &heightStr) const {
  std::uint64_t height = 0;
  if (!parseUint64Strict(heightStr, height)) {
    return jsonError("Invalid height: " + heightStr);
  }

  const auto &blocks = m_runtime.blockchain().blocks();
  if (height >= blocks.size()) {
    return jsonError("Block not found at height " + heightStr);
  }

  const core::Block &block = blocks[static_cast<std::size_t>(height)];
  std::ostringstream oss;
  oss << "{"
      << "\"height\":" << block.index()
      << ",\"hash\":" << jsonString(block.hash())
      << ",\"previousHash\":" << jsonString(block.previousHash())
      << ",\"timestamp\":" << block.timestamp()
      << ",\"recordCount\":" << block.records().size() << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleBlockByHash(const std::string &blockHash) const {
  if (blockHash.empty()) {
    return jsonError("Missing block hash");
  }

  for (const auto &block : m_runtime.blockchain().blocks()) {
    if (block.hash() == blockHash) {
      std::ostringstream oss;
      oss << "{"
          << "\"height\":" << block.index()
          << ",\"hash\":" << jsonString(block.hash())
          << ",\"previousHash\":" << jsonString(block.previousHash())
          << ",\"timestamp\":" << block.timestamp()
          << ",\"recordCount\":" << block.records().size() << "}";
      return oss.str();
    }
  }
  return jsonError("Block not found for hash " + blockHash);
}

std::string NodeRpcServer::handleTx(const std::string &txId) const {
  for (const auto &block : m_runtime.blockchain().blocks()) {
    for (const auto &record : block.records()) {
      if (record.id() == txId || record.sourceId() == txId) {
        std::ostringstream oss;
        oss << "{"
            << "\"id\":" << jsonString(record.id())
            << ",\"sourceId\":" << jsonString(record.sourceId()) << ",\"type\":"
            << jsonString(core::ledgerRecordTypeToString(record.type()))
            << ",\"blockHeight\":" << block.index()
            << ",\"timestamp\":" << record.timestamp() << "}";
        return oss.str();
      }
    }
  }
  return jsonError("Transaction not found: " + txId);
}

std::string NodeRpcServer::handleAccount(const std::string &address) const {
  const std::uint64_t minimumFeeRaw = m_runtime.effectiveMinimumFeeRawUnits();
  if (minimumFeeRaw >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return jsonError("Network minimum fee exceeds supported range.");
  }
  const core::AccountState account =
      m_runtime
          .cachedAccountStateAtTip(static_cast<std::int64_t>(minimumFeeRaw))
          .accountOrDefault(address);

  std::ostringstream oss;
  oss << "{"
      << "\"address\":" << jsonString(address)
      << ",\"balance\":" << account.balance().rawUnits()
      << ",\"nonce\":" << account.nonce() << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleAccountProof(const std::string &address) const {
  const std::uint64_t minimumFeeRaw = m_runtime.effectiveMinimumFeeRawUnits();
  if (minimumFeeRaw >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return jsonError("Network minimum fee exceeds supported range.");
  }
  const core::AccountStateView &view = m_runtime.cachedAccountStateAtTip(
      static_cast<std::int64_t>(minimumFeeRaw));

  if (!view.hasAccount(address)) {
    return jsonError("Address not present in current account state: " +
                     address);
  }

  const core::MerkleProof proof =
      core::StateRootCalculator::accountInclusionProof(view, address);
  if (!proof.isValid()) {
    return jsonError("Failed to build inclusion proof for address: " + address);
  }

  std::ostringstream oss;
  oss << "{"
      << "\"address\":" << jsonString(address)
      << ",\"accountStateRoot\":" << jsonString(proof.reconstructRoot())
      << ",\"leafHash\":" << jsonString(proof.leafHash()) << ",\"path\":[";
  const auto &steps = proof.steps();
  for (std::size_t i = 0; i < steps.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << "{\"siblingHash\":" << jsonString(steps[i].siblingHash)
        << ",\"siblingIsLeft\":" << (steps[i].siblingIsLeft ? "true" : "false")
        << "}";
  }
  oss << "]}";
  return oss.str();
}

std::string NodeRpcServer::handleValidators() const {
  const std::vector<std::string> addresses =
      m_runtime.validatorRegistry().activeValidatorAddresses();

  std::ostringstream oss;
  oss << "{\"validators\":[";
  for (std::size_t i = 0; i < addresses.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << jsonString(addresses[i]);
  }
  oss << "],\"count\":" << addresses.size() << ",\"totalConsensusWeight\":"
      << m_runtime.validatorRegistry().totalConsensusWeight()
      << ",\"validatorSetRoot\":"
      << jsonString(m_runtime.validatorRegistry().validatorSetRoot())
      << ",\"validatorDetails\":[";
  for (std::size_t i = 0; i < addresses.size(); ++i) {
    if (i > 0)
      oss << ",";
    const core::ValidatorRegistryEntry *entry =
        m_runtime.validatorRegistry().entryForAddress(addresses[i]);
    oss << "{"
        << "\"address\":" << jsonString(addresses[i]) << ",\"status\":"
        << jsonString(
               entry == nullptr
                   ? "UNKNOWN"
                   : core::validatorRegistrationStatusToString(entry->status()))
        << ",\"eligible\":"
        << (entry != nullptr && entry->eligibleForConsensus() ? "true"
                                                              : "false")
        << ",\"stakeRawUnits\":"
        << (entry == nullptr ? 0 : entry->stakeAmount())
        << ",\"consensusWeight\":"
        << m_runtime.validatorRegistry().consensusWeightFor(addresses[i])
        << "}";
  }
  oss << "]}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakeStatus(const std::string &validatorAddress) const {
  const auto account =
      m_runtime.stakingRegistry().accountOrDefault(validatorAddress);
  const core::ValidatorRegistryEntry *entry =
      m_runtime.validatorRegistry().entryForAddress(validatorAddress);
  std::ostringstream oss;
  oss << "{"
      << "\"validatorAddress\":" << jsonString(validatorAddress)
      << ",\"registered\":" << (entry == nullptr ? "false" : "true")
      << ",\"validatorStatus\":"
      << jsonString(
             entry == nullptr
                 ? "UNKNOWN"
                 : core::validatorRegistrationStatusToString(entry->status()))
      << ",\"bondedRawUnits\":" << account.bondedAmount().rawUnits()
      << ",\"activeRawUnits\":"
      << m_runtime.stakingRegistry().activeStakeFor(validatorAddress).rawUnits()
      << ",\"slashedRawUnits\":" << account.slashedAmount().rawUnits()
      << ",\"jailed\":" << (account.jailed() ? "true" : "false")
      << ",\"tombstoned\":" << (account.tombstoned() ? "true" : "false")
      << ",\"consensusWeight\":"
      << m_runtime.validatorRegistry().consensusWeightFor(validatorAddress)
      << ",\"positions\":[";
  bool first = true;
  for (const auto &position : m_runtime.stakingRegistry().positions()) {
    if (position.validatorAddress != validatorAddress)
      continue;
    if (!first)
      oss << ",";
    oss << jsonStakePosition(position);
    first = false;
  }
  oss << "]}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakePositions(const std::string &ownerAddress) const {
  const std::vector<StakePositionView> positions =
      ownerAddress.empty()
          ? m_runtime.stakingRegistry().positions()
          : m_runtime.stakingRegistry().positionsForOwner(ownerAddress);
  std::ostringstream oss;
  oss << "{\"ownerAddress\":" << jsonString(ownerAddress) << ",\"positions\":[";
  for (std::size_t i = 0; i < positions.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << jsonStakePosition(positions[i]);
  }
  oss << "],\"count\":" << positions.size() << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakePosition(const std::string &positionId) const {
  for (const auto &position : m_runtime.stakingRegistry().positions()) {
    if (position.positionId == positionId) {
      return jsonStakePosition(position);
    }
  }
  return jsonError("Stake position not found: " + positionId);
}

std::string NodeRpcServer::handleStakePendingUnbonding(
    const std::string &validatorAddress) const {
  std::ostringstream oss;
  oss << "{\"validatorAddress\":" << jsonString(validatorAddress)
      << ",\"pendingUnbonding\":[";
  bool first = true;
  std::int64_t total = 0;
  for (const auto &position : m_runtime.stakingRegistry().positions()) {
    if (position.validatorAddress != validatorAddress ||
        !position.pendingUnbondingAmount.isPositive()) {
      continue;
    }
    if (!first)
      oss << ",";
    oss << jsonStakePosition(position);
    total += position.pendingUnbondingAmount.rawUnits();
    first = false;
  }
  oss << "],\"totalPendingUnbondingRawUnits\":" << total << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakeValidator(const std::string &validatorAddress) const {
  return handleStakeStatus(validatorAddress);
}

std::string NodeRpcServer::handleStakeAudit() const {
  std::ostringstream oss;
  oss << "{"
      << "\"valid\":"
      << (m_runtime.stakingRegistry().isValid() ? "true" : "false")
      << ",\"accountCount\":" << m_runtime.stakingRegistry().accounts().size()
      << ",\"positionCount\":" << m_runtime.stakingRegistry().positions().size()
      << ",\"lifecycleRecordCount\":"
      << m_runtime.stakingRegistry().lifecycleRecords().size() << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleStakeMutationInfo(const std::string &operation) const {
  std::string transactionType = "STAKE_DEPOSIT";
  if (operation == "topUp" || operation == "top-up")
    transactionType = "STAKE_TOP_UP";
  if (operation == "unlock")
    transactionType = "STAKE_UNLOCK";
  if (operation == "withdraw")
    transactionType = "STAKE_WITHDRAW";
  std::ostringstream oss;
  oss << "{"
      << "\"operation\":" << jsonString(operation)
      << ",\"requiresSignedTransaction\":true"
      << ",\"transactionType\":" << jsonString(transactionType)
      << ",\"submitEndpoint\":\"/submit\""
      << "}";
  return oss.str();
}

std::string NodeRpcServer::handlePeers() const {
  const auto peers = m_runtime.peerManager().peers();

  std::ostringstream oss;
  oss << "{\"peers\":[";
  for (std::size_t i = 0; i < peers.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << "{"
        << "\"peerId\":" << jsonString(peers[i].peerId())
        << ",\"endpoint\":" << jsonString(peers[i].endpoint())
        << ",\"latestKnownHeight\":" << peers[i].latestKnownHeight() << "}";
  }
  oss << "],\"count\":" << peers.size() << "}";
  return oss.str();
}

std::string NodeRpcServer::handleMempool() const {
  const auto &pool = m_runtime.mempool();
  const auto pending = pool.transactionsForBlock(20);

  std::ostringstream oss;
  oss << "{\"size\":" << pool.size() << ",\"transactions\":[";
  for (std::size_t i = 0; i < pending.size(); ++i) {
    if (i > 0)
      oss << ",";
    const auto &tx = pending[i];
    oss << "{"
        << "\"id\":" << jsonString(tx.id())
        << ",\"from\":" << jsonString(tx.fromAddress())
        << ",\"to\":" << jsonString(tx.toAddress())
        << ",\"amount\":" << tx.amount().rawUnits()
        << ",\"fee\":" << tx.fee().rawUnits() << "}";
  }
  oss << "]}";
  return oss.str();
}

std::string NodeRpcServer::handleEstimateFee(const std::string &urgency) const {
  const std::uint64_t minimum = m_runtime.effectiveMinimumFeeRawUnits();
  std::uint64_t multiplier = 1;
  if (urgency == "MEDIUM" || urgency == "medium") {
    multiplier = 2;
  } else if (urgency == "HIGH" || urgency == "high") {
    multiplier = 4;
  } else if (!urgency.empty() && urgency != "LOW" && urgency != "low") {
    return jsonError("Invalid fee urgency: " + urgency);
  }

  const std::uint64_t estimated =
      minimum > (std::numeric_limits<std::uint64_t>::max() / multiplier)
          ? std::numeric_limits<std::uint64_t>::max()
          : minimum * multiplier;

  std::ostringstream oss;
  oss << "{"
      << "\"urgency\":" << jsonString(urgency.empty() ? "LOW" : urgency)
      << ",\"minimumFeeRawUnits\":" << minimum
      << ",\"estimatedFeeRawUnits\":" << estimated << "}";
  return oss.str();
}

std::string NodeRpcServer::handleChainInfo() const {
  const config::GenesisConfig &genesis = m_runtime.config().genesisConfig();
  const config::NetworkParameters &network = genesis.networkParameters();
  const auto &chain = m_runtime.blockchain();
  const std::uint64_t height = chain.empty() ? 0 : chain.latestBlock().index();
  const std::string latestHash =
      chain.empty() ? "" : chain.latestBlock().hash();

  std::ostringstream oss;
  oss << "{"
      << "\"networkName\":" << jsonString(network.networkName())
      << ",\"chainId\":" << jsonString(network.chainId())
      << ",\"genesisConfigId\":" << jsonString(genesis.deterministicId())
      << ",\"height\":" << height << ",\"finalizedHeight\":"
      << m_runtime.finalizationRegistry().highestFinalizedHeight()
      << ",\"latestHash\":" << jsonString(latestHash)
      << ",\"minimumFeeRawUnits\":" << m_runtime.effectiveMinimumFeeRawUnits()
      << ",\"validatorCount\":" << m_runtime.validatorRegistry().activeCount()
      << "}";
  return oss.str();
}

std::string NodeRpcServer::handleJsonRpcMethods() const {
  const std::vector<std::string> methods =
      m_jsonRpcDispatcher.registeredMethods();
  std::ostringstream oss;
  oss << "{\"methods\":[";
  for (std::size_t i = 0; i < methods.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << jsonString(methods[i]);
  }
  oss << "],\"count\":" << methods.size() << "}";
  return oss.str();
}

std::string NodeRpcServer::handleGovernanceStatus() const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  const std::uint64_t nextHeight = m_runtime.blockchain().size();
  std::ostringstream oss;
  oss << "{"
      << "\"activeProposalCount\":" << governance.activeProposalCount()
      << ",\"approvedProposalCount\":" << governance.approvedProposalCount()
      << ",\"executableProposalCount\":"
      << governance.executableProposalCount(nextHeight)
      << ",\"executedProposalCount\":" << governance.executedProposalCount()
      << ",\"effectiveMinimumFeeRawUnits\":"
      << m_runtime.effectiveMinimumFeeRawUnits() << "}";
  return oss.str();
}

std::string NodeRpcServer::handleGovernanceProposals() const {
  const std::vector<std::string> ids =
      m_runtime.governanceExecutor().proposalIds();
  std::ostringstream oss;
  oss << "{\"proposals\":[";
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << jsonString(ids[i]);
  }
  oss << "],\"count\":" << ids.size() << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleGovernanceProposal(const std::string &proposalId) const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  if (!governance.hasProposal(proposalId)) {
    return jsonError("Governance proposal not found: " + proposalId);
  }
  std::ostringstream oss;
  oss << "{"
      << "\"proposalId\":" << jsonString(proposalId)
      << ",\"detail\":" << jsonString(governance.proposalDetail(proposalId))
      << ",\"status\":"
      << jsonString(governanceProposalStatusToString(
             governance.proposalStatus(proposalId)))
      << ",\"votingStartHeight\":"
      << governance.proposalVotingStartHeight(proposalId)
      << ",\"votingEndHeight\":"
      << governance.proposalVotingEndHeight(proposalId) << ",\"approved\":"
      << (governance.proposalApproved(proposalId) ? "true" : "false")
      << ",\"executed\":"
      << (governance.hasBeenExecuted(proposalId) ? "true" : "false") << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleGovernanceVotes(const std::string &proposalId) const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  if (!governance.hasProposal(proposalId)) {
    return jsonError("Governance proposal not found: " + proposalId);
  }
  std::ostringstream oss;
  oss << "{"
      << "\"proposalId\":" << jsonString(proposalId)
      << ",\"votes\":" << jsonString(governance.proposalVotes(proposalId))
      << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleGovernanceTally(const std::string &proposalId) const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  if (!governance.hasProposal(proposalId)) {
    return jsonError("Governance proposal not found: " + proposalId);
  }
  const GovernanceTallySnapshot tally = governance.tallyForProposal(proposalId);
  std::ostringstream oss;
  oss << "{"
      << "\"proposalId\":" << jsonString(proposalId)
      << ",\"yesWeight\":" << tally.yesWeight()
      << ",\"noWeight\":" << tally.noWeight()
      << ",\"abstainWeight\":" << tally.abstainWeight()
      << ",\"participatingWeight\":" << tally.participatingWeight()
      << ",\"totalEligibleWeight\":" << tally.totalEligibleWeight()
      << ",\"quorumMet\":" << (tally.quorumMet() ? "true" : "false")
      << ",\"approvalThresholdMet\":"
      << (tally.approvalThresholdMet() ? "true" : "false")
      << ",\"approved\":" << (tally.approved() ? "true" : "false") << "}";
  return oss.str();
}

std::string
NodeRpcServer::handleGovernanceDecision(const std::string &proposalId) const {
  return handleGovernanceProposal(proposalId);
}

std::string
NodeRpcServer::handleGovernanceExecution(const std::string &proposalId) const {
  const GovernanceExecutor &governance = m_runtime.governanceExecutor();
  if (!governance.hasProposal(proposalId)) {
    return jsonError("Governance proposal not found: " + proposalId);
  }
  std::ostringstream oss;
  oss << "{"
      << "\"proposalId\":" << jsonString(proposalId) << ",\"status\":"
      << jsonString(governanceProposalStatusToString(
             governance.proposalStatus(proposalId)))
      << ",\"executed\":"
      << (governance.hasBeenExecuted(proposalId) ? "true" : "false")
      << ",\"detail\":"
      << jsonString(governance.proposalExecutionDetail(proposalId)) << "}";
  return oss.str();
}

std::string NodeRpcServer::handleSubmit(const std::string &body) {
  if (body.empty()) {
    return jsonError("Empty request body");
  }

  std::optional<core::Transaction> parsedTx;
  try {
    parsedTx = [&]() {
      if (body.rfind(kRpcSubmitSchemaId, 0) == 0) {
        return parseSignedTransactionSubmission(body);
      }

      if (body.rfind("Transaction{", 0) == 0) {
        throw std::invalid_argument("Raw Transaction serialization is not "
                                    "self-contained for RPC submit; "
                                    "send a NODO_RPC_TRANSACTION_SUBMISSION_V1 "
                                    "envelope with public key material.");
      }

      throw std::invalid_argument("Unsupported submit payload schema.");
    }();
  } catch (const std::exception &e) {
    return jsonError(std::string("Invalid submit payload: ") + e.what());
  }

  const core::Transaction &tx = parsedTx.value();
  if (tx.id().empty() || !tx.hasSignatureBundle()) {
    return jsonError("Failed to deserialize signed transaction");
  }

  const std::int64_t now =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  const config::GenesisConfig &genesisConfig =
      m_runtime.config().genesisConfig();
  const config::NetworkParameters &networkParameters =
      genesisConfig.networkParameters();

  const crypto::ProtocolCryptoContext cryptoContext =
      crypto::ProtocolCryptoContext::fromNetworkName(
          networkParameters.networkName());

  if (!cryptoContext.isValid()) {
    return jsonError("Runtime crypto context is invalid: " +
                     cryptoContext.rejectionReason());
  }

  if (networkParameters.minimumFeeRawUnits() >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return jsonError("Network minimum fee exceeds supported Amount range.");
  }

  const core::AccountStateView accountState =
      RuntimeAccountStateBuilder::accountStateViewAtTip(
          genesisConfig, m_runtime.blockchain(),
          static_cast<std::int64_t>(networkParameters.minimumFeeRawUnits()));

  const TransactionAdmissionContext admissionContext(
      accountState, m_runtime.mempool(), m_runtime.stakingRegistry(),
      m_runtime.validatorRegistry(), m_runtime.governanceExecutor(),
      m_runtime.blockchain().size());

  const TransactionAdmissionResult validation =
      TransactionAdmissionValidator::validateNetworkSubmission(
          tx, networkParameters, accountState, m_runtime.mempool(),
          cryptoContext.policy(), crypto::SecurityContext::USER_TRANSACTION,
          cryptoContext.userSignatureProvider(),
          m_runtime.effectiveMinimumFeeRawUnits(), &admissionContext);

  if (!validation.accepted()) {
    utils::log(utils::LogLevel::DEBUG, "NodeRpcServer") << "NodeRpcServer handleSubmit validation failed: "
              << validation.reason() << std::endl;
    return jsonError("Transaction rejected: " +
                     transactionAdmissionStatusToString(validation.status()) +
                     ": " + validation.reason());
  }

  std::string gossipPayload;
  if (m_gossip != nullptr) {
    const crypto::SignatureBundle &signatures = tx.signatureBundle();
    if (signatures.signatures().empty()) {
      return jsonError("Transaction signature bundle is empty.");
    }
    gossipPayload = PersistentMempoolStore::serializeForGossip(
        tx, signatures.signatures().front().publicKey(), now);
    if (gossipPayload.empty()) {
      return jsonError("Unable to build canonical transaction gossip payload.");
    }
  }

  const mempool::MempoolAdmissionResult result =
      m_runtime.mutableMempool().admitTransaction(
          tx, cryptoContext.policy(), crypto::SecurityContext::USER_TRANSACTION,
          now);

  // Broadcast the same self-contained schema consumed by NodeDaemon peers.
  if (m_gossip != nullptr && result.success()) {
    m_gossip->broadcast(p2p::NetworkMessageType::TRANSACTION_GOSSIP,
                        gossipPayload, now);
  }

  if (result.success()) {
    m_eventBus->publish(NodeEventType::TX_ADMITTED,
                        m_runtime.blockchain().empty()
                            ? 0
                            : m_runtime.blockchain().latestBlock().index(),
                        m_runtime.blockchain().empty()
                            ? std::string()
                            : m_runtime.blockchain().latestBlock().hash(),
                        std::string("{\"txId\":") +
                            jsonString(result.transactionId()) +
                            ",\"source\":\"json_rpc\"}",
                        now);
  } else if (!result.success()) {
    utils::log(utils::LogLevel::DEBUG, "NodeRpcServer") << "NodeRpcServer handleSubmit admitTransaction failed: "
              << result.reason() << std::endl;
  }

  std::ostringstream oss;
  oss << "{"
      << "\"status\":"
      << jsonString(mempool::mempoolAdmissionStatusToString(result.status()))
      << ",\"txId\":" << jsonString(result.transactionId())
      << ",\"reason\":" << jsonString(result.reason()) << "}";
  return oss.str();
}

std::string NodeRpcServer::handleLightCheckpoint() const {
  return LightClientService::checkpointJson(m_runtime);
}

std::string
NodeRpcServer::handleLightHeaders(const std::string &fromHeight,
                                  const std::string &maxHeaders) const {
  std::uint64_t from = 0;
  if (!parseUint64Strict(fromHeight, from)) {
    return jsonError("Invalid fromHeight parameter.");
  }
  std::uint64_t maxH = 0;
  if (!parseUint64Strict(maxHeaders, maxH)) {
    return jsonError("Invalid maxHeaders parameter.");
  }
  return LightClientService::headerRangeJson(m_runtime, from, maxH);
}

std::string
NodeRpcServer::handleLightAccountProof(const std::string &address) const {
  return LightClientService::accountProofJson(m_runtime, address);
}

std::string
NodeRpcServer::handleLightTransactionProof(const std::string &txId) const {
  return LightClientService::transactionProofJson(m_runtime, txId);
}

std::string NodeRpcServer::handleEvents(const std::string &afterSequence,
                                        const std::string &type,
                                        const std::string &limit) const {
  std::uint64_t seq = 0;
  if (!afterSequence.empty() && !parseUint64Strict(afterSequence, seq)) {
    return jsonError("Invalid afterSequence parameter.");
  }
  std::uint64_t maxL = 100;
  if (!limit.empty() && !parseUint64Strict(limit, maxL)) {
    return jsonError("Invalid limit parameter.");
  }
  std::optional<NodeEventType> eventType;
  if (!type.empty()) {
    eventType = nodeEventTypeFromString(type);
    if (!eventType.has_value()) {
      return jsonError("Invalid event type parameter.");
    }
  }
  if (m_eventBus != nullptr) {
    return m_eventBus->recentJson(seq, static_cast<std::size_t>(maxL),
                                  eventType);
  }
  return jsonError("Event bus is not configured on this node.");
}

std::string NodeRpcServer::handleHealth() const {
  const std::int64_t now =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  const NodeMetricsSnapshot metrics = NodeMetricsCollector::collect(
      m_runtime, m_syncHealth, m_eventBus, m_running.load(), "", now);
  return HealthCheckService::evaluate(metrics).serializeJson();
}

std::string NodeRpcServer::handleMetrics() const {
  const std::int64_t now =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  const NodeMetricsSnapshot metrics = NodeMetricsCollector::collect(
      m_runtime, m_syncHealth, m_eventBus, m_running.load(), "", now);
  const NodeHealthReport health = HealthCheckService::evaluate(metrics);
  std::ostringstream oss;
  oss << "{"
      << "\"healthStatus\":"
      << jsonString(nodeHealthStatusToString(health.status()))
      << ",\"metrics\":" << metrics.serializeJson() << "}";
  return oss.str();
}

std::string NodeRpcServer::handlePrometheusMetrics() const {
  const std::int64_t now =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  const NodeMetricsSnapshot metrics = NodeMetricsCollector::collect(
      m_runtime, m_syncHealth, m_eventBus, m_running.load(), "", now);
  const NodeHealthReport health = HealthCheckService::evaluate(metrics);
  return PrometheusExporter::exportMetrics(metrics, health.status());
}

} // namespace nodo::node
