#ifndef NODO_CONFIG_NETWORK_PARAMETERS_HPP
#define NODO_CONFIG_NETWORK_PARAMETERS_HPP

#include "core/Blockchain.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/PublicKey.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::config {

/*
 * NetworkClass classifies the intended operational scope of a network profile.
 *
 * Security principle:
 * Code that is only safe on development networks must be guarded by network
 * class rather than by a fragile string comparison. Readiness and key policy
 * checks must also consult network class.
 */
enum class NetworkClass {
  DEVELOPMENT_LOCAL,
  STAGING_CANDIDATE,
  LOCKED_PRODUCTION
};

std::string networkClassToString(NetworkClass nc);

/*
 * NetworkParameters defines the immutable safety limits for one Nodo network.
 *
 * Security principle:
 * Nodes must agree on these parameters before they trust each other's blocks,
 * votes and sync messages.
 */
class NetworkParameters {
public:
  NetworkParameters();

  NetworkParameters(
      std::string chainId, std::string networkName, std::string protocolVersion,
      std::uint64_t epochDurationSeconds, std::uint64_t minimumValidatorCount,
      std::uint64_t quorumThresholdNumerator,
      std::uint64_t quorumThresholdDenominator,
      std::uint64_t maxTransactionsPerBlock, std::uint64_t maxPeerCount,
      std::uint64_t maxMempoolTransactions = 10000,
      std::uint64_t minimumFeeRawUnits = 0,
      std::uint64_t targetBlockTimeSeconds = 60,
      std::uint64_t finalityDepth = 1,
      std::string signatureAlgorithm = "NODO_CRYPTO_SUITE_V1",
      std::string storageFormatVersion = "NODO_STORAGE_V2",
      std::uint64_t proposalTimeoutMs = 3000,
      std::uint64_t prevoteTimeoutMs = 3000,
      std::uint64_t precommitTimeoutMs = 3000,
      std::uint32_t maxGossipMessagesPerPeerWindow = 100,
      std::uint32_t maxTransactionGossipPerPeerWindow = 50,
      std::uint32_t maxTransactionRelayPerSecond = 20,
      std::uint32_t doubleVoteSlashFractionBasisPoints = 500,
      std::uint32_t proposerEquivocationSlashFractionBasisPoints = 1000,
      std::uint32_t epochSlashCapBasisPoints = 5000);

  const std::string &chainId() const;
  const std::string &networkName() const;
  const std::string &protocolVersion() const;
  std::uint64_t epochDurationSeconds() const;
  std::uint64_t minimumValidatorCount() const;
  std::uint64_t quorumThresholdNumerator() const;
  std::uint64_t quorumThresholdDenominator() const;
  std::uint64_t maxTransactionsPerBlock() const;
  std::uint64_t maxPeerCount() const;
  std::uint64_t maxMempoolTransactions() const;
  std::uint64_t minimumFeeRawUnits() const;
  std::uint64_t targetBlockTimeSeconds() const;
  std::uint64_t finalityDepth() const;
  const std::string &signatureAlgorithm() const;
  const std::string &storageFormatVersion() const;
  std::uint64_t proposalTimeoutMs() const;
  std::uint64_t prevoteTimeoutMs() const;
  std::uint64_t precommitTimeoutMs() const;
  std::uint32_t maxGossipMessagesPerPeerWindow() const;
  std::uint32_t maxTransactionGossipPerPeerWindow() const;
  std::uint32_t maxTransactionRelayPerSecond() const;
  std::uint32_t doubleVoteSlashFractionBasisPoints() const;
  std::uint32_t proposerEquivocationSlashFractionBasisPoints() const;
  std::uint32_t epochSlashCapBasisPoints() const;

  bool isValid() const;

  // Derived from networkName. No storage overhead.
  NetworkClass networkClass() const;

  std::string deterministicId() const;
  std::string serialize() const;

  static NetworkParameters developmentLocal();
  static NetworkParameters developmentSoak();
  static NetworkParameters testnetCandidate();

private:
  std::string m_chainId;
  std::string m_networkName;
  std::string m_protocolVersion;
  std::uint64_t m_epochDurationSeconds;
  std::uint64_t m_minimumValidatorCount;
  std::uint64_t m_quorumThresholdNumerator;
  std::uint64_t m_quorumThresholdDenominator;
  std::uint64_t m_maxTransactionsPerBlock;
  std::uint64_t m_maxPeerCount;
  std::uint64_t m_maxMempoolTransactions;
  std::uint64_t m_minimumFeeRawUnits;
  std::uint64_t m_targetBlockTimeSeconds;
  std::uint64_t m_finalityDepth;
  std::string m_signatureAlgorithm;
  std::string m_storageFormatVersion;
  std::uint64_t m_proposalTimeoutMs;
  std::uint64_t m_prevoteTimeoutMs;
  std::uint64_t m_precommitTimeoutMs;
  std::uint32_t m_maxGossipMessagesPerPeerWindow;
  std::uint32_t m_maxTransactionGossipPerPeerWindow;
  std::uint32_t m_maxTransactionRelayPerSecond;
  std::uint32_t m_doubleVoteSlashFractionBasisPoints;
  std::uint32_t m_proposerEquivocationSlashFractionBasisPoints;
  std::uint32_t m_epochSlashCapBasisPoints;
};

