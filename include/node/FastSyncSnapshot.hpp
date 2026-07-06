#ifndef NODO_NODE_FAST_SYNC_SNAPSHOT_HPP
#define NODO_NODE_FAST_SYNC_SNAPSHOT_HPP

#include "config/NetworkParameters.hpp"
#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/Block.hpp"
#include "node/ProtocolStateTransition.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * FastSyncSnapshot is the portable, canonical snapshot payload used by Nodo's
 * fast-sync pipeline.
 *
 * It intentionally stores a full account-state boundary plus protocol-domain
 * commitment roots.  The account state is materialized because it is required
 * for transaction admission and post-snapshot block execution; every protocol
 * domain is represented by the exact domain digest that participates in the
 * protocol state root.  A future domain-specific hydrator can add structured
 * domain payloads without changing the trust model: the verifier already binds
 * the snapshot to chain id, genesis id, finalized block hash and state root.
 */
class FastSyncSnapshot {
public:
  static constexpr const char *SCHEMA_VERSION = "NODO_FAST_SYNC_SNAPSHOT_V1";

  FastSyncSnapshot();

  FastSyncSnapshot(std::string genesisConfigId, std::string chainId,
                   std::string networkName, std::uint64_t blockHeight,
                   std::string blockHash, std::string stateRoot,
                   std::string accountRoot, std::string protocolDomainDigest,
                   std::vector<core::AccountState> accounts,
                   std::int64_t createdAt);

  const std::string &genesisConfigId() const;
  const std::string &chainId() const;
  const std::string &networkName() const;
  std::uint64_t blockHeight() const;
  const std::string &blockHash() const;
  const std::string &stateRoot() const;
  const std::string &accountRoot() const;
  const std::string &protocolDomainDigest() const;
  const std::vector<core::AccountState> &accounts() const;
  std::int64_t createdAt() const;

  core::AccountStateView accountStateView() const;

  bool isValid() const;
  std::string digest() const;
  std::string serialize() const;

  static FastSyncSnapshot deserialize(const std::string &serialized);

  static FastSyncSnapshot
  fromReplayState(const config::GenesisConfig &genesisConfig,
                  std::uint64_t blockHeight, const std::string &blockHash,
                  const ProtocolReplayState &replayState,
                  std::int64_t createdAt);

  static FastSyncSnapshot fromRuntime(const NodeRuntime &runtime,
                                      std::int64_t createdAt);

private:
  std::string m_genesisConfigId;
  std::string m_chainId;
  std::string m_networkName;
  std::uint64_t m_blockHeight;
  std::string m_blockHash;
  std::string m_stateRoot;
  std::string m_accountRoot;
  std::string m_protocolDomainDigest;
  std::vector<core::AccountState> m_accounts;
  std::int64_t m_createdAt;
};

} // namespace nodo::node

#endif
