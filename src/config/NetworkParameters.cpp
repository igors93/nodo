#include "config/NetworkParameters.hpp"

#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/hash.h"
#include "economics/ValidationWorkRecord.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::config {

std::string networkClassToString(NetworkClass nc) {
  switch (nc) {
  case NetworkClass::DEVELOPMENT_LOCAL:
    return "DEVELOPMENT_LOCAL";
  case NetworkClass::STAGING_CANDIDATE:
    return "STAGING_CANDIDATE";
  case NetworkClass::LOCKED_PRODUCTION:
    return "LOCKED_PRODUCTION";
  default:
    return "UNKNOWN";
  }
}

namespace {

bool isSafeScalar(const std::string &value) {
  if (value.empty()) {
    return false;
  }

  for (const char character : value) {
    const bool allowed = (character >= 'a' && character <= 'z') ||
                         (character >= 'A' && character <= 'Z') ||
                         (character >= '0' && character <= '9') ||
                         character == '_' || character == '-' ||
                         character == '.' || character == ':' ||
                         character == '/';

    if (!allowed) {
      return false;
    }
  }

  return true;
}

std::string hashString(const std::string &value) {
  char output[NODO_HASH_BUFFER_SIZE] = {0};

  nodo_hash_string(value.c_str(), output, sizeof(output));

  return std::string(output);
}

core::LedgerRecord
genesisValidatorLedgerRecord(const GenesisConfig &config,
                             const BootstrapValidatorConfig &validator) {
  const std::string address = validator.validatorAddress();

  economics::ValidationWorkRecord record(
      address, 1, economics::ValidationWorkType::VALIDATE_BLOCK,
      economics::ValidationWorkResult::ACCEPTED,
      "genesis-target-" + config.networkParameters().chainId(),
      "genesis-validator-" + address, validator.bootstrapWeight(),
      config.genesisTimestamp());

  return core::LedgerRecord::fromValidationWorkRecord(
      record, config.genesisTimestamp());
}

} // namespace

NetworkParameters::NetworkParameters()
    : m_chainId(""), m_networkName(""), m_protocolVersion(""),
      m_epochDurationSeconds(0), m_minimumValidatorCount(0),
      m_quorumThresholdNumerator(0), m_quorumThresholdDenominator(0),
      m_maxTransactionsPerBlock(0), m_maxPeerCount(0),
      m_maxMempoolTransactions(0), m_minimumFeeRawUnits(0),
      m_targetBlockTimeSeconds(0), m_finalityDepth(0), m_signatureAlgorithm(""),
      m_storageFormatVersion(""), m_proposalTimeoutMs(0), m_prevoteTimeoutMs(0),
      m_precommitTimeoutMs(0), m_maxGossipMessagesPerPeerWindow(0),
      m_maxTransactionGossipPerPeerWindow(0), m_maxTransactionRelayPerSecond(0),
      m_doubleVoteSlashFractionBasisPoints(0),
      m_proposerEquivocationSlashFractionBasisPoints(0),
      m_epochSlashCapBasisPoints(0), m_treasuryTimelockBlocks(0),
      m_treasuryMaxSpendPerProposalRawUnits(0),
      m_treasuryMaxSpendPerEpochRawUnits(0) {}

