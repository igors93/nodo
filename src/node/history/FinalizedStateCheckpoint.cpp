#include "node/history/FinalizedStateCheckpoint.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "consensus/QuorumThreshold.hpp"
#include "node/ValidatorLifecycle.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "utils/JsonText.hpp"
#include "utils/SafeScalar.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

constexpr std::size_t kMaxBodyFieldBytes = 512;

bool isSafeName(const std::string &value) {
  return utils::isSafeIdentifier(value, 128, "_-.:/");
}

bool isRootHex(const std::string &value) {
  // Development roots are 64 lowercase hex characters (Block's commitment
  // rule); new ADR 0014 digests are additionally non-zero.
  return value.size() == 64 &&
         value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

} // namespace

FinalizedStateCheckpoint::FinalizedStateCheckpoint()
    : m_fields(), m_quorumCertificate() {}

FinalizedStateCheckpoint::FinalizedStateCheckpoint(
    FinalizedStateCheckpointFields fields,
    consensus::QuorumCertificate quorumCertificate)
    : m_fields(std::move(fields)),
      m_quorumCertificate(std::move(quorumCertificate)) {}

const FinalizedStateCheckpointFields &FinalizedStateCheckpoint::fields() const {
  return m_fields;
}

const consensus::QuorumCertificate &
FinalizedStateCheckpoint::quorumCertificate() const {
  return m_quorumCertificate;
}

std::uint64_t FinalizedStateCheckpoint::height() const {
  return m_fields.height;
}

bool FinalizedStateCheckpoint::isStructurallyValid() const {
  const FinalizedStateCheckpointFields &f = m_fields;
  if (!isSafeName(f.networkName) || !isSafeName(f.chainId) ||
      !isSafeName(f.genesisConfigId) || !isSafeName(f.protocolVersion) ||
      !archive::isDigestHex(f.historyParametersId) || f.height == 0 ||
      f.blockTimestamp <= 0 || !isRootHex(f.blockHash) ||
      f.previousBlockHash.empty() || !isRootHex(f.stateRoot) ||
      !isRootHex(f.accountsRoot) || !isRootHex(f.validatorSetRoot) ||
      !isRootHex(f.nextValidatorSetRoot) ||
      !archive::isDigestHex(f.consensusContextDigest) ||
      !archive::isDigestHex(f.snapshotDigest) ||
      !archive::isDigestHex(f.archiveIndexRoot)) {
    return false;
  }
  if (f.epoch != ValidatorLifecycle::epochIndexForBlock(f.height)) {
    return false;
  }
  const bool firstCheckpoint =
      f.previousCheckpointHeight == 0 && f.previousCheckpointId.empty();
  const bool linkedCheckpoint = f.previousCheckpointHeight > 0 &&
                                f.previousCheckpointHeight < f.height &&
                                archive::isDigestHex(f.previousCheckpointId);
  if (!firstCheckpoint && !linkedCheckpoint) {
    return false;
  }
  const consensus::QuorumCertificate &qc = m_quorumCertificate;
  return qc.isStructurallyValid() && qc.blockIndex() == f.height &&
         qc.blockHash() == f.blockHash &&
         qc.previousHash() == f.previousBlockHash &&
         qc.validatorSetRoot() == f.validatorSetRoot;
}

std::vector<unsigned char> FinalizedStateCheckpoint::encodeBody() const {
  const FinalizedStateCheckpointFields &f = m_fields;
  serialization::CanonicalWriter writer;
  writer.writeString(SCHEMA);
  writer.writeUInt16(VERSION);
  writer.writeString(f.networkName);
  writer.writeString(f.chainId);
  writer.writeString(f.genesisConfigId);
  writer.writeString(f.protocolVersion);
  writer.writeString(f.historyParametersId);
  writer.writeUInt64(f.height);
  writer.writeUInt64(f.epoch);
  writer.writeString(f.blockHash);
  writer.writeString(f.previousBlockHash);
  writer.writeInt64(f.blockTimestamp);
  writer.writeString(f.stateRoot);
  writer.writeString(f.accountsRoot);
  writer.writeString(f.validatorSetRoot);
  writer.writeString(f.nextValidatorSetRoot);
  writer.writeString(f.consensusContextDigest);
  writer.writeString(f.snapshotDigest);
  writer.writeUInt64(f.archiveSealedSegmentCount);
  writer.writeString(f.archiveIndexRoot);
  writer.writeUInt64(f.previousCheckpointHeight);
  writer.writeString(f.previousCheckpointId);
  return writer.bytes();
}

