#include "node/FastSyncSnapshot.hpp"

#include "core/StateRootCalculator.hpp"
#include "node/NodeRuntime.hpp"
#include "serialization/CanonicalHash.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "utils/Amount.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

bool isCanonicalHash(const std::string &value) {
  if (value.size() != 64) {
    return false;
  }
  bool nonZero = false;
  for (const char c : value) {
    const bool digit = c >= '0' && c <= '9';
    const bool lowerHex = c >= 'a' && c <= 'f';
    if (!digit && !lowerHex) {
      return false;
    }
    if (c != '0') {
      nonZero = true;
    }
  }
  return nonZero;
}

bool isSafeScalar(const std::string &value) {
  if (value.empty()) {
    return false;
  }
  for (const char c : value) {
    const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                         c == '.' || c == ':' || c == '#';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

std::uint64_t parseU64(const std::string &value, const std::string &field) {
  if (value.empty()) {
    throw std::invalid_argument("empty uint64 field: " + field);
  }
  for (const char c : value) {
    if (c < '0' || c > '9') {
      throw std::invalid_argument("malformed uint64 field: " + field);
    }
  }
  std::size_t used = 0;
  const unsigned long long parsed = std::stoull(value, &used);
  if (used != value.size()) {
    throw std::invalid_argument("malformed uint64 field: " + field);
  }
  return static_cast<std::uint64_t>(parsed);
}

std::int64_t parseI64(const std::string &value, const std::string &field) {
  if (value.empty()) {
    throw std::invalid_argument("empty int64 field: " + field);
  }
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char c = value[i];
    if (c == '-' && i == 0 && value.size() > 1) {
      continue;
    }
    if (c < '0' || c > '9') {
      throw std::invalid_argument("malformed int64 field: " + field);
    }
  }
  std::size_t used = 0;
  const long long parsed = std::stoll(value, &used);
  if (used != value.size()) {
    throw std::invalid_argument("malformed int64 field: " + field);
  }
  return static_cast<std::int64_t>(parsed);
}

std::vector<core::AccountState>
sortedAccounts(std::vector<core::AccountState> accounts) {
  std::sort(
      accounts.begin(), accounts.end(),
      [](const core::AccountState &left, const core::AccountState &right) {
        return left.address() < right.address();
      });
  return accounts;
}

std::string
digestProtocolDomains(const std::map<std::string, std::string> &domains) {
  nodo::serialization::CanonicalWriter writer;
  writer.writeUInt32(static_cast<std::uint32_t>(domains.size()));
  for (const auto &[name, digest] : domains) {
    writer.writeString(name);
    writer.writeString(digest);
  }
  return nodo::serialization::CanonicalHash::hashBytes(
      writer.bytes(), "NODO_FAST_SYNC_PROTOCOL_DOMAIN_DIGEST_V1");
}

std::int64_t checkedMinimumFee(const config::GenesisConfig &genesisConfig) {
  const std::uint64_t raw =
      genesisConfig.networkParameters().minimumFeeRawUnits();
  if (raw >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error("minimum fee exceeds supported Amount range");
  }
  return static_cast<std::int64_t>(raw);
}

} // namespace

FastSyncSnapshot::FastSyncSnapshot()
    : m_genesisConfigId(""), m_chainId(""), m_networkName(""), m_blockHeight(0),
      m_blockHash(""), m_stateRoot(""), m_accountRoot(""),
      m_protocolDomainDigest(""), m_accounts(), m_createdAt(0) {}

FastSyncSnapshot::FastSyncSnapshot(
    std::string genesisConfigId, std::string chainId, std::string networkName,
    std::uint64_t blockHeight, std::string blockHash, std::string stateRoot,
    std::string accountRoot, std::string protocolDomainDigest,
    std::vector<core::AccountState> accounts, std::int64_t createdAt)
    : m_genesisConfigId(std::move(genesisConfigId)),
      m_chainId(std::move(chainId)), m_networkName(std::move(networkName)),
      m_blockHeight(blockHeight), m_blockHash(std::move(blockHash)),
      m_stateRoot(std::move(stateRoot)), m_accountRoot(std::move(accountRoot)),
      m_protocolDomainDigest(std::move(protocolDomainDigest)),
      m_accounts(sortedAccounts(std::move(accounts))), m_createdAt(createdAt) {}