NetworkParameters::NetworkParameters(
    std::string chainId, std::string networkName, std::string protocolVersion,
    std::uint64_t epochDurationSeconds, std::uint64_t minimumValidatorCount,
    std::uint64_t quorumThresholdNumerator,
    std::uint64_t quorumThresholdDenominator,
    std::uint64_t maxTransactionsPerBlock, std::uint64_t maxPeerCount,
    std::uint64_t maxMempoolTransactions, std::uint64_t minimumFeeRawUnits,
    std::uint64_t targetBlockTimeSeconds, std::uint64_t finalityDepth,
    std::string signatureAlgorithm, std::string storageFormatVersion,
    std::uint64_t proposalTimeoutMs, std::uint64_t prevoteTimeoutMs,
    std::uint64_t precommitTimeoutMs,
    std::uint32_t maxGossipMessagesPerPeerWindow,
    std::uint32_t maxTransactionGossipPerPeerWindow,
    std::uint32_t maxTransactionRelayPerSecond,
    std::uint32_t doubleVoteSlashFractionBasisPoints,
    std::uint32_t proposerEquivocationSlashFractionBasisPoints,
    std::uint32_t epochSlashCapBasisPoints,
    std::uint64_t treasuryTimelockBlocks,
    std::uint64_t treasuryMaxSpendPerProposalRawUnits,
    std::uint64_t treasuryMaxSpendPerEpochRawUnits)
    : m_chainId(std::move(chainId)), m_networkName(std::move(networkName)),
      m_protocolVersion(std::move(protocolVersion)),
      m_epochDurationSeconds(epochDurationSeconds),
      m_minimumValidatorCount(minimumValidatorCount),
      m_quorumThresholdNumerator(quorumThresholdNumerator),
      m_quorumThresholdDenominator(quorumThresholdDenominator),
      m_maxTransactionsPerBlock(maxTransactionsPerBlock),
      m_maxPeerCount(maxPeerCount),
      m_maxMempoolTransactions(maxMempoolTransactions),
      m_minimumFeeRawUnits(minimumFeeRawUnits),
      m_targetBlockTimeSeconds(targetBlockTimeSeconds),
      m_finalityDepth(finalityDepth),
      m_signatureAlgorithm(std::move(signatureAlgorithm)),
      m_storageFormatVersion(std::move(storageFormatVersion)),
      m_proposalTimeoutMs(proposalTimeoutMs),
      m_prevoteTimeoutMs(prevoteTimeoutMs),
      m_precommitTimeoutMs(precommitTimeoutMs),
      m_maxGossipMessagesPerPeerWindow(maxGossipMessagesPerPeerWindow),
      m_maxTransactionGossipPerPeerWindow(maxTransactionGossipPerPeerWindow),
      m_maxTransactionRelayPerSecond(maxTransactionRelayPerSecond),
      m_doubleVoteSlashFractionBasisPoints(doubleVoteSlashFractionBasisPoints),
      m_proposerEquivocationSlashFractionBasisPoints(
          proposerEquivocationSlashFractionBasisPoints),
      m_epochSlashCapBasisPoints(epochSlashCapBasisPoints),
      m_treasuryTimelockBlocks(treasuryTimelockBlocks),
      m_treasuryMaxSpendPerProposalRawUnits(
          treasuryMaxSpendPerProposalRawUnits),
      m_treasuryMaxSpendPerEpochRawUnits(treasuryMaxSpendPerEpochRawUnits) {}

const std::string &NetworkParameters::chainId() const { return m_chainId; }

const std::string &NetworkParameters::networkName() const {
  return m_networkName;
}

const std::string &NetworkParameters::protocolVersion() const {
  return m_protocolVersion;
}

std::uint64_t NetworkParameters::epochDurationSeconds() const {
  return m_epochDurationSeconds;
}

std::uint64_t NetworkParameters::minimumValidatorCount() const {
  return m_minimumValidatorCount;
}

std::uint64_t NetworkParameters::quorumThresholdNumerator() const {
  return m_quorumThresholdNumerator;
}

std::uint64_t NetworkParameters::quorumThresholdDenominator() const {
  return m_quorumThresholdDenominator;
}

std::uint64_t NetworkParameters::maxTransactionsPerBlock() const {
  return m_maxTransactionsPerBlock;
}

std::uint64_t NetworkParameters::maxPeerCount() const { return m_maxPeerCount; }

std::uint64_t NetworkParameters::maxMempoolTransactions() const {
  return m_maxMempoolTransactions;
}

std::uint64_t NetworkParameters::minimumFeeRawUnits() const {
  return m_minimumFeeRawUnits;
}

std::uint64_t NetworkParameters::targetBlockTimeSeconds() const {
  return m_targetBlockTimeSeconds;
}

std::uint64_t NetworkParameters::finalityDepth() const {
  return m_finalityDepth;
}

const std::string &NetworkParameters::signatureAlgorithm() const {
  return m_signatureAlgorithm;
}

const std::string &NetworkParameters::storageFormatVersion() const {
  return m_storageFormatVersion;
}

std::uint64_t NetworkParameters::proposalTimeoutMs() const {
  return m_proposalTimeoutMs;
}

std::uint64_t NetworkParameters::prevoteTimeoutMs() const {
  return m_prevoteTimeoutMs;
}

std::uint64_t NetworkParameters::precommitTimeoutMs() const {
  return m_precommitTimeoutMs;
}

std::uint32_t NetworkParameters::maxGossipMessagesPerPeerWindow() const {
  return m_maxGossipMessagesPerPeerWindow;
}

std::uint32_t NetworkParameters::maxTransactionGossipPerPeerWindow() const {
  return m_maxTransactionGossipPerPeerWindow;
}

