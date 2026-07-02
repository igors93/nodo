#include "node/FinalizedArtifactGossipAdmission.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "node/FinalizedBlockRecordStore.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/NetworkEnvelope.hpp"

#include <exception>
#include <utility>

namespace nodo::node {

FinalizedArtifactGossipAdmissionResult
FinalizedArtifactGossipAdmissionResult::accepted() {
  FinalizedArtifactGossipAdmissionResult result;
  result.m_status = FinalizedArtifactGossipAdmissionStatus::ACCEPTED;
  return result;
}

FinalizedArtifactGossipAdmissionResult
FinalizedArtifactGossipAdmissionResult::duplicate() {
  FinalizedArtifactGossipAdmissionResult result;
  result.m_status = FinalizedArtifactGossipAdmissionStatus::DUPLICATE;
  return result;
}

FinalizedArtifactGossipAdmissionResult
FinalizedArtifactGossipAdmissionResult::rejected(
    FinalizedArtifactGossipAdmissionStatus status, std::string reason) {
  FinalizedArtifactGossipAdmissionResult result;
  result.m_status = status;
  result.m_reason = std::move(reason);
  return result;
}

FinalizedArtifactGossipAdmissionStatus
FinalizedArtifactGossipAdmissionResult::status() const {
  return m_status;
}

const std::string &FinalizedArtifactGossipAdmissionResult::reason() const {
  return m_reason;
}

bool FinalizedArtifactGossipAdmissionResult::acceptedRecord() const {
  return m_status == FinalizedArtifactGossipAdmissionStatus::ACCEPTED;
}

bool FinalizedArtifactGossipAdmissionResult::duplicateRecord() const {
  return m_status == FinalizedArtifactGossipAdmissionStatus::DUPLICATE;
}

bool FinalizedArtifactGossipAdmissionResult::fatalConsistencyError() const {
  return m_status ==
             FinalizedArtifactGossipAdmissionStatus::PERSISTENCE_FAILED ||
         m_status ==
             FinalizedArtifactGossipAdmissionStatus::REGISTRATION_FAILED;
}

FinalizedArtifactGossipAdmissionResult FinalizedArtifactGossipAdmission::admit(
    const p2p::NetworkEnvelope &envelope, NodeRuntime &runtime,
    const crypto::CryptoPolicy &policy,
    const crypto::SignatureProvider &provider,
    const FinalizedBlockRecordStore &store) {
  if (envelope.messageType() !=
      p2p::NetworkMessageType::FINALIZED_BLOCK_ARTIFACT) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::WRONG_MESSAGE_TYPE,
        "Envelope is not a finalized block artifact.");
  }

  const auto &network = runtime.config().genesisConfig().networkParameters();
  if (envelope.networkId() != network.networkName() ||
      envelope.chainId() != network.chainId() ||
      envelope.protocolVersion() !=
          runtime.config().localPeer().protocolVersion()) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::NETWORK_CONTEXT_MISMATCH,
        "Finalized artifact envelope does not match the local network "
        "context.");
  }

  if (envelope.payload().empty()) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::EMPTY_PAYLOAD,
        "Finalized artifact payload is empty.");
  }

  consensus::FinalizedBlockRecord record;
  try {
    record = consensus::FinalizedBlockRecord::deserialize(envelope.payload());
  } catch (const std::exception &error) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::MALFORMED_PAYLOAD,
        error.what());
  } catch (...) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::MALFORMED_PAYLOAD,
        "Finalized artifact payload could not be decoded.");
  }

  if (!record.isStructurallyValid()) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::INVALID_RECORD,
        "Finalized artifact record is structurally invalid.");
  }

  const std::uint64_t blockIndex = record.blockIndex();
  const auto &blocks = runtime.blockchain().blocks();
  if (blockIndex >= blocks.size()) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::BLOCK_UNAVAILABLE,
        "Finalized artifact block is not present in the local canonical "
        "chain.");
  }
  if (!record.matchesBlock(blocks[static_cast<std::size_t>(blockIndex)])) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::BLOCK_MISMATCH,
        "Finalized artifact does not match the local canonical block.");
  }

  if (!runtime.validatorSetHistory().hasSet(blockIndex)) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::VALIDATOR_SET_UNAVAILABLE,
        "Historical validator set is unavailable for the artifact height.");
  }
  bool verified = false;
  try {
    verified = record.verify(runtime.validatorSetHistory().setAt(blockIndex),
                             policy, provider);
  } catch (...) {
    verified = false;
  }
  if (!verified) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::INVALID_QUORUM_CERTIFICATE,
        "Finalized artifact quorum certificate is invalid.");
  }

  consensus::BlockFinalizationRegistry &registry =
      runtime.mutableFinalizationRegistry();
  const consensus::FinalizedBlockRecord *existing =
      registry.recordForHeight(blockIndex);
  if (existing != nullptr) {
    if (existing->serialize() != record.serialize()) {
      return FinalizedArtifactGossipAdmissionResult::rejected(
          FinalizedArtifactGossipAdmissionStatus::CONFLICTING_FINALIZATION,
          "Finalized artifact conflicts with the recorded finality proof.");
    }
    if (!store.save(record)) {
      return FinalizedArtifactGossipAdmissionResult::rejected(
          FinalizedArtifactGossipAdmissionStatus::PERSISTENCE_FAILED,
          "Existing finality proof could not be persisted.");
    }
    return FinalizedArtifactGossipAdmissionResult::duplicate();
  }

  if (!registry.canRegister(record)) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::CONFLICTING_FINALIZATION,
        "Finalized artifact cannot be registered without violating finality.");
  }

  // Persist before exposing finality in memory so block-sync QC mode never
  // observes volatile-only finality without its durable proof.
  if (!store.save(record)) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::PERSISTENCE_FAILED,
        "Finality proof could not be persisted atomically.");
  }

  const consensus::BlockFinalizationRegistryResult registered =
      registry.registerFinalizedBlock(record);
  if (registered.duplicate()) {
    return FinalizedArtifactGossipAdmissionResult::duplicate();
  }
  if (!registered.registered()) {
    return FinalizedArtifactGossipAdmissionResult::rejected(
        FinalizedArtifactGossipAdmissionStatus::REGISTRATION_FAILED,
        registered.reason());
  }

  return FinalizedArtifactGossipAdmissionResult::accepted();
}

} // namespace nodo::node
