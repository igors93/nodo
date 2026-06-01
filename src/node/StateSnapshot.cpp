#include "node/StateSnapshot.hpp"

#include "serialization/CanonicalHash.hpp"
#include "serialization/CanonicalWriter.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace nodo::node {

namespace {

constexpr const char* STATE_SNAPSHOT_SCHEMA_VERSION =
    "NODO_STATE_SNAPSHOT_V1";

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

std::vector<core::AccountState> sortedAccounts(
    std::vector<core::AccountState> accounts
) {
    std::sort(
        accounts.begin(),
        accounts.end(),
        [](const core::AccountState& left, const core::AccountState& right) {
            return left.address() < right.address();
        }
    );

    return accounts;
}

} // namespace

StateSnapshot::StateSnapshot()
    : m_schemaVersion(""),
      m_blockHeight(0),
      m_blockHash(""),
      m_commitment(),
      m_accounts(),
      m_validatorRegistryDigest(""),
      m_createdAt(0) {}

StateSnapshot::StateSnapshot(
    std::string schemaVersion,
    std::uint64_t blockHeight,
    std::string blockHash,
    core::StateCommitment commitment,
    std::vector<core::AccountState> accounts,
    std::string validatorRegistryDigest,
    std::int64_t createdAt
) : m_schemaVersion(std::move(schemaVersion)),
    m_blockHeight(blockHeight),
    m_blockHash(std::move(blockHash)),
    m_commitment(std::move(commitment)),
    m_accounts(sortedAccounts(std::move(accounts))),
    m_validatorRegistryDigest(std::move(validatorRegistryDigest)),
    m_createdAt(createdAt) {}

const std::string& StateSnapshot::schemaVersion() const { return m_schemaVersion; }
std::uint64_t StateSnapshot::blockHeight() const { return m_blockHeight; }
const std::string& StateSnapshot::blockHash() const { return m_blockHash; }
const core::StateCommitment& StateSnapshot::commitment() const { return m_commitment; }
const std::vector<core::AccountState>& StateSnapshot::accounts() const { return m_accounts; }
const std::string& StateSnapshot::validatorRegistryDigest() const { return m_validatorRegistryDigest; }
std::int64_t StateSnapshot::createdAt() const { return m_createdAt; }

bool StateSnapshot::isValid() const {
    if (m_schemaVersion != STATE_SNAPSHOT_SCHEMA_VERSION ||
        m_blockHeight == 0 ||
        m_blockHash.empty() ||
        m_createdAt <= 0 ||
        !m_commitment.isValid() ||
        m_commitment.blockHeight() != m_blockHeight ||
        m_commitment.blockHash() != m_blockHash ||
        !looksLikeHash(m_validatorRegistryDigest)) {
        return false;
    }

    std::string previousAddress;

    for (std::size_t index = 0; index < m_accounts.size(); ++index) {
        const auto& account = m_accounts[index];

        if (!account.isValid()) {
            return false;
        }

        if (index != 0 && account.address() <= previousAddress) {
            return false;
        }

        previousAddress = account.address();
    }

    return core::StateCommitment::hashAccounts(m_accounts) ==
           m_commitment.accountRoot();
}

std::string StateSnapshot::canonicalDigest() const {
    nodo::serialization::CanonicalWriter writer;
    writer.writeString(m_schemaVersion);
    writer.writeUInt64(m_blockHeight);
    writer.writeString(m_blockHash);
    writer.writeString(m_commitment.finalizedStateRoot());
    writer.writeString(m_validatorRegistryDigest);
    writer.writeUInt32(static_cast<std::uint32_t>(m_accounts.size()));

    for (const auto& account : m_accounts) {
        writer.writeString(account.serialize());
    }

    return nodo::serialization::CanonicalHash::hashBytes(
        writer.bytes(),
        "NODO_STATE_SNAPSHOT_DIGEST_V1"
    );
}

std::string StateSnapshot::serialize() const {
    std::ostringstream output;
    output << "StateSnapshot{"
           << "schemaVersion=" << m_schemaVersion
           << ";blockHeight=" << m_blockHeight
           << ";blockHash=" << m_blockHash
           << ";stateRoot=" << m_commitment.finalizedStateRoot()
           << ";validatorRegistryDigest=" << m_validatorRegistryDigest
           << ";accountCount=" << m_accounts.size()
           << ";createdAt=" << m_createdAt
           << ";snapshotDigest=" << canonicalDigest()
           << "}";
    return output.str();
}

StateSnapshot StateSnapshot::create(
    std::uint64_t blockHeight,
    const std::string& blockHash,
    const core::AccountStateView& accountStateView,
    const core::ValidatorRegistry& validatorRegistry,
    const std::vector<core::LedgerRecord>& ledgerRecords,
    std::int64_t createdAt
) {
    const core::StateCommitment commitment =
        core::StateCommitment::calculate(
            blockHeight,
            blockHash,
            accountStateView,
            ledgerRecords,
            validatorRegistry,
            createdAt
        );

    return StateSnapshot(
        STATE_SNAPSHOT_SCHEMA_VERSION,
        blockHeight,
        blockHash,
        commitment,
        accountStateView.accounts(),
        core::StateCommitment::hashValidatorRegistry(validatorRegistry),
        createdAt
    );
}

} // namespace nodo::node