std::string FinalizedStateCheckpoint::checkpointId() const {
  return archive::hashHex("HISTORY/CHECKPOINT", encodeBody());
}

std::vector<unsigned char> FinalizedStateCheckpoint::encode() const {
  serialization::CanonicalWriter writer;
  writer.writeString(FILE_SCHEMA);
  writer.writeBytes(encodeBody());
  writer.writeString(m_quorumCertificate.serialize());
  return writer.bytes();
}

FinalizedStateCheckpoint
FinalizedStateCheckpoint::decode(const std::vector<unsigned char> &bytes) {
  if (bytes.empty() || bytes.size() > kMaxEncodedBytes) {
    throw std::invalid_argument("Checkpoint encoding size is out of range.");
  }
  serialization::CanonicalReader outer(bytes, kMaxEncodedBytes);
  if (outer.readString() != FILE_SCHEMA) {
    throw std::invalid_argument("Unknown checkpoint file schema.");
  }
  const std::vector<unsigned char> body = outer.readBytes();
  const std::string qcText = outer.readString();
  outer.requireFullyConsumed();

  serialization::CanonicalReader reader(body, kMaxBodyFieldBytes);
  if (reader.readString() != SCHEMA || reader.readUInt16() != VERSION) {
    throw std::invalid_argument("Unsupported checkpoint schema or version.");
  }
  FinalizedStateCheckpointFields f;
  f.networkName = reader.readString();
  f.chainId = reader.readString();
  f.genesisConfigId = reader.readString();
  f.protocolVersion = reader.readString();
  f.historyParametersId = reader.readString();
  f.height = reader.readUInt64();
  f.epoch = reader.readUInt64();
  f.blockHash = reader.readString();
  f.previousBlockHash = reader.readString();
  f.blockTimestamp = reader.readInt64();
  f.stateRoot = reader.readString();
  f.accountsRoot = reader.readString();
  f.validatorSetRoot = reader.readString();
  f.nextValidatorSetRoot = reader.readString();
  f.consensusContextDigest = reader.readString();
  f.snapshotDigest = reader.readString();
  f.archiveSealedSegmentCount = reader.readUInt64();
  f.archiveIndexRoot = reader.readString();
  f.previousCheckpointHeight = reader.readUInt64();
  f.previousCheckpointId = reader.readString();
  reader.requireFullyConsumed();

  FinalizedStateCheckpoint checkpoint(
      std::move(f), consensus::QuorumCertificate::deserialize(qcText));
  if (!checkpoint.isStructurallyValid()) {
    throw std::invalid_argument("Checkpoint is structurally invalid.");
  }
  if (checkpoint.encode() != bytes) {
    throw std::invalid_argument("Checkpoint encoding is not canonical.");
  }
  return checkpoint;
}

std::string FinalizedStateCheckpoint::serializeJson() const {
  using utils::jsonString;
  const FinalizedStateCheckpointFields &f = m_fields;
  std::ostringstream oss;
  oss << "{\"checkpointId\":" << jsonString(checkpointId())
      << ",\"networkName\":" << jsonString(f.networkName)
      << ",\"chainId\":" << jsonString(f.chainId)
      << ",\"genesisConfigId\":" << jsonString(f.genesisConfigId)
      << ",\"protocolVersion\":" << jsonString(f.protocolVersion)
      << ",\"historyParametersId\":" << jsonString(f.historyParametersId)
      << ",\"height\":" << f.height << ",\"epoch\":" << f.epoch
      << ",\"blockHash\":" << jsonString(f.blockHash)
      << ",\"previousBlockHash\":" << jsonString(f.previousBlockHash)
      << ",\"blockTimestamp\":" << f.blockTimestamp
      << ",\"stateRoot\":" << jsonString(f.stateRoot)
      << ",\"accountsRoot\":" << jsonString(f.accountsRoot)
      << ",\"validatorSetRoot\":" << jsonString(f.validatorSetRoot)
      << ",\"nextValidatorSetRoot\":" << jsonString(f.nextValidatorSetRoot)
      << ",\"consensusContextDigest\":" << jsonString(f.consensusContextDigest)
      << ",\"snapshotDigest\":" << jsonString(f.snapshotDigest)
      << ",\"archiveSealedSegmentCount\":" << f.archiveSealedSegmentCount
      << ",\"archiveIndexRoot\":" << jsonString(f.archiveIndexRoot)
      << ",\"previousCheckpointHeight\":" << f.previousCheckpointHeight
      << ",\"previousCheckpointId\":" << jsonString(f.previousCheckpointId)
      << ",\"quorumCertificate\":{\"round\":" << m_quorumCertificate.round()
      << ",\"voteCount\":" << m_quorumCertificate.voteCount()
      << ",\"signedVotingWeight\":"
      << m_quorumCertificate.signedVotingWeight()
      << ",\"requiredVotingWeight\":"
      << m_quorumCertificate.requiredVotingWeight() << "}}";
  return oss.str();
}