std::uint32_t NetworkParameters::maxTransactionRelayPerSecond() const {
  return m_maxTransactionRelayPerSecond;
}

std::uint32_t NetworkParameters::doubleVoteSlashFractionBasisPoints() const {
  return m_doubleVoteSlashFractionBasisPoints;
}

std::uint32_t
NetworkParameters::proposerEquivocationSlashFractionBasisPoints() const {
  return m_proposerEquivocationSlashFractionBasisPoints;
}

std::uint32_t NetworkParameters::epochSlashCapBasisPoints() const {
  return m_epochSlashCapBasisPoints;
}

std::uint64_t NetworkParameters::treasuryTimelockBlocks() const {
  return m_treasuryTimelockBlocks;
}

std::uint64_t NetworkParameters::treasuryMaxSpendPerProposalRawUnits() const {
  return m_treasuryMaxSpendPerProposalRawUnits;
}

std::uint64_t NetworkParameters::treasuryMaxSpendPerEpochRawUnits() const {
  return m_treasuryMaxSpendPerEpochRawUnits;
}

NetworkClass NetworkParameters::networkClass() const {
  if (m_networkName == "localnet" || m_networkName == "localnet-soak") {
    return NetworkClass::DEVELOPMENT_LOCAL;
  }
  if (m_networkName == "testnet-candidate") {
    return NetworkClass::STAGING_CANDIDATE;
  }
  return NetworkClass::LOCKED_PRODUCTION;
}

bool NetworkParameters::isValid() const {
  if (!isSafeScalar(m_chainId) || !isSafeScalar(m_networkName) ||
      !isSafeScalar(m_protocolVersion) || !isSafeScalar(m_signatureAlgorithm) ||
      !isSafeScalar(m_storageFormatVersion)) {
    return false;
  }

  if (m_epochDurationSeconds == 0 || m_minimumValidatorCount == 0 ||
      m_maxTransactionsPerBlock == 0 || m_maxPeerCount == 0 ||
      m_maxMempoolTransactions == 0 || m_targetBlockTimeSeconds == 0 ||
      m_finalityDepth == 0) {
    return false;
  }

  if (m_quorumThresholdNumerator == 0 || m_quorumThresholdDenominator == 0 ||
      m_quorumThresholdNumerator > m_quorumThresholdDenominator) {
    return false;
  }

  if (m_proposalTimeoutMs == 0 || m_prevoteTimeoutMs == 0 ||
      m_precommitTimeoutMs == 0) {
    return false;
  }

  return true;
}

std::string NetworkParameters::deterministicId() const {
  if (!isValid()) {
    return "";
  }

  return hashString(serialize());
}

std::string NetworkParameters::serialize() const {
  std::ostringstream oss;

  oss << "NetworkParameters{"
      << "chainId=" << m_chainId << ";networkName=" << m_networkName
      << ";protocolVersion=" << m_protocolVersion
      << ";epochDurationSeconds=" << m_epochDurationSeconds
      << ";minimumValidatorCount=" << m_minimumValidatorCount
      << ";quorumThresholdNumerator=" << m_quorumThresholdNumerator
      << ";quorumThresholdDenominator=" << m_quorumThresholdDenominator
      << ";maxTransactionsPerBlock=" << m_maxTransactionsPerBlock
      << ";maxPeerCount=" << m_maxPeerCount
      << ";maxMempoolTransactions=" << m_maxMempoolTransactions
      << ";minimumFeeRawUnits=" << m_minimumFeeRawUnits
      << ";targetBlockTimeSeconds=" << m_targetBlockTimeSeconds
      << ";finalityDepth=" << m_finalityDepth
      << ";signatureAlgorithm=" << m_signatureAlgorithm
      << ";storageFormatVersion=" << m_storageFormatVersion
      << ";proposalTimeoutMs=" << m_proposalTimeoutMs
      << ";prevoteTimeoutMs=" << m_prevoteTimeoutMs
      << ";precommitTimeoutMs=" << m_precommitTimeoutMs
      << ";maxGossipMessagesPerPeerWindow=" << m_maxGossipMessagesPerPeerWindow
      << ";maxTransactionGossipPerPeerWindow="
      << m_maxTransactionGossipPerPeerWindow
      << ";maxTransactionRelayPerSecond=" << m_maxTransactionRelayPerSecond
      << ";doubleVoteSlashFractionBasisPoints="
      << m_doubleVoteSlashFractionBasisPoints
      << ";proposerEquivocationSlashFractionBasisPoints="
      << m_proposerEquivocationSlashFractionBasisPoints
      << ";epochSlashCapBasisPoints=" << m_epochSlashCapBasisPoints
      << ";treasuryTimelockBlocks=" << m_treasuryTimelockBlocks
      << ";treasuryMaxSpendPerProposalRawUnits="
      << m_treasuryMaxSpendPerProposalRawUnits
      << ";treasuryMaxSpendPerEpochRawUnits="
      << m_treasuryMaxSpendPerEpochRawUnits << "}";

  return oss.str();
}

