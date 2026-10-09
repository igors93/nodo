#ifndef NODO_NODE_HISTORY_CHECKPOINT_BOOTSTRAP_HPP
#define NODO_NODE_HISTORY_CHECKPOINT_BOOTSTRAP_HPP

#include "config/HistoryParameters.hpp"
#include "config/NetworkParameters.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "core/Block.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "node/history/FinalizedStateCheckpoint.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * The checkpoint a new node trusts, obtained out of band (ADR 0004 weak
 * subjectivity): "<height>:<checkpointId>". Nothing a peer sends can replace
 * it. Without an anchor a bootstrap is refused, because a peer can always
 * fabricate a self-consistent history signed by keys whose stake has long
 * unbonded (long-range attack).
 */
struct TrustedCheckpointAnchor {
  std::uint64_t height = 0;
  std::string checkpointId;

  bool isValid() const;
  std::string serialize() const;
  static std::optional<TrustedCheckpointAnchor> parse(const std::string &text);
};

// One finalized block a peer serves after the checkpoint, with the record
// carrying its PRECOMMIT quorum certificate.
struct BootstrapFinalizedBlock {
  core::Block block;
  consensus::FinalizedBlockRecord finalizedRecord;
};

struct CheckpointBootstrapResult {
  bool verified = false;
  CheckpointVerificationStatus status = CheckpointVerificationStatus::MALFORMED;
  std::string reason;
  std::uint64_t verifiedHeight = 0;
  std::string verifiedBlockHash;
  std::string verifiedStateRoot;
  std::uint64_t replayedBlocks = 0;
  std::optional<ProtocolReplayState> state;
};

/*
 * CheckpointBootstrapVerifier turns untrusted peer data into a verified tip:
 *
 *   trusted anchor -> checkpoint id -> identity and parameters
 *   -> snapshot digest -> recomputed state root and decoded domains
 *   -> consensus window -> QC signatures by the committed set
 *   -> every later block: parent link, header hash, its own QC verified
 *      against the frozen set for its height, deterministic replay and the
 *      header state root.
 *
 * The first failing step rejects the whole bootstrap; nothing partial is
 * returned. The verifier never trusts the peer that served the data.
 */
class CheckpointBootstrapVerifier {
public:
  static CheckpointBootstrapResult
  verify(const config::GenesisConfig &genesisConfig,
         const config::HistoryParameters &parameters,
         const TrustedCheckpointAnchor &anchor,
         const FinalizedStateCheckpoint &checkpoint,
         const std::vector<unsigned char> &snapshotBytes,
         const std::vector<BootstrapFinalizedBlock> &laterBlocks,
         std::int64_t now);
};

} // namespace nodo::node

#endif
