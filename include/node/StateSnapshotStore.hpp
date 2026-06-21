#ifndef NODO_NODE_STATE_SNAPSHOT_STORE_HPP
#define NODO_NODE_STATE_SNAPSHOT_STORE_HPP

#include "core/State.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace nodo::node {

/*
 * StateSnapshotStore persists a serialized core::State to disk after each
 * finalized block so that restarts do not need to replay the full chain.
 *
 * File format:
 * NODO_STATE_SNAPSHOT_V1\n<blockHeight>\n<accountStateRoot>\n<serializedState>
 *
 * Security: the snapshot is verified by block height, canonical State decoding
 * and account-state-root recomputation. If any check fails, the snapshot is
 * discarded and full rebuild is used. The snapshot is a cache; the chain blocks
 * are always the authoritative source.
 */
class StateSnapshotStore {
public:
    explicit StateSnapshotStore(std::filesystem::path snapshotPath);

    // Save current state after a finalized block at blockHeight.
    // Returns true on success; false if the state height mismatches blockHeight
    // or if an I/O error occurs.
    bool save(const core::State& state, std::uint64_t blockHeight);

    // Load snapshot if it exists and its blockHeight matches expectedHeight.
    // Returns nullopt if snapshot is absent, stale, or corrupt.
    std::optional<core::State> load(std::uint64_t expectedHeight) const;

    bool exists() const;
    void remove();

    // Returns the block height stored in the snapshot, or 0 if no valid snapshot.
    std::uint64_t snapshotHeight() const;

    std::filesystem::path path() const;

private:
    std::filesystem::path m_path;

    static constexpr const char* HEADER = "NODO_STATE_SNAPSHOT_V1";
};

} // namespace nodo::node

#endif
