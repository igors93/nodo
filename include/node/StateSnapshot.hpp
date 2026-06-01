#ifndef NODO_NODE_STATE_SNAPSHOT_HPP
#define NODO_NODE_STATE_SNAPSHOT_HPP

#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateCommitment.hpp"
#include "core/ValidatorRegistry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class StateSnapshot {
public:
    StateSnapshot();

    StateSnapshot(
        std::string schemaVersion,
        std::uint64_t blockHeight,
        std::string blockHash,
        core::StateCommitment commitment,
        std::vector<core::AccountState> accounts,
        std::string validatorRegistryDigest,
        std::int64_t createdAt
    );

    const std::string& schemaVersion() const;
    std::uint64_t blockHeight() const;
    const std::string& blockHash() const;
    const core::StateCommitment& commitment() const;
    const std::vector<core::AccountState>& accounts() const;
    const std::string& validatorRegistryDigest() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string canonicalDigest() const;
    std::string serialize() const;

    static StateSnapshot create(
        std::uint64_t blockHeight,
        const std::string& blockHash,
        const core::AccountStateView& accountStateView,
        const core::ValidatorRegistry& validatorRegistry,
        const std::vector<core::LedgerRecord>& ledgerRecords,
        std::int64_t createdAt
    );

private:
    std::string m_schemaVersion;
    std::uint64_t m_blockHeight;
    std::string m_blockHash;
    core::StateCommitment m_commitment;
    std::vector<core::AccountState> m_accounts;
    std::string m_validatorRegistryDigest;
    std::int64_t m_createdAt;
};

} // namespace nodo::node

#endif
