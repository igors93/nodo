#ifndef NODO_CONFIG_NETWORK_PARAMETERS_HPP
#define NODO_CONFIG_NETWORK_PARAMETERS_HPP

#include "core/Blockchain.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::config {

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
        std::string chainId,
        std::string networkName,
        std::string protocolVersion,
        std::uint64_t epochDurationSeconds,
        std::uint64_t minimumValidatorCount,
        std::uint64_t quorumThresholdNumerator,
        std::uint64_t quorumThresholdDenominator,
        std::uint64_t maxTransactionsPerBlock,
        std::uint64_t maxPeerCount
    );

    const std::string& chainId() const;
    const std::string& networkName() const;
    const std::string& protocolVersion() const;
    std::uint64_t epochDurationSeconds() const;
    std::uint64_t minimumValidatorCount() const;
    std::uint64_t quorumThresholdNumerator() const;
    std::uint64_t quorumThresholdDenominator() const;
    std::uint64_t maxTransactionsPerBlock() const;
    std::uint64_t maxPeerCount() const;

    bool isValid() const;

    std::string deterministicId() const;
    std::string serialize() const;

    static NetworkParameters developmentLocal();

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
};

/*
 * BootstrapValidatorConfig describes a validator identity present at genesis.
 *
 * It does not mint coins. It only provides the first validator identities that
 * can protect the chain during local/bootstrap operation.
 */
class BootstrapValidatorConfig {
public:
    BootstrapValidatorConfig();

    BootstrapValidatorConfig(
        crypto::PublicKey validatorPublicKey,
        std::uint64_t activationEpoch,
        std::uint32_t bootstrapWeight,
        std::string metadataHash
    );

    const crypto::PublicKey& validatorPublicKey() const;
    std::uint64_t activationEpoch() const;
    std::uint32_t bootstrapWeight() const;
    const std::string& metadataHash() const;

    std::string validatorAddress() const;

    bool isValid() const;
    std::string serialize() const;

private:
    crypto::PublicKey m_validatorPublicKey;
    std::uint64_t m_activationEpoch;
    std::uint32_t m_bootstrapWeight;
    std::string m_metadataHash;
};

class GenesisConfig {
public:
    GenesisConfig();

    GenesisConfig(
        NetworkParameters networkParameters,
        std::int64_t genesisTimestamp,
        std::vector<BootstrapValidatorConfig> bootstrapValidators,
        std::string genesisMemo
    );

    const NetworkParameters& networkParameters() const;
    std::int64_t genesisTimestamp() const;
    const std::vector<BootstrapValidatorConfig>& bootstrapValidators() const;
    const std::string& genesisMemo() const;

    bool isValid() const;

    std::string deterministicId() const;
    std::string serialize() const;

private:
    NetworkParameters m_networkParameters;
    std::int64_t m_genesisTimestamp;
    std::vector<BootstrapValidatorConfig> m_bootstrapValidators;
    std::string m_genesisMemo;
};

enum class GenesisBuildStatus {
    BUILT,
    INVALID_CONFIG,
    INVALID_VALIDATOR_REGISTRY,
    INVALID_BLOCKCHAIN
};

std::string genesisBuildStatusToString(
    GenesisBuildStatus status
);

class GenesisBuildResult {
public:
    GenesisBuildResult();

    static GenesisBuildResult built(
        core::Blockchain blockchain,
        core::ValidatorRegistry validatorRegistry
    );

    static GenesisBuildResult rejected(
        GenesisBuildStatus status,
        std::string reason
    );

    GenesisBuildStatus status() const;
    const std::string& reason() const;
    bool built() const;

    const core::Blockchain& blockchain() const;
    const core::ValidatorRegistry& validatorRegistry() const;

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
    static GenesisBuildResult build(
        const GenesisConfig& genesisConfig
    );
};

} // namespace nodo::config

#endif