std::string
checkpointVerificationStatusToString(CheckpointVerificationStatus status) {
  switch (status) {
  case CheckpointVerificationStatus::ACCEPTED:
    return "ACCEPTED";
  case CheckpointVerificationStatus::MALFORMED:
    return "MALFORMED";
  case CheckpointVerificationStatus::IDENTITY_MISMATCH:
    return "IDENTITY_MISMATCH";
  case CheckpointVerificationStatus::PARAMETERS_MISMATCH:
    return "PARAMETERS_MISMATCH";
  case CheckpointVerificationStatus::NOT_CHECKPOINT_HEIGHT:
    return "NOT_CHECKPOINT_HEIGHT";
  case CheckpointVerificationStatus::CERTIFICATE_MISMATCH:
    return "CERTIFICATE_MISMATCH";
  case CheckpointVerificationStatus::CERTIFICATE_INVALID:
    return "CERTIFICATE_INVALID";
  case CheckpointVerificationStatus::VALIDATOR_SET_MISMATCH:
    return "VALIDATOR_SET_MISMATCH";
  case CheckpointVerificationStatus::LINK_MISMATCH:
    return "LINK_MISMATCH";
  case CheckpointVerificationStatus::ANCHOR_MISMATCH:
    return "ANCHOR_MISMATCH";
  case CheckpointVerificationStatus::STALE_ANCHOR:
    return "STALE_ANCHOR";
  case CheckpointVerificationStatus::CONFLICT:
    return "CONFLICT";
  case CheckpointVerificationStatus::STATE_MISMATCH:
    return "STATE_MISMATCH";
  case CheckpointVerificationStatus::CONSENSUS_CONTEXT_MISMATCH:
    return "CONSENSUS_CONTEXT_MISMATCH";
  case CheckpointVerificationStatus::SNAPSHOT_DIGEST_MISMATCH:
    return "SNAPSHOT_DIGEST_MISMATCH";
  case CheckpointVerificationStatus::BLOCK_MISMATCH:
    return "BLOCK_MISMATCH";
  }
  return "MALFORMED";
}

CheckpointVerificationResult::CheckpointVerificationResult(
    CheckpointVerificationStatus status, std::string reason)
    : m_status(status), m_reason(std::move(reason)) {}

CheckpointVerificationResult CheckpointVerificationResult::accepted() {
  return CheckpointVerificationResult(CheckpointVerificationStatus::ACCEPTED,
                                      "");
}

CheckpointVerificationResult
CheckpointVerificationResult::rejected(CheckpointVerificationStatus status,
                                       std::string reason) {
  return CheckpointVerificationResult(status, std::move(reason));
}

CheckpointVerificationStatus CheckpointVerificationResult::status() const {
  return m_status;
}
const std::string &CheckpointVerificationResult::reason() const {
  return m_reason;
}
bool CheckpointVerificationResult::isAccepted() const {
  return m_status == CheckpointVerificationStatus::ACCEPTED;
}

