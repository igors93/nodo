#ifndef NODO_STORAGE_ACCOUNT_STATE_SNAPSHOT_STORE_HPP
#define NODO_STORAGE_ACCOUNT_STATE_SNAPSHOT_STORE_HPP

#include "core/AccountStateView.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace nodo::storage {

/*
 * AccountStateSnapshot bundles the pre-computed account state for a specific
 * finalized block height along with the metadata needed to verify that it still
 * matches the on-disk chain before trusting it for a partial-replay startup.
 */
class AccountStateSnapshot {
public:
    AccountStateSnapshot();

    AccountStateSnapshot(
        std::string genesisConfigId,
        std::uint64_t height,
        std::string blockHash,
        core::AccountStateView view
    );

    const std::string& genesisConfigId() const;
    std::uint64_t height() const;
    const std::string& blockHash() const;
    const core::AccountStateView& view() const;

    bool isValid() const;

private:
    std::string m_genesisConfigId;
    std::uint64_t m_height;
    std::string m_blockHash;
    core::AccountStateView m_view;
};

/*
 * AccountStateSnapshotStore persists and loads AccountStateSnapshot files.
 *
 * File format (plain text, line-oriented):
 *   NODO_ACCOUNT_SNAPSHOT_V1
 *   genesisConfigId=<id>
 *   height=<n>
 *   blockHash=<hash>
 *   <address>\t<balanceRaw>\t<nonce>
 *   ...
 *
 * Files are written atomically via AtomicFile so a crash mid-write never
 * produces a corrupt snapshot — the previous file is untouched.
 */
class AccountStateSnapshotStore {
public:
    static constexpr const char* kSnapshotFileName = "account_state_snapshot.txt";

    explicit AccountStateSnapshotStore(
        const std::filesystem::path& dataRoot
    );

    // Persist the snapshot. Overwrites any existing snapshot file.
    void save(const AccountStateSnapshot& snapshot) const;

    // Load the snapshot. Returns std::nullopt if no file exists or it is corrupt.
    std::optional<AccountStateSnapshot> load() const;

    // Return the full path to the snapshot file.
    std::filesystem::path snapshotPath() const;

private:
    std::filesystem::path m_dataRoot;
};

} // namespace nodo::storage

#endif