NetworkParameters NetworkParameters::developmentLocal() {
  return NetworkParameters("nodo-localnet-1", "localnet", "nodo/0.1", 60, 1, 2,
                           3, 1000, 128, 10000, 0, 60, 1,
                           "NODO_CRYPTO_SUITE_V1", "NODO_STORAGE_V2", 3000,
                           3000, 3000, 100, 50, 20, 500, 1000, 5000);
}

NetworkParameters NetworkParameters::developmentSoak() {
  return NetworkParameters("nodo-localnet-soak-1", "localnet-soak", "nodo/0.1",
                           60, 3, 2, 3, 1000, 128, 10000, 0, 60, 1,
                           "NODO_CRYPTO_SUITE_V1", "NODO_STORAGE_V2", 3000,
                           3000, 3000, 100, 50, 20, 500, 1000, 5000);
}

NetworkParameters NetworkParameters::testnetCandidate() {
  return NetworkParameters("nodo-testnet-1", "testnet-candidate", "nodo/0.1",
                           300, 4, 2, 3, 500, 64, 5000, 1000, 30, 3,
                           "NODO_CRYPTO_SUITE_V1", "NODO_STORAGE_V2", 3000,
                           3000, 3000, 100, 50, 20, 500, 1000, 5000);
}

BootstrapValidatorConfig::BootstrapValidatorConfig()
    : m_validatorPublicKey(), m_activationEpoch(0), m_bootstrapWeight(0),
      m_metadataHash(""), m_ownerAddress("") {}

BootstrapValidatorConfig::BootstrapValidatorConfig(
    crypto::PublicKey validatorPublicKey, std::uint64_t activationEpoch,
    std::uint32_t bootstrapWeight, std::string metadataHash,
    std::string ownerAddress)
    : m_validatorPublicKey(std::move(validatorPublicKey)),
      m_activationEpoch(activationEpoch), m_bootstrapWeight(bootstrapWeight),
      m_metadataHash(std::move(metadataHash)),
      m_ownerAddress(std::move(ownerAddress)) {}

const crypto::PublicKey &BootstrapValidatorConfig::validatorPublicKey() const {
  return m_validatorPublicKey;
}

std::uint64_t BootstrapValidatorConfig::activationEpoch() const {
  return m_activationEpoch;
}

std::uint32_t BootstrapValidatorConfig::bootstrapWeight() const {
  return m_bootstrapWeight;
}

const std::string &BootstrapValidatorConfig::metadataHash() const {
  return m_metadataHash;
}

const std::string &BootstrapValidatorConfig::ownerAddress() const {
  return m_ownerAddress;
}

std::string BootstrapValidatorConfig::validatorAddress() const {
  if (!m_validatorPublicKey.isValid()) {
    return "";
  }

  return crypto::AddressDerivation::deriveFromPublicKey(m_validatorPublicKey)
      .value();
}

std::string BootstrapValidatorConfig::effectiveOwnerAddress() const {
  return m_ownerAddress.empty() ? validatorAddress() : m_ownerAddress;
}

bool BootstrapValidatorConfig::isValid() const {
  return m_validatorPublicKey.isValid() && m_activationEpoch > 0 &&
         m_bootstrapWeight > 0 && isSafeScalar(m_metadataHash) &&
         (m_ownerAddress.empty() || isSafeScalar(m_ownerAddress)) &&
         !validatorAddress().empty();
}

std::string BootstrapValidatorConfig::serialize() const {
  std::ostringstream oss;

  oss << "BootstrapValidatorConfig{"
      << "validatorAddress=" << validatorAddress()
      << ";publicKey=" << m_validatorPublicKey.serialize()
      << ";publicKeyFingerprint=" << m_validatorPublicKey.fingerprint()
      << ";activationEpoch=" << m_activationEpoch
      << ";bootstrapWeight=" << m_bootstrapWeight
      << ";metadataHash=" << m_metadataHash
      << ";ownerAddress=" << m_ownerAddress << "}";

  return oss.str();
}