/*
 * BootstrapValidatorConfig describes a validator identity present at genesis.
 *
 * It does not mint coins. It only provides the first validator identities that
 * can protect the chain during local/bootstrap operation.
 *
 * ownerAddress is a separate Ed25519 identity authorized to sign owner-gated
 * operations on the validator's behalf (governance votes, exit/unjail
 * requests) — the same owner/validator split already used for validators
 * registered through a VALIDATOR_REGISTER transaction. Mempool transactions
 * are only ever verified under an Ed25519-only security context, so a
 * validator can never sign those operations with its own BLS consensus key.
 * When left empty, the validator has no distinct owner and cannot cast an
 * authorized owner-gated transaction — this is the pre-existing default and
 * remains valid for validator sets that never touch governance.
 */
class BootstrapValidatorConfig {
public:
  BootstrapValidatorConfig();

  BootstrapValidatorConfig(crypto::PublicKey validatorPublicKey,
                           std::uint64_t activationEpoch,
                           std::uint32_t bootstrapWeight,
                           std::string metadataHash,
                           std::string ownerAddress = "");

  const crypto::PublicKey &validatorPublicKey() const;
  std::uint64_t activationEpoch() const;
  std::uint32_t bootstrapWeight() const;
  const std::string &metadataHash() const;
  const std::string &ownerAddress() const;

  std::string validatorAddress() const;

  // ownerAddress() if explicitly configured, otherwise validatorAddress().
  // This is the address genesis registration actually grants ownership to.
  std::string effectiveOwnerAddress() const;

  bool isValid() const;
  std::string serialize() const;

private:
  crypto::PublicKey m_validatorPublicKey;
  std::uint64_t m_activationEpoch;
  std::uint32_t m_bootstrapWeight;
  std::string m_metadataHash;
  std::string m_ownerAddress;
};

class GenesisAccountConfig {
public:
  GenesisAccountConfig();

  GenesisAccountConfig(std::string address, utils::Amount balance,
                       std::uint64_t nonce);

  const std::string &address() const;
  utils::Amount balance() const;
  std::uint64_t nonce() const;

  bool isValid() const;
  std::string serialize() const;

private:
  std::string m_address;
  utils::Amount m_balance;
  std::uint64_t m_nonce;
};

class GenesisConfig {
public:
  GenesisConfig();

  GenesisConfig(NetworkParameters networkParameters,
                std::int64_t genesisTimestamp,
                std::vector<BootstrapValidatorConfig> bootstrapValidators,
                std::string genesisMemo);

  GenesisConfig(NetworkParameters networkParameters,
                std::int64_t genesisTimestamp,
                std::vector<BootstrapValidatorConfig> bootstrapValidators,
                std::vector<GenesisAccountConfig> genesisAccounts,
                std::string genesisMemo);

  const NetworkParameters &networkParameters() const;
  std::int64_t genesisTimestamp() const;
  const std::vector<BootstrapValidatorConfig> &bootstrapValidators() const;
  const std::vector<GenesisAccountConfig> &genesisAccounts() const;
  const std::string &genesisMemo() const;

  bool isValid() const;

  std::string deterministicId() const;
  std::string serialize() const;

private:
  NetworkParameters m_networkParameters;
  std::int64_t m_genesisTimestamp;
  std::vector<BootstrapValidatorConfig> m_bootstrapValidators;
  std::vector<GenesisAccountConfig> m_genesisAccounts;
  std::string m_genesisMemo;
};

enum class GenesisBuildStatus {
  BUILT,
  INVALID_CONFIG,
  INVALID_VALIDATOR_REGISTRY,
  INVALID_BLOCKCHAIN
};

std::string genesisBuildStatusToString(GenesisBuildStatus status);

class GenesisBuildResult {
public:
  GenesisBuildResult();

  static GenesisBuildResult built(core::Blockchain blockchain,
                                  core::ValidatorRegistry validatorRegistry);

  static GenesisBuildResult rejected(GenesisBuildStatus status,
                                     std::string reason);

  GenesisBuildStatus status() const;
  const std::string &reason() const;
  bool built() const;

  const core::Blockchain &blockchain() const;
  const core::ValidatorRegistry &validatorRegistry() const;

  std::string serialize() const;

private:
  GenesisBuildStatus m_status;
  std::string m_reason;
  core::Blockchain m_blockchain;
  core::ValidatorRegistry m_validatorRegistry;
};

/*
 * GenesisBuilder creates the first local chain and validator registry from a
 * deterministic genesis config.
 */
class GenesisBuilder {
public:
  static GenesisBuildResult build(const GenesisConfig &genesisConfig);
};

} // namespace nodo::config

#endif
