#include "node/FastSyncSnapshotVerifier.hpp"

namespace nodo::node {

std::string fastSyncSnapshotVerificationStatusToString(
    FastSyncSnapshotVerificationStatus status) {
  switch (status) {
  case FastSyncSnapshotVerificationStatus::ACCEPTED:
    return "ACCEPTED";
  case FastSyncSnapshotVerificationStatus::INVALID_SNAPSHOT:
    return "INVALID_SNAPSHOT";
  case FastSyncSnapshotVerificationStatus::GENESIS_MISMATCH:
    return "GENESIS_MISMATCH";
  case FastSyncSnapshotVerificationStatus::CHAIN_MISMATCH:
    return "CHAIN_MISMATCH";
  case FastSyncSnapshotVerificationStatus::HEIGHT_MISMATCH:
    return "HEIGHT_MISMATCH";
  case FastSyncSnapshotVerificationStatus::HASH_MISMATCH:
    return "HASH_MISMATCH";
  case FastSyncSnapshotVerificationStatus::STATE_ROOT_MISMATCH:
    return "STATE_ROOT_MISMATCH";
  case FastSyncSnapshotVerificationStatus::DIGEST_MISMATCH:
    return "DIGEST_MISMATCH";
  default:
    return "INVALID_SNAPSHOT";
  }
}

FastSyncSnapshotVerificationResult::FastSyncSnapshotVerificationResult()
    : m_status(FastSyncSnapshotVerificationStatus::INVALID_SNAPSHOT),
      m_reason("Uninitialized fast-sync snapshot verification result.") {}

FastSyncSnapshotVerificationResult
FastSyncSnapshotVerificationResult::acceptedResult() {
  FastSyncSnapshotVerificationResult result;
  result.m_status = FastSyncSnapshotVerificationStatus::ACCEPTED;
  result.m_reason = "";
  return result;
}

FastSyncSnapshotVerificationResult FastSyncSnapshotVerificationResult::rejected(
    FastSyncSnapshotVerificationStatus status, std::string reason) {
  FastSyncSnapshotVerificationResult result;
  result.m_status = status;
  result.m_reason = std::move(reason);
  return result;
}

FastSyncSnapshotVerificationStatus
FastSyncSnapshotVerificationResult::status() const {
  return m_status;
}

const std::string &FastSyncSnapshotVerificationResult::reason() const {
  return m_reason;
}

bool FastSyncSnapshotVerificationResult::accepted() const {
  return m_status == FastSyncSnapshotVerificationStatus::ACCEPTED;
}

FastSyncSnapshotVerificationResult FastSyncSnapshotVerifier::verifyForGenesis(
    const FastSyncSnapshot &snapshot,
    const config::GenesisConfig &genesisConfig) {
  if (!snapshot.isValid()) {
    return FastSyncSnapshotVerificationResult::rejected(
        FastSyncSnapshotVerificationStatus::INVALID_SNAPSHOT,
        "snapshot is structurally invalid");
  }
  if (!genesisConfig.isValid()) {
    return FastSyncSnapshotVerificationResult::rejected(
        FastSyncSnapshotVerificationStatus::GENESIS_MISMATCH,
        "genesis config is invalid");
  }
  if (snapshot.genesisConfigId() != genesisConfig.deterministicId()) {
    return FastSyncSnapshotVerificationResult::rejected(
        FastSyncSnapshotVerificationStatus::GENESIS_MISMATCH,
        "snapshot genesis id does not match local genesis");
  }
  if (snapshot.chainId() != genesisConfig.networkParameters().chainId() ||
      snapshot.networkName() !=
          genesisConfig.networkParameters().networkName()) {
    return FastSyncSnapshotVerificationResult::rejected(
        FastSyncSnapshotVerificationStatus::CHAIN_MISMATCH,
        "snapshot chain id or network name does not match local genesis");
  }
  return FastSyncSnapshotVerificationResult::acceptedResult();
}

FastSyncSnapshotVerificationResult
FastSyncSnapshotVerifier::verifyAgainstManifest(
    const FastSyncSnapshot &snapshot,
    const config::GenesisConfig &genesisConfig,
    const PersistentSnapshotSyncManifest &manifest) {
  const FastSyncSnapshotVerificationResult base =
      verifyForGenesis(snapshot, genesisConfig);
  if (!base.accepted()) {
    return base;
  }
  if (!manifest.isValid()) {
    return FastSyncSnapshotVerificationResult::rejected(
        FastSyncSnapshotVerificationStatus::INVALID_SNAPSHOT,
        "snapshot sync manifest is invalid");
  }
  if (snapshot.blockHeight() != manifest.snapshotHeight()) {
    return FastSyncSnapshotVerificationResult::rejected(
        FastSyncSnapshotVerificationStatus::HEIGHT_MISMATCH,
        "snapshot height does not match manifest");
  }
  if (snapshot.blockHash() != manifest.snapshotBlockHash()) {
    return FastSyncSnapshotVerificationResult::rejected(
        FastSyncSnapshotVerificationStatus::HASH_MISMATCH,
        "snapshot block hash does not match manifest");
  }
  if (snapshot.stateRoot() != manifest.snapshotStateRoot()) {
    return FastSyncSnapshotVerificationResult::rejected(
        FastSyncSnapshotVerificationStatus::STATE_ROOT_MISMATCH,
        "snapshot state root does not match manifest");
  }
  if (snapshot.digest() != manifest.snapshotDigest()) {
    return FastSyncSnapshotVerificationResult::rejected(
        FastSyncSnapshotVerificationStatus::DIGEST_MISMATCH,
        "snapshot digest does not match manifest");
  }
  return FastSyncSnapshotVerificationResult::acceptedResult();
}

} // namespace nodo::node