GenesisAccountConfig::GenesisAccountConfig()
    : m_address(""), m_balance(), m_nonce(0) {}

GenesisAccountConfig::GenesisAccountConfig(std::string address,
                                           utils::Amount balance,
                                           std::uint64_t nonce)
    : m_address(std::move(address)), m_balance(balance), m_nonce(nonce) {}

const std::string &GenesisAccountConfig::address() const { return m_address; }

utils::Amount GenesisAccountConfig::balance() const { return m_balance; }

std::uint64_t GenesisAccountConfig::nonce() const { return m_nonce; }

bool GenesisAccountConfig::isValid() const {
  return isSafeScalar(m_address) && !m_balance.isNegative();
}

std::string GenesisAccountConfig::serialize() const {
  std::ostringstream oss;

  oss << "GenesisAccountConfig{"
      << "address=" << m_address << ";balanceRaw=" << m_balance.rawUnits()
      << ";nonce=" << m_nonce << "}";

  return oss.str();
}

GenesisConfig::GenesisConfig()
    : m_networkParameters(), m_genesisTimestamp(0), m_bootstrapValidators(),
      m_genesisAccounts(), m_genesisMemo("") {}

GenesisConfig::GenesisConfig(
    NetworkParameters networkParameters, std::int64_t genesisTimestamp,
    std::vector<BootstrapValidatorConfig> bootstrapValidators,
    std::string genesisMemo)
    : GenesisConfig(std::move(networkParameters), genesisTimestamp,
                    std::move(bootstrapValidators), {},
                    std::move(genesisMemo)) {}

GenesisConfig::GenesisConfig(
    NetworkParameters networkParameters, std::int64_t genesisTimestamp,
    std::vector<BootstrapValidatorConfig> bootstrapValidators,
    std::vector<GenesisAccountConfig> genesisAccounts, std::string genesisMemo)
    : m_networkParameters(std::move(networkParameters)),
      m_genesisTimestamp(genesisTimestamp),
      m_bootstrapValidators(std::move(bootstrapValidators)),
      m_genesisAccounts(std::move(genesisAccounts)),
      m_genesisMemo(std::move(genesisMemo)) {}

const NetworkParameters &GenesisConfig::networkParameters() const {
  return m_networkParameters;
}

std::int64_t GenesisConfig::genesisTimestamp() const {
  return m_genesisTimestamp;
}

const std::vector<BootstrapValidatorConfig> &
GenesisConfig::bootstrapValidators() const {
  return m_bootstrapValidators;
}

const std::vector<GenesisAccountConfig> &
GenesisConfig::genesisAccounts() const {
  return m_genesisAccounts;
}

const std::string &GenesisConfig::genesisMemo() const { return m_genesisMemo; }

bool GenesisConfig::isValid() const {
  if (!m_networkParameters.isValid() || m_genesisTimestamp <= 0 ||
      !isSafeScalar(m_genesisMemo)) {
    return false;
  }

  if (m_bootstrapValidators.size() <
      m_networkParameters.minimumValidatorCount()) {
    return false;
  }

  std::set<std::string> seenAddresses;

  for (const auto &validator : m_bootstrapValidators) {
    if (!validator.isValid()) {
      return false;
    }

    if (!seenAddresses.insert(validator.validatorAddress()).second) {
      return false;
    }
  }

  std::set<std::string> seenGenesisAccounts;

  for (const auto &account : m_genesisAccounts) {
    if (!account.isValid()) {
      return false;
    }

    if (!seenGenesisAccounts.insert(account.address()).second) {
      return false;
    }
  }

  return true;
}

std::string GenesisConfig::deterministicId() const {
  if (!isValid()) {
    return "";
  }

  return hashString(serialize());
}

std::string GenesisConfig::serialize() const {
  std::ostringstream oss;

  oss << "GenesisConfig{"
      << "networkParameters=" << m_networkParameters.serialize()
      << ";genesisTimestamp=" << m_genesisTimestamp
      << ";genesisMemo=" << m_genesisMemo << ";bootstrapValidators=[";

  for (std::size_t index = 0; index < m_bootstrapValidators.size(); ++index) {
    if (index != 0) {
      oss << ",";
    }

    oss << m_bootstrapValidators[index].serialize();
  }

  oss << "];genesisAccounts=[";

  for (std::size_t index = 0; index < m_genesisAccounts.size(); ++index) {
    if (index != 0) {
      oss << ",";
    }

    oss << m_genesisAccounts[index].serialize();
  }

  oss << "]}";

  return oss.str();
}