const std::string &FastSyncSnapshot::genesisConfigId() const {
  return m_genesisConfigId;
}
const std::string &FastSyncSnapshot::chainId() const { return m_chainId; }
const std::string &FastSyncSnapshot::networkName() const {
  return m_networkName;
}
std::uint64_t FastSyncSnapshot::blockHeight() const { return m_blockHeight; }
const std::string &FastSyncSnapshot::blockHash() const { return m_blockHash; }
const std::string &FastSyncSnapshot::stateRoot() const { return m_stateRoot; }
const std::string &FastSyncSnapshot::accountRoot() const {
  return m_accountRoot;
}
const std::string &FastSyncSnapshot::protocolDomainDigest() const {
  return m_protocolDomainDigest;
}
const std::vector<core::AccountState> &FastSyncSnapshot::accounts() const {
  return m_accounts;
}
std::int64_t FastSyncSnapshot::createdAt() const { return m_createdAt; }

core::AccountStateView FastSyncSnapshot::accountStateView() const {
  core::AccountStateView view;
  for (const core::AccountState &account : m_accounts) {
    if (!view.putAccount(account)) {
      throw std::logic_error(
          "Fast-sync snapshot contains invalid account state.");
    }
  }
  return view;
}

bool FastSyncSnapshot::isValid() const {
  if (!isSafeScalar(m_genesisConfigId) || !isSafeScalar(m_chainId) ||
      !isSafeScalar(m_networkName) || m_blockHeight == 0 ||
      !isCanonicalHash(m_blockHash) || !isCanonicalHash(m_stateRoot) ||
      !isCanonicalHash(m_accountRoot) ||
      !isCanonicalHash(m_protocolDomainDigest) || m_createdAt <= 0) {
    return false;
  }

  std::set<std::string> seenAccounts;
  for (const core::AccountState &account : m_accounts) {
    if (!account.isValid()) {
      return false;
    }
    if (!seenAccounts.insert(account.address()).second) {
      return false;
    }
  }

  try {
    return core::StateRootCalculator::calculateAccountStateRoot(
               accountStateView()) == m_accountRoot;
  } catch (...) {
    return false;
  }
}

std::string FastSyncSnapshot::digest() const {
  nodo::serialization::CanonicalWriter writer;
  writer.writeString(SCHEMA_VERSION);
  writer.writeString(m_genesisConfigId);
  writer.writeString(m_chainId);
  writer.writeString(m_networkName);
  writer.writeUInt64(m_blockHeight);
  writer.writeString(m_blockHash);
  writer.writeString(m_stateRoot);
  writer.writeString(m_accountRoot);
  writer.writeString(m_protocolDomainDigest);
  writer.writeUInt32(static_cast<std::uint32_t>(m_accounts.size()));
  for (const core::AccountState &account : m_accounts) {
    writer.writeString(account.address());
    writer.writeString(std::to_string(account.balance().rawUnits()));
    writer.writeUInt64(account.nonce());
  }
  return nodo::serialization::CanonicalHash::hashBytes(
      writer.bytes(), "NODO_FAST_SYNC_SNAPSHOT_DIGEST_V1");
}

std::string FastSyncSnapshot::serialize() const {
  std::vector<std::pair<std::string, std::string>> fields;
  fields.emplace_back("genesisConfigId", m_genesisConfigId);
  fields.emplace_back("chainId", m_chainId);
  fields.emplace_back("networkName", m_networkName);
  fields.emplace_back("blockHeight", std::to_string(m_blockHeight));
  fields.emplace_back("blockHash", m_blockHash);
  fields.emplace_back("stateRoot", m_stateRoot);
  fields.emplace_back("accountRoot", m_accountRoot);
  fields.emplace_back("protocolDomainDigest", m_protocolDomainDigest);
  fields.emplace_back("createdAt", std::to_string(m_createdAt));
  fields.emplace_back("accountCount", std::to_string(m_accounts.size()));

  for (std::size_t i = 0; i < m_accounts.size(); ++i) {
    const std::string prefix = "account." + std::to_string(i) + ".";
    fields.emplace_back(prefix + "address", m_accounts[i].address());
    fields.emplace_back(prefix + "balanceRaw",
                        std::to_string(m_accounts[i].balance().rawUnits()));
    fields.emplace_back(prefix + "nonce",
                        std::to_string(m_accounts[i].nonce()));
  }

  fields.emplace_back("snapshotDigest", digest());
  return serialization::KeyValueFileCodec::serialize(SCHEMA_VERSION, fields);
}

