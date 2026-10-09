#ifndef NODO_NODE_HISTORY_FULL_PROTOCOL_STATE_SNAPSHOT_HPP
#define NODO_NODE_HISTORY_FULL_PROTOCOL_STATE_SNAPSHOT_HPP

#include "config/HistoryParameters.hpp"
#include "config/NetworkParameters.hpp"
#include "core/ValidatorRegistry.hpp"
#include "node/FastSyncSnapshot.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "node/history/FinalizedStateCheckpoint.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class NodeRuntime;

/*
 * FullProtocolStateSnapshot is everything a node needs to continue
 * validating finalized blocks after a checkpoint without the earlier history
 * (ADR 0014). It is a representation of protocol state, never a second
 * source of truth:
 *
 *   state            the existing FastSyncSnapshot (accounts plus every
 *                    protocol domain payload); it must hash to the
 *                    checkpoint's stateRoot, which the block QC certifies.
 *   consensusWindow  the frozen validator sets for heights
 *                    [max(1, H+1-evidenceMaxAge), H+1], so equivocation
 *                    evidence that is still admissible after H can be
 *                    verified. Its set at H must match the QC's set root.
 *
 * The snapshot digest is the v1 Merkle root (kind "snapshot-chunk") of the
 * canonical encoding cut into the network's snapshot chunk size, so a peer's
 * chunks can be verified one by one against a checkpoint.
 */
class FullProtocolStateSnapshot {
public:
  static constexpr const char *SCHEMA = "NODO_FULL_PROTOCOL_STATE_SNAPSHOT_V1";
  static constexpr const char *WINDOW_SCHEMA = "NODO_VALIDATOR_SET_WINDOW_V1";

  FullProtocolStateSnapshot();
  FullProtocolStateSnapshot(FastSyncSnapshot state,
                            core::ValidatorSetHistory consensusWindow);

  const FastSyncSnapshot &state() const;
  const core::ValidatorSetHistory &consensusWindow() const;
  std::uint64_t height() const;

  // First height whose validator set a snapshot at `height` must carry.
  static std::uint64_t consensusWindowStart(std::uint64_t height);

  std::string consensusContextDigest() const;
  std::vector<unsigned char> encode() const;
  static FullProtocolStateSnapshot decode(const std::vector<unsigned char> &bytes,
                                          std::uint64_t maxBytes);

  std::string digest(std::uint32_t chunkBytes) const;
  static std::string digestOfEncoding(const std::vector<unsigned char> &encoded,
                                      std::uint32_t chunkBytes);

  // Decoded protocol replay state. Throws if a domain fails strict decoding.
  ProtocolReplayState toReplayState() const;

  static FullProtocolStateSnapshot
  fromReplayState(const config::GenesisConfig &genesisConfig,
                  std::uint64_t height, const std::string &blockHash,
                  std::int64_t blockTimestamp,
                  const ProtocolReplayState &replayState);

  static FullProtocolStateSnapshot fromRuntime(const NodeRuntime &runtime);

private:
  FastSyncSnapshot m_state;
  core::ValidatorSetHistory m_consensusWindow;
};

class FullProtocolStateSnapshotVerifier {
public:
  // Rejects any snapshot that does not reproduce exactly what the checkpoint
  // commits: identity, height, block hash, recomputed state root, strictly
  // decoded domains, the consensus window and its digest, the QC set root,
  // the deterministic next-set projection and the chunked digest.
  static CheckpointVerificationResult
  verifyAgainstCheckpoint(const FullProtocolStateSnapshot &snapshot,
                          const FinalizedStateCheckpoint &checkpoint,
                          const config::GenesisConfig &genesisConfig,
                          const config::HistoryParameters &parameters);
};

} // namespace nodo::node

#endif