std::string genesisBuildStatusToString(GenesisBuildStatus status) {
  switch (status) {
  case GenesisBuildStatus::BUILT:
    return "BUILT";
  case GenesisBuildStatus::INVALID_CONFIG:
    return "INVALID_CONFIG";
  case GenesisBuildStatus::INVALID_VALIDATOR_REGISTRY:
    return "INVALID_VALIDATOR_REGISTRY";
  case GenesisBuildStatus::INVALID_BLOCKCHAIN:
    return "INVALID_BLOCKCHAIN";
  default:
    return "INVALID_CONFIG";
  }
}

GenesisBuildResult::GenesisBuildResult()
    : m_status(GenesisBuildStatus::INVALID_CONFIG),
      m_reason("Uninitialized genesis build result."), m_blockchain(),
      m_validatorRegistry() {}

GenesisBuildResult
GenesisBuildResult::built(core::Blockchain blockchain,
                          core::ValidatorRegistry validatorRegistry) {
  GenesisBuildResult result;
  result.m_status = GenesisBuildStatus::BUILT;
  result.m_reason = "";
  result.m_blockchain = std::move(blockchain);
  result.m_validatorRegistry = std::move(validatorRegistry);
  return result;
}

GenesisBuildResult GenesisBuildResult::rejected(GenesisBuildStatus status,
                                                std::string reason) {
  GenesisBuildResult result;
  result.m_status = status;
  result.m_reason = std::move(reason);
  return result;
}

GenesisBuildStatus GenesisBuildResult::status() const { return m_status; }

const std::string &GenesisBuildResult::reason() const { return m_reason; }

bool GenesisBuildResult::built() const {
  return m_status == GenesisBuildStatus::BUILT && !m_blockchain.empty() &&
         m_blockchain.isValid() && m_validatorRegistry.isValid();
}

const core::Blockchain &GenesisBuildResult::blockchain() const {
  return m_blockchain;
}

const core::ValidatorRegistry &GenesisBuildResult::validatorRegistry() const {
  return m_validatorRegistry;
}

std::string GenesisBuildResult::serialize() const {
  std::ostringstream oss;

  oss << "GenesisBuildResult{"
      << "status=" << genesisBuildStatusToString(m_status)
      << ";reason=" << m_reason << ";blockchainSize=" << m_blockchain.size()
      << ";validatorRegistrySize=" << m_validatorRegistry.size() << "}";

  return oss.str();
}

GenesisBuildResult GenesisBuilder::build(const GenesisConfig &genesisConfig) {
  if (!genesisConfig.isValid()) {
    return GenesisBuildResult::rejected(GenesisBuildStatus::INVALID_CONFIG,
                                        "Genesis config is invalid.");
  }

  core::ValidatorRegistry validatorRegistry;
  std::vector<core::LedgerRecord> genesisRecords;

  for (const auto &validator : genesisConfig.bootstrapValidators()) {
    const core::ValidatorRegistrationRecord registration(
        validator.validatorAddress(), validator.validatorPublicKey(),
        validator.activationEpoch(), validator.metadataHash(),
        genesisConfig.genesisTimestamp());

    const std::uint64_t bootstrapStake =
        std::max(static_cast<std::uint64_t>(validator.bootstrapWeight()),
                 core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS);

    const auto result = validatorRegistry.registerValidator(
        registration, bootstrapStake, validator.effectiveOwnerAddress());

    if (!result.accepted()) {
      return GenesisBuildResult::rejected(
          GenesisBuildStatus::INVALID_VALIDATOR_REGISTRY,
          "Bootstrap validator registration failed: " + result.reason());
    }

    genesisRecords.push_back(
        genesisValidatorLedgerRecord(genesisConfig, validator));
  }

  if (!validatorRegistry.isValid()) {
    return GenesisBuildResult::rejected(
        GenesisBuildStatus::INVALID_VALIDATOR_REGISTRY,
        "Validator registry failed genesis audit.");
  }

  const core::Block genesisBlock = core::Block::createGenesisBlock(
      genesisRecords, genesisConfig.genesisTimestamp());

  core::Blockchain blockchain;
  blockchain.addGenesisBlock(genesisBlock);

  if (blockchain.empty() || !blockchain.isValid()) {
    return GenesisBuildResult::rejected(GenesisBuildStatus::INVALID_BLOCKCHAIN,
                                        "Genesis blockchain failed audit.");
  }

  return GenesisBuildResult::built(blockchain, validatorRegistry);
}

} // namespace nodo::config
