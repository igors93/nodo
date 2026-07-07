#include "node/LightClientService.hpp"

#include "core/MerkleTree.hpp"
#include "core/StateRootCalculator.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace nodo::node {
namespace {

std::string jsonString(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (const char c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  out.push_back('"');
  return out;
}

std::string jsonError(const std::string &message) {
  return "{\"error\":" + jsonString(message) + "}";
}

std::string recordJson(const core::LedgerRecord &record) {
  std::ostringstream oss;
  oss << "{"
      << "\"id\":" << jsonString(record.id())
      << ",\"sourceId\":" << jsonString(record.sourceId()) << ",\"type\":"
      << jsonString(core::ledgerRecordTypeToString(record.type()))
      << ",\"payloadHash\":" << jsonString(record.payloadHash())
      << ",\"timestamp\":" << record.timestamp() << "}";
  return oss.str();
}

std::string accountJson(const core::AccountState &account) {
  std::ostringstream oss;
  oss << "{"
      << "\"address\":" << jsonString(account.address())
      << ",\"balance\":" << account.balance().rawUnits()
      << ",\"nonce\":" << account.nonce() << "}";
  return oss.str();
}

std::uint64_t safeMinimumFeeRaw(const NodeRuntime &runtime) {
  return runtime.effectiveMinimumFeeRawUnits();
}

std::int64_t minimumFeeForAccountView(const NodeRuntime &runtime) {
  const std::uint64_t raw = safeMinimumFeeRaw(runtime);
  if (raw >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return std::numeric_limits<std::int64_t>::max();
  }
  return static_cast<std::int64_t>(raw);
}

LightClientHeader buildHeader(const NodeRuntime &runtime,
                              const core::Block &block,
                              const consensus::FinalizedBlockRecord &record) {
  const config::GenesisConfig &genesis = runtime.config().genesisConfig();
  const config::NetworkParameters &network = genesis.networkParameters();
  const std::string validatorSetRoot =
      record.quorumCertificate().validatorSetRoot().empty()
          ? runtime.validatorRegistry().validatorSetRoot()
          : record.quorumCertificate().validatorSetRoot();
  return LightClientHeader(
      network.networkName(), network.chainId(), genesis.deterministicId(),
      block.index(), block.hash(), block.previousHash(), block.stateRoot(),
      block.receiptsRoot(), block.timestamp(), block.headerPayload(),
      validatorSetRoot, record.serialize(),
      record.quorumCertificate().serialize());
}

std::string headersJson(const std::vector<LightClientHeader> &headers,
                        const std::string &verificationReason) {
  std::ostringstream oss;
  std::string reason = verificationReason;
  const bool verified =
      LightClientProtocolVerifier::verifyHeaderChain(headers, &reason);
  oss << "{\"headers\":[";
  for (std::size_t i = 0; i < headers.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << headers[i].serializeJson();
  }
  oss << "],\"count\":" << headers.size()
      << ",\"verified\":" << (verified ? "true" : "false") << ",\"reason\":"
      << jsonString(reason.empty() ? verificationReason : reason) << "}";
  return oss.str();
}

core::MerkleProof
buildSortedMerkleProof(const std::vector<std::string> &payloads,
                       const std::string &targetPayload) {
  if (payloads.empty()) {
    return core::MerkleProof();
  }

  std::vector<std::string> sorted = payloads;
  std::sort(sorted.begin(), sorted.end());
  auto found = std::find(sorted.begin(), sorted.end(), targetPayload);
  if (found == sorted.end()) {
    return core::MerkleProof();
  }

  std::size_t targetIndex =
      static_cast<std::size_t>(std::distance(sorted.begin(), found));
  std::vector<std::string> level;
  level.reserve(sorted.size());
  for (const auto &payload : sorted) {
    level.push_back(core::MerkleTree::hashLeaf(payload));
  }

  const std::string leafHash = level[targetIndex];
  std::vector<core::MerkleProofStep> steps;
  while (level.size() > 1) {
    const bool isRightChild = (targetIndex % 2) == 1;
    std::size_t siblingIndex = isRightChild ? targetIndex - 1 : targetIndex + 1;
    if (siblingIndex >= level.size()) {
      siblingIndex = targetIndex;
    }
    steps.push_back(core::MerkleProofStep{level[siblingIndex], isRightChild});

    std::vector<std::string> next;
    next.reserve((level.size() + 1) / 2);
    for (std::size_t i = 0; i < level.size(); i += 2) {
      const std::string &left = level[i];
      const std::string &right =
          (i + 1 < level.size()) ? level[i + 1] : level[i];
      next.push_back(core::MerkleTree::hashNode(left, right));
    }
    targetIndex /= 2;
    level = std::move(next);
  }

  return core::MerkleProof(leafHash, steps);
}

std::string recordsRootForBlock(const core::Block &block) {
  std::vector<std::string> payloads;
  payloads.reserve(block.records().size());
  for (const auto &record : block.records()) {
    payloads.push_back(record.serialize());
  }
  return core::MerkleTree::buildRoot(payloads);
}

} // namespace

std::optional<LightClientHeader>
LightClientService::headerAt(const NodeRuntime &runtime, std::uint64_t height) {
  const auto blockOpt = runtime.blockchain().blockByHeight(height);
  if (!blockOpt.has_value()) {
    return std::nullopt;
  }
  const consensus::FinalizedBlockRecord *record =
      runtime.finalizationRegistry().recordForHeight(height);
  if (record == nullptr || !record->matchesBlock(blockOpt.value()) ||
      !record->isStructurallyValid()) {
    return std::nullopt;
  }
  return buildHeader(runtime, blockOpt.value(), *record);
}

std::vector<LightClientHeader>
LightClientService::headerRange(const NodeRuntime &runtime,
                                std::uint64_t fromHeight,
                                std::uint64_t maxHeaders) {
  maxHeaders =
      std::min<std::uint64_t>(std::max<std::uint64_t>(maxHeaders, 1), 512);
  std::vector<LightClientHeader> headers;
  headers.reserve(static_cast<std::size_t>(maxHeaders));
  const std::uint64_t highestFinalized =
      runtime.finalizationRegistry().highestFinalizedHeight();
  for (std::uint64_t height = fromHeight;
       height <= highestFinalized && headers.size() < maxHeaders; ++height) {
    const auto header = headerAt(runtime, height);
    if (!header.has_value()) {
      break;
    }
    headers.push_back(header.value());
    if (height == std::numeric_limits<std::uint64_t>::max()) {
      break;
    }
  }
  return headers;
}

std::optional<LightClientHeader>
LightClientService::latestFinalizedHeader(const NodeRuntime &runtime) {
  const std::uint64_t highest =
      runtime.finalizationRegistry().highestFinalizedHeight();
  return headerAt(runtime, highest);
}

std::string LightClientService::checkpointJson(const NodeRuntime &runtime) {
  const auto header = latestFinalizedHeader(runtime);
  if (!header.has_value()) {
    return jsonError("No finalized light-client checkpoint is available.");
  }
  return "{\"checkpoint\":" + header->serializeJson() + "}";
}

std::string LightClientService::headerRangeJson(const NodeRuntime &runtime,
                                                std::uint64_t fromHeight,
                                                std::uint64_t maxHeaders) {
  const auto headers = headerRange(runtime, fromHeight, maxHeaders);
  if (headers.empty()) {
    return jsonError(
        "No finalized light-client headers are available for requested range.");
  }
  return headersJson(headers, "verified");
}

std::string LightClientService::accountProofJson(const NodeRuntime &runtime,
                                                 const std::string &address) {
  if (address.empty()) {
    return jsonError("Missing address.");
  }
  const auto header = latestFinalizedHeader(runtime);
  if (!header.has_value()) {
    return jsonError("No finalized checkpoint is available for account proof.");
  }

  const core::AccountStateView &view =
      runtime.cachedAccountStateAtTip(minimumFeeForAccountView(runtime));
  if (!view.hasAccount(address)) {
    return jsonError("Address not present in current account state: " +
                     address);
  }
  const core::MerkleProof proof =
      core::StateRootCalculator::accountInclusionProof(view, address);
  if (!proof.isValid()) {
    return jsonError("Failed to build account inclusion proof: " + address);
  }
  const std::string accountRoot =
      core::StateRootCalculator::calculateAccountStateRoot(view);
  const LightClientAccountProof bundle(
      *header, address, accountJson(view.accountOrDefault(address)),
      accountRoot, proof);
  return bundle.serializeJson();
}

std::string
LightClientService::transactionProofJson(const NodeRuntime &runtime,
                                         const std::string &transactionId) {
  if (transactionId.empty()) {
    return jsonError("Missing transaction id.");
  }
  for (const auto &block : runtime.blockchain().blocks()) {
    for (const auto &record : block.records()) {
      if (record.id() != transactionId && record.sourceId() != transactionId) {
        continue;
      }
      const auto header = headerAt(runtime, block.index());
      if (!header.has_value()) {
        return jsonError("Transaction block is not finalized or lacks a QC.");
      }
      std::vector<std::string> payloads;
      payloads.reserve(block.records().size());
      for (const auto &candidate : block.records()) {
        payloads.push_back(candidate.serialize());
      }
      const core::MerkleProof proof =
          buildSortedMerkleProof(payloads, record.serialize());
      if (!proof.isValid()) {
        return jsonError("Failed to build transaction inclusion proof.");
      }
      const LightClientTransactionProof bundle(
          *header, transactionId, recordJson(record),
          recordsRootForBlock(block), proof);
      return bundle.serializeJson();
    }
  }
  return jsonError("Transaction not found: " + transactionId);
}

} // namespace nodo::node
