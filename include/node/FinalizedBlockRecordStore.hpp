#ifndef NODO_NODE_FINALIZED_BLOCK_RECORD_STORE_HPP
#define NODO_NODE_FINALIZED_BLOCK_RECORD_STORE_HPP

#include "consensus/BlockFinalizer.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace nodo::node {

/*
 * FinalizedBlockRecordStore persists one FinalizedBlockRecord per finalized
 * block height to disk. Each record carries the QuorumCertificate (QC) that
 * proved 2/3+ validator weight approved the block, giving the node a durable
 * finality proof that survives restarts.
 *
 * Storage layout: {dataDirectory}/sync/qc/{height}.qc
 *
 * Files are written atomically via AtomicFile so a crash between write and
 * rename never leaves a partially-written record on disk.
 *
 * Idempotency: saving the identical record for an already-stored height
 * succeeds silently. Saving a divergent record (different block hash) for
 * the same height returns false — conflicting finality decisions are a
 * protocol violation and must not overwrite a stored proof.
 */
class FinalizedBlockRecordStore {
public:
  explicit FinalizedBlockRecordStore(std::filesystem::path dataDirectory);

  const std::filesystem::path &dataDirectory() const;

  // Returns the path for the record file at the given height.
  std::filesystem::path recordFilePath(std::uint64_t height) const;

  // Persists record atomically. Returns true on success or if the identical
  // record is already stored. Returns false for a divergent record or on
  // I/O failure.
  bool save(const consensus::FinalizedBlockRecord &record) const;

  // Returns the record for height, or nullopt if absent or unreadable.
  std::optional<consensus::FinalizedBlockRecord>
  load(std::uint64_t height) const;

  // Returns all loadable records sorted by ascending block height.
  // Corrupted or missing files are silently skipped.
  std::vector<consensus::FinalizedBlockRecord> loadAll() const;

private:
  std::filesystem::path m_dataDirectory;
};

} // namespace nodo::node

#endif