FastSyncSnapshot FastSyncSnapshot::deserialize(const std::string &serialized) {
  const serialization::KeyValueFileDocument doc =
      serialization::KeyValueFileCodec::parse(serialized, SCHEMA_VERSION);

  const std::uint64_t accountCount =
      parseU64(doc.requireField("accountCount"), "accountCount");

  std::set<std::string> allowed = {"genesisConfigId", "chainId",
                                   "networkName",     "blockHeight",
                                   "blockHash",       "stateRoot",
                                   "accountRoot",     "protocolDomainDigest",
                                   "createdAt",       "accountCount",
                                   "snapshotDigest"};
  for (std::uint64_t i = 0; i < accountCount; ++i) {
    const std::string prefix = "account." + std::to_string(i) + ".";
    allowed.insert(prefix + "address");
    allowed.insert(prefix + "balanceRaw");
    allowed.insert(prefix + "nonce");
  }
  doc.requireOnlyFields(allowed);

  std::vector<core::AccountState> accounts;
  accounts.reserve(static_cast<std::size_t>(accountCount));
  for (std::uint64_t i = 0; i < accountCount; ++i) {
    const std::string prefix = "account." + std::to_string(i) + ".";
    accounts.emplace_back(
        doc.requireField(prefix + "address"),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(prefix + "balanceRaw"), prefix + "balanceRaw")),
        parseU64(doc.requireField(prefix + "nonce"), prefix + "nonce"));
  }

  FastSyncSnapshot snapshot(
      doc.requireField("genesisConfigId"), doc.requireField("chainId"),
      doc.requireField("networkName"),
      parseU64(doc.requireField("blockHeight"), "blockHeight"),
      doc.requireField("blockHash"), doc.requireField("stateRoot"),
      doc.requireField("accountRoot"), doc.requireField("protocolDomainDigest"),
      std::move(accounts),
      parseI64(doc.requireField("createdAt"), "createdAt"));

  if (!snapshot.isValid()) {
    throw std::invalid_argument(
        "Fast-sync snapshot failed structural validation.");
  }
  if (snapshot.digest() != doc.requireField("snapshotDigest")) {
    throw std::invalid_argument("Fast-sync snapshot digest mismatch.");
  }
  if (snapshot.serialize() != serialized) {
    throw std::invalid_argument(
        "Fast-sync snapshot serialization is not canonical.");
  }
  return snapshot;
}

FastSyncSnapshot FastSyncSnapshot::fromReplayState(
    const config::GenesisConfig &genesisConfig, std::uint64_t blockHeight,
    const std::string &blockHash, const ProtocolReplayState &replayState,
    std::int64_t createdAt) {
  if (!genesisConfig.isValid() || blockHeight == 0 ||
      !core::Block::isCanonicalCommitmentRoot(blockHash) || createdAt <= 0) {
    throw std::invalid_argument(
        "Cannot build fast-sync snapshot from invalid metadata.");
  }
  if (!replayState.accounts.isValid() || replayState.stateRoot.empty()) {
    throw std::invalid_argument(
        "Cannot build fast-sync snapshot from invalid replay state.");
  }

  const std::string accountRoot =
      core::StateRootCalculator::calculateAccountStateRoot(
          replayState.accounts);
  const std::string protocolDigest =
      digestProtocolDomains(protocolExecutionDomains(replayState.execution));

  return FastSyncSnapshot(genesisConfig.deterministicId(),
                          genesisConfig.networkParameters().chainId(),
                          genesisConfig.networkParameters().networkName(),
                          blockHeight, blockHash, replayState.stateRoot,
                          accountRoot, protocolDigest,
                          replayState.accounts.accounts(), createdAt);
}

FastSyncSnapshot FastSyncSnapshot::fromRuntime(const NodeRuntime &runtime,
                                               std::int64_t createdAt) {
  if (!runtime.isValid()) {
    throw std::invalid_argument(
        "Cannot build fast-sync snapshot from invalid runtime.");
  }
  const ProtocolReplayState replay = ProtocolStateTransition::replayToTip(
      runtime.config().genesisConfig(), runtime.blockchain(),
      checkedMinimumFee(runtime.config().genesisConfig()));
  return fromReplayState(runtime.config().genesisConfig(),
                         runtime.blockchain().latestBlock().index(),
                         runtime.blockchain().latestBlock().hash(), replay,
                         createdAt);
}

} // namespace nodo::node