CheckpointVerificationResult FinalizedStateCheckpointVerifier::verifyIdentity(
    const FinalizedStateCheckpoint &checkpoint,
    const config::GenesisConfig &genesisConfig,
    const config::HistoryParameters &parameters) {
  using Status = CheckpointVerificationStatus;
  if (!checkpoint.isStructurallyValid()) {
    return CheckpointVerificationResult::rejected(
        Status::MALFORMED, "checkpoint or its QC is structurally invalid");
  }
  const FinalizedStateCheckpointFields &f = checkpoint.fields();
  const config::NetworkParameters &network = genesisConfig.networkParameters();
  if (!genesisConfig.isValid() ||
      f.genesisConfigId != genesisConfig.deterministicId() ||
      f.chainId != network.chainId() ||
      f.networkName != network.networkName() ||
      f.protocolVersion != network.protocolVersion()) {
    return CheckpointVerificationResult::rejected(
        Status::IDENTITY_MISMATCH,
        "checkpoint belongs to a different network, genesis or protocol");
  }
  if (!parameters.isValid() ||
      parameters.networkName() != network.networkName() ||
      f.historyParametersId != parameters.deterministicId()) {
    return CheckpointVerificationResult::rejected(
        Status::PARAMETERS_MISMATCH,
        "checkpoint was built with different history parameters");
  }
  if (!parameters.isCheckpointHeight(f.height)) {
    return CheckpointVerificationResult::rejected(
        Status::NOT_CHECKPOINT_HEIGHT,
        "height " + std::to_string(f.height) +
            " is not on the checkpoint schedule");
  }
  const std::uint64_t interval = parameters.checkpointIntervalBlocks();
  const bool expectedFirst = f.height == interval;
  if ((expectedFirst && f.previousCheckpointHeight != 0) ||
      (!expectedFirst && f.previousCheckpointHeight != f.height - interval)) {
    return CheckpointVerificationResult::rejected(
        Status::LINK_MISMATCH,
        "checkpoint does not link to the previous scheduled checkpoint");
  }
  if (f.archiveSealedSegmentCount != parameters.sealedSegmentCount(f.height)) {
    return CheckpointVerificationResult::rejected(
        Status::MALFORMED,
        "checkpoint archive index does not cover every sealed segment");
  }
  const consensus::QuorumCertificate &qc = checkpoint.quorumCertificate();
  if (qc.requiredVotingWeight() !=
      consensus::QuorumThreshold::requiredWeight(qc.totalVotingWeight())) {
    return CheckpointVerificationResult::rejected(
        Status::CERTIFICATE_MISMATCH,
        "checkpoint QC names a noncanonical quorum");
  }
  return CheckpointVerificationResult::accepted();
}

CheckpointVerificationResult FinalizedStateCheckpointVerifier::verifyCertificate(
    const FinalizedStateCheckpoint &checkpoint,
    const core::ValidatorRegistry &validatorSetAtHeight,
    const crypto::ProtocolCryptoContext &cryptoContext) {
  using Status = CheckpointVerificationStatus;
  if (!checkpoint.isStructurallyValid()) {
    return CheckpointVerificationResult::rejected(
        Status::MALFORMED, "checkpoint or its QC is structurally invalid");
  }
  if (!validatorSetAtHeight.isValid() ||
      validatorSetAtHeight.validatorSetRoot() !=
          checkpoint.fields().validatorSetRoot) {
    return CheckpointVerificationResult::rejected(
        Status::VALIDATOR_SET_MISMATCH,
        "validator set does not match the checkpoint's committed set root");
  }
  if (!cryptoContext.isValid() ||
      !checkpoint.quorumCertificate().verify(
          validatorSetAtHeight, cryptoContext.policy(),
          cryptoContext.validatorSignatureProvider())) {
    return CheckpointVerificationResult::rejected(
        Status::CERTIFICATE_INVALID,
        "checkpoint QC signatures or voting weight do not verify");
  }
  return CheckpointVerificationResult::accepted();
}

CheckpointVerificationResult FinalizedStateCheckpointVerifier::verifyLink(
    const FinalizedStateCheckpoint &checkpoint,
    const FinalizedStateCheckpoint &previous) {
  const FinalizedStateCheckpointFields &f = checkpoint.fields();
  const FinalizedStateCheckpointFields &p = previous.fields();
  if (!checkpoint.isStructurallyValid() || !previous.isStructurallyValid() ||
      f.previousCheckpointHeight != p.height ||
      f.previousCheckpointId != previous.checkpointId() ||
      f.chainId != p.chainId || f.genesisConfigId != p.genesisConfigId ||
      f.archiveSealedSegmentCount < p.archiveSealedSegmentCount) {
    return CheckpointVerificationResult::rejected(
        CheckpointVerificationStatus::LINK_MISMATCH,
        "checkpoint does not extend the previous checkpoint");
  }
  return CheckpointVerificationResult::accepted();
}

} // namespace nodo::node
