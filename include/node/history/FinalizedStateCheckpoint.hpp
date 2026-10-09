#ifndef NODO_NODE_HISTORY_FINALIZED_STATE_CHECKPOINT_HPP
#define NODO_NODE_HISTORY_FINALIZED_STATE_CHECKPOINT_HPP

#include "config/HistoryParameters.hpp"
#include "config/NetworkParameters.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/ProtocolCryptoContext.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

// Named fields: a checkpoint has many adjacent hashes that must not swap.
struct FinalizedStateCheckpointFields {
  std::string networkName;
  std::string chainId;
  std::string genesisConfigId;
  std::string protocolVersion;
  std::string historyParametersId;

  std::uint64_t height = 0;
  std::uint64_t epoch = 0;
  std::string blockHash;
  std::string previousBlockHash;
  std::int64_t blockTimestamp = 0;

  // The canonical protocol state root already commits accounts and every
  // protocol domain (staking, validators, slashing, governance, supply,
  // burns); accountsRoot is its account leaf, kept for light-client proofs.
  std::string stateRoot;
  std::string accountsRoot;

  // Frozen consensus set that signed this height and the set selected for
  // the next height, plus the digest of the validator-set history window the
  // snapshot carries for still-admissible evidence.
  std::string validatorSetRoot;
  std::string nextValidatorSetRoot;
  std::string consensusContextDigest;
  std::string snapshotDigest;

  std::uint64_t archiveSealedSegmentCount = 0;
  std::string archiveIndexRoot;

  std::uint64_t previousCheckpointHeight = 0;
  std::string previousCheckpointId;
};

/*
 * FinalizedStateCheckpoint is a commitment to the complete protocol state at
 * a height the existing BFT consensus already finalized (ADR 0014).
 *
 * It adds no new vote. The block hash commits the header state root, and the
 * block's own PRECOMMIT quorum certificate signs that hash, so the QC already
 * certifies the state root. The checkpoint carries that QC as evidence.
 *
 * checkpointId() hashes every field except the QC: different honest nodes may
 * hold different valid vote subsets for the same block, but they derive the
 * same identifier. That identifier is what operators exchange out of band as
 * a weak-subjectivity anchor (ADR 0004).
 */
class FinalizedStateCheckpoint {
public:
  static constexpr const char *SCHEMA = "NODO_FINALIZED_STATE_CHECKPOINT_V1";
  static constexpr const char *FILE_SCHEMA =
      "NODO_FINALIZED_STATE_CHECKPOINT_FILE_V1";
  static constexpr std::uint16_t VERSION = 1;
  // A QC carries every vote, so the cap follows the v1 QC object limit.
  static constexpr std::size_t kMaxEncodedBytes = 1024 * 1024 + 4096;

  FinalizedStateCheckpoint();
  FinalizedStateCheckpoint(FinalizedStateCheckpointFields fields,
                           consensus::QuorumCertificate quorumCertificate);

  const FinalizedStateCheckpointFields &fields() const;
  const consensus::QuorumCertificate &quorumCertificate() const;
  std::uint64_t height() const;

  bool isStructurallyValid() const;
  std::string checkpointId() const;

  std::vector<unsigned char> encodeBody() const;
  std::vector<unsigned char> encode() const;
  static FinalizedStateCheckpoint decode(const std::vector<unsigned char> &bytes);

  std::string serializeJson() const;

private:
  FinalizedStateCheckpointFields m_fields;
  consensus::QuorumCertificate m_quorumCertificate;
};

enum class CheckpointVerificationStatus {
  ACCEPTED,
  MALFORMED,
  IDENTITY_MISMATCH,
  PARAMETERS_MISMATCH,
  NOT_CHECKPOINT_HEIGHT,
  CERTIFICATE_MISMATCH,
  CERTIFICATE_INVALID,
  VALIDATOR_SET_MISMATCH,
  LINK_MISMATCH,
  ANCHOR_MISMATCH,
  STALE_ANCHOR,
  CONFLICT,
  STATE_MISMATCH,
  CONSENSUS_CONTEXT_MISMATCH,
  SNAPSHOT_DIGEST_MISMATCH,
  BLOCK_MISMATCH
};

std::string
checkpointVerificationStatusToString(CheckpointVerificationStatus status);

class CheckpointVerificationResult {
public:
  static CheckpointVerificationResult accepted();
  static CheckpointVerificationResult rejected(CheckpointVerificationStatus status,
                                               std::string reason);

  CheckpointVerificationStatus status() const;
  const std::string &reason() const;
  bool isAccepted() const;

private:
  CheckpointVerificationResult(CheckpointVerificationStatus status,
                               std::string reason);
  CheckpointVerificationStatus m_status;
  std::string m_reason;
};

class FinalizedStateCheckpointVerifier {
public:
  // Network identity, parameter binding, checkpoint height, epoch, archive
  // count and QC/header field agreement. No signatures are checked.
  static CheckpointVerificationResult
  verifyIdentity(const FinalizedStateCheckpoint &checkpoint,
                 const config::GenesisConfig &genesisConfig,
                 const config::HistoryParameters &parameters);

  // The QC must be signed by the frozen validator set for the checkpoint
  // height, and that set must be the one the checkpoint commits.
  static CheckpointVerificationResult
  verifyCertificate(const FinalizedStateCheckpoint &checkpoint,
                    const core::ValidatorRegistry &validatorSetAtHeight,
                    const crypto::ProtocolCryptoContext &cryptoContext);

  static CheckpointVerificationResult
  verifyLink(const FinalizedStateCheckpoint &checkpoint,
             const FinalizedStateCheckpoint &previous);
};

} // namespace nodo::node

#endif
