#include "core/StateCommitment.hpp"

#include "serialization/CanonicalHash.hpp"
#include "serialization/CanonicalWriter.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace nodo::core {

namespace {

constexpr const char* STATE_COMMITMENT_SCHEMA_VERSION =
    "NODO_STATE_COMMITMENT_V1";

bool looksLikeHash(
    const std::string& value
) {
    if (value.size() != 64) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f');

        if (!allowed) {
            return false;
        }
    }

    return true;
}

} // namespace

StateCommitment::StateCommitment()
    : m_schemaVersion(""),
      m_blockHeight(0),
      m_blockHash(""),
      m_accountRoot(""),
      m_ledgerRoot(""),
      m_validatorRoot(""),
      m_finalizedStateRoot(""),
      m_createdAt(0) {}

StateCommitment::StateCommitment(
    std::string schemaVersion,
    std::uint64_t blockHeight,
    std::string blockHash,
    std::string accountRoot,
    std::string ledgerRoot,
    std::string validatorRoot,
    std::string finalizedStateRoot,
    std::int64_t createdAt
) : m_schemaVersion(std::move(schemaVersion)),
    m_blockHeight(blockHeight),
    m_blockHash(std::move(blockHash)),
    m_accountRoot(std::move(accountRoot)),
    m_ledgerRoot(std::move(ledgerRoot)),
    m_validatorRoot(std::move(validatorRoot)),
    m_finalizedStateRoot(std::move(finalizedStateRoot)),
    m_createdAt(createdAt) {}

const std::string& StateCommitment::schemaVersion() const { return m_schemaVersion; }
std::uint64_t StateCommitment::blockHeight() const { return m_blockHeight; }
const std::string& StateCommitment::blockHash() const { return m_blockHash; }
const std::string& StateCommitment::accountRoot() const { return m_accountRoot; }
const std::string& StateCommitment::ledgerRoot() const { return m_ledgerRoot; }
const std::string& StateCommitment::validatorRoot() const { return m_validatorRoot; }
const std::string& StateCommitment::finalizedStateRoot() const { return m_finalizedStateRoot; }
std::int64_t StateCommitment::createdAt() const { return m_createdAt; }

bool StateCommitment::isValid() const {
    return m_schemaVersion == STATE_COMMITMENT_SCHEMA_VERSION &&
           m_blockHeight > 0 &&
           looksLikeHash(m_blockHash) &&
           looksLikeHash(m_accountRoot) &&
           looksLikeHash(m_ledgerRoot) &&
           looksLikeHash(m_validatorRoot) &&
           looksLikeHash(m_finalizedStateRoot) &&
           m_finalizedStateRoot == combineRoots(
               m_accountRoot,
               m_ledgerRoot,
               m_validatorRoot,
               m_blockHeight,
               m_blockHash
           ) &&
           m_createdAt > 0;
}

std::string StateCommitment::serialize() const {
    std::ostringstream output;
    output << "StateCommitment{"
           << "schemaVersion=" << m_schemaVersion
           << ";blockHeight=" << m_blockHeight
           << ";blockHash=" << m_blockHash
           << ";accountRoot=" << m_accountRoot
           << ";ledgerRoot=" << m_ledgerRoot
           << ";validatorRoot=" << m_validatorRoot
           << ";finalizedStateRoot=" << m_finalizedStateRoot
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

std::string StateCommitment::hashAccounts(
    const std::vector<AccountState>& accounts
) {
    std::vector<AccountState> sortedAccounts = accounts;

    std::sort(
        sortedAccounts.begin(),
        sortedAccounts.end(),
        [](const AccountState& left, const AccountState& right) {
            return left.address() < right.address();
        }
    );

    nodo::serialization::CanonicalWriter writer;
    writer.writeString("NODO_ACCOUNT_ROOT_INPUT_V1");
    writer.writeUInt32(static_cast<std::uint32_t>(sortedAccounts.size()));

    for (const auto& account : sortedAccounts) {
        writer.writeString(account.serialize());
    }

    return nodo::serialization::CanonicalHash::hashBytes(
        writer.bytes(),
        "NODO_ACCOUNT_ROOT_V1"
    );
}

std::string StateCommitment::hashLedgerRecords(
    const std::vector<LedgerRecord>& records
) {
    nodo::serialization::CanonicalWriter writer;
    writer.writeString("NODO_LEDGER_ROOT_INPUT_V1");
    writer.writeUInt32(static_cast<std::uint32_t>(records.size()));

    for (const auto& record : records) {
        writer.writeString(record.serialize());
    }

    return nodo::serialization::CanonicalHash::hashBytes(
        writer.bytes(),
        "NODO_LEDGER_ROOT_V1"
    );
}

std::string StateCommitment::hashValidatorRegistry(
    const ValidatorRegistry& validatorRegistry
) {
    nodo::serialization::CanonicalWriter writer;
    writer.writeString("NODO_VALIDATOR_ROOT_INPUT_V1");
    writer.writeString(validatorRegistry.serialize());

    return nodo::serialization::CanonicalHash::hashBytes(
        writer.bytes(),
        "NODO_VALIDATOR_ROOT_V1"
    );
}

std::string StateCommitment::combineRoots(
    const std::string& accountRoot,
    const std::string& ledgerRoot,
    const std::string& validatorRoot,
    std::uint64_t blockHeight,
    const std::string& blockHash
) {
    nodo::serialization::CanonicalWriter writer;
    writer.writeString("NODO_FINALIZED_STATE_ROOT_INPUT_V1");
    writer.writeUInt64(blockHeight);
    writer.writeString(blockHash);
    writer.writeString(accountRoot);
    writer.writeString(ledgerRoot);
    writer.writeString(validatorRoot);

    return nodo::serialization::CanonicalHash::hashBytes(
        writer.bytes(),
        "NODO_FINALIZED_STATE_ROOT_V1"
    );
}

StateCommitment StateCommitment::calculate(
    std::uint64_t blockHeight,
    const std::string& blockHash,
    const AccountStateView& accountStateView,
    const std::vector<LedgerRecord>& ledgerRecords,
    const ValidatorRegistry& validatorRegistry,
    std::int64_t createdAt
) {
    const std::string accountRoot =
        hashAccounts(accountStateView.accounts());

    const std::string ledgerRoot =
        hashLedgerRecords(ledgerRecords);

    const std::string validatorRoot =
        hashValidatorRegistry(validatorRegistry);

    return StateCommitment(
        STATE_COMMITMENT_SCHEMA_VERSION,
        blockHeight,
        blockHash,
        accountRoot,
        ledgerRoot,
        validatorRoot,
        combineRoots(
            accountRoot,
            ledgerRoot,
            validatorRoot,
            blockHeight,
            blockHash
        ),
        createdAt
    );
}

} // namespace nodo::core
