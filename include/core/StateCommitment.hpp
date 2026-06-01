#ifndef NODO_CORE_STATE_COMMITMENT_HPP
#define NODO_CORE_STATE_COMMITMENT_HPP

#include "core/AccountStateView.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ValidatorRegistry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * StateCommitment is the first explicit state-root primitive for Nodo. It does
 * not mutate state. It deterministically commits to account state, ledger
 * records and validator registry data at one finalized block boundary.
 */
class StateCommitment {
public:
    StateCommitment();

    StateCommitment(
        std::string schemaVersion,
        std::uint64_t blockHeight,
        std::string blockHash,
        std::string accountRoot,
        std::string ledgerRoot,
        std::string validatorRoot,
        std::string finalizedStateRoot,
        std::int64_t createdAt
    );

    const std::string& schemaVersion() const;
    std::uint64_t blockHeight() const;
    const std::string& blockHash() const;
    const std::string& accountRoot() const;
    const std::string& ledgerRoot() const;
    const std::string& validatorRoot() const;
    const std::string& finalizedStateRoot() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string serialize() const;

    static std::string hashAccounts(
        const std::vector<AccountState>& accounts
    );

    static std::string hashLedgerRecords(
        const std::vector<LedgerRecord>& records
    );

    static std::string hashValidatorRegistry(
        const ValidatorRegistry& validatorRegistry
    );

    static std::string combineRoots(
        const std::string& accountRoot,
        const std::string& ledgerRoot,
        const std::string& validatorRoot,
        std::uint64_t blockHeight,
        const std::string& blockHash
    );

    static StateCommitment calculate(
        std::uint64_t blockHeight,
        const std::string& blockHash,
        const AccountStateView& accountStateView,
        const std::vector<LedgerRecord>& ledgerRecords,
        const ValidatorRegistry& validatorRegistry,
        std::int64_t createdAt
    );

private:
    std::string m_schemaVersion;
    std::uint64_t m_blockHeight;
    std::string m_blockHash;
    std::string m_accountRoot;
    std::string m_ledgerRoot;
    std::string m_validatorRoot;
    std::string m_finalizedStateRoot;
    std::int64_t m_createdAt;
};

} // namespace nodo::core

#endif
