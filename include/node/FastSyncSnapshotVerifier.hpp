#ifndef NODO_NODE_FAST_SYNC_SNAPSHOT_VERIFIER_HPP
#define NODO_NODE_FAST_SYNC_SNAPSHOT_VERIFIER_HPP

#include "config/NetworkParameters.hpp"
#include "node/FastSyncSnapshot.hpp"
#include "node/PersistentBlockStateSync.hpp"

#include <string>

namespace nodo::node {

enum class FastSyncSnapshotVerificationStatus {
  ACCEPTED,
  INVALID_SNAPSHOT,
  GENESIS_MISMATCH,
  CHAIN_MISMATCH,
  HEIGHT_MISMATCH,
  HASH_MISMATCH,
  STATE_ROOT_MISMATCH,
  DIGEST_MISMATCH
};

std::string fastSyncSnapshotVerificationStatusToString(
    FastSyncSnapshotVerificationStatus status);

class FastSyncSnapshotVerificationResult {
public:
  static FastSyncSnapshotVerificationResult acceptedResult();
  static FastSyncSnapshotVerificationResult
  rejected(FastSyncSnapshotVerificationStatus status, std::string reason);

  FastSyncSnapshotVerificationStatus status() const;
  const std::string &reason() const;
  bool accepted() const;

private:
  FastSyncSnapshotVerificationResult();
  FastSyncSnapshotVerificationStatus m_status;
  std::string m_reason;
};

class FastSyncSnapshotVerifier {
public:
  static FastSyncSnapshotVerificationResult
  verifyForGenesis(const FastSyncSnapshot &snapshot,
                   const config::GenesisConfig &genesisConfig);

  static FastSyncSnapshotVerificationResult
  verifyAgainstManifest(const FastSyncSnapshot &snapshot,
                        const config::GenesisConfig &genesisConfig,
                        const PersistentSnapshotSyncManifest &manifest);
};

} // namespace nodo::node

#endif
