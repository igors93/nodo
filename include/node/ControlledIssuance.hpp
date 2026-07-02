#ifndef NODO_NODE_CONTROLLED_ISSUANCE_HPP
#define NODO_NODE_CONTROLLED_ISSUANCE_HPP

#include "config/NetworkParameters.hpp"
#include "node/MonetaryFirewall.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

constexpr std::uint64_t NODO_CONTROLLED_ISSUANCE_EPOCH_BLOCKS = 525600;
constexpr std::uint64_t NODO_CONTROLLED_ISSUANCE_TIMELOCK_BLOCKS = 10080;

class InflationEpochSnapshot {
public:
    InflationEpochSnapshot();

    InflationEpochSnapshot(
        std::string status,
        std::uint64_t blockHeight,
        std::uint64_t epochStartBlock,
        std::uint64_t epochEndBlock,
        std::uint32_t maxAnnualInflationBasisPoints,
        utils::Amount baseSupply,
        utils::Amount annualMintLimit,
        utils::Amount mintedThisEpoch,
        utils::Amount remainingMintCapacity,
        std::string policyId,
        std::string reason
    );

    static InflationEpochSnapshot notEvaluated();

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    std::uint64_t epochStartBlock() const;
    std::uint64_t epochEndBlock() const;
    std::uint32_t maxAnnualInflationBasisPoints() const;
    utils::Amount baseSupply() const;
    utils::Amount annualMintLimit() const;
    utils::Amount mintedThisEpoch() const;
    utils::Amount remainingMintCapacity() const;
    const std::string& policyId() const;
    const std::string& reason() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    std::uint64_t m_epochStartBlock;
    std::uint64_t m_epochEndBlock;
    std::uint32_t m_maxAnnualInflationBasisPoints;
    utils::Amount m_baseSupply;
    utils::Amount m_annualMintLimit;
    utils::Amount m_mintedThisEpoch;
    utils::Amount m_remainingMintCapacity;
    std::string m_policyId;
    std::string m_reason;
};

class MintAuthorizationRecord {
public:
    MintAuthorizationRecord();

    MintAuthorizationRecord(
        std::string status,
        std::uint64_t blockHeight,
        std::string authorizationId,
        utils::Amount authorizedAmount,
        std::uint64_t activationBlock,
        std::uint64_t expirationBlock,
        std::uint32_t requiredApprovalBasisPoints,
        std::uint64_t timelockBlocks,
        std::string governanceDigest,
        std::string reason,
        std::string sourceEpochDigest
    );

    static MintAuthorizationRecord none(
        const InflationEpochSnapshot& epoch
    );

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    const std::string& authorizationId() const;
    utils::Amount authorizedAmount() const;
    std::uint64_t activationBlock() const;
    std::uint64_t expirationBlock() const;
    std::uint32_t requiredApprovalBasisPoints() const;
    std::uint64_t timelockBlocks() const;
    const std::string& governanceDigest() const;
    const std::string& reason() const;
    const std::string& sourceEpochDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    std::string m_authorizationId;
    utils::Amount m_authorizedAmount;
    std::uint64_t m_activationBlock;
    std::uint64_t m_expirationBlock;
    std::uint32_t m_requiredApprovalBasisPoints;
    std::uint64_t m_timelockBlocks;
    std::string m_governanceDigest;
    std::string m_reason;
    std::string m_sourceEpochDigest;
};

class SupplyExpansionRecord {
public:
    SupplyExpansionRecord();

    SupplyExpansionRecord(
        std::string status,
        std::uint64_t blockHeight,
        utils::Amount mintedAmount,
        std::string recipientAddress,
        std::string authorizationId,
        std::string policyId,
        std::string reason,
        std::string sourceAuthorizationDigest
    );

    static SupplyExpansionRecord none(
        const MintAuthorizationRecord& authorization,
        const InflationEpochSnapshot& epoch
    );

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    utils::Amount mintedAmount() const;
    const std::string& recipientAddress() const;
    const std::string& authorizationId() const;
    const std::string& policyId() const;
    const std::string& reason() const;
    const std::string& sourceAuthorizationDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    utils::Amount m_mintedAmount;
    std::string m_recipientAddress;
    std::string m_authorizationId;
    std::string m_policyId;
    std::string m_reason;
    std::string m_sourceAuthorizationDigest;
};

class ControlledIssuance {
public:
    static constexpr const char* INFLATION_EPOCH_REASON =
        "CONTROLLED_ISSUANCE_EPOCH";

    static constexpr const char* NO_AUTHORIZATION_REASON =
        "NO_ACTIVE_MINT_AUTHORIZATION";

    static constexpr const char* NO_SUPPLY_EXPANSION_REASON =
        "NO_SUPPLY_EXPANSION_EXECUTED";

    static constexpr const char* EPOCH_REWARD_AUTHORIZATION_REASON =
        "CANONICAL_EPOCH_REWARD_AUTHORIZATION";

    static constexpr const char* EPOCH_REWARD_EXPANSION_REASON =
        "CANONICAL_EPOCH_REWARD_EXPANSION";

    static constexpr const char* GOVERNANCE_LOCKED_DIGEST =
        "GOVERNANCE_LOCKED_FOR_FUTURE_PHASE";

    static constexpr const char* NO_RECIPIENT_ADDRESS =
        "NO_MINT_RECIPIENT";

    static constexpr std::uint32_t REQUIRED_APPROVAL_BASIS_POINTS =
        8000;

    static InflationEpochSnapshot buildInflationEpochSnapshot(
        const config::GenesisConfig& genesisConfig,
        std::uint64_t blockHeight,
        utils::Amount mintedThisEpoch
    );

    static MintAuthorizationRecord buildNoMintAuthorization(
        const InflationEpochSnapshot& epoch
    );

    static SupplyExpansionRecord buildNoSupplyExpansion(
        const MintAuthorizationRecord& authorization,
        const InflationEpochSnapshot& epoch
    );

    static MintAuthorizationRecord buildEpochRewardAuthorization(
        const InflationEpochSnapshot& epoch,
        utils::Amount authorizedAmount,
        const std::string& authorizationId,
        const std::string& rewardEvidenceDigest
    );

    static SupplyExpansionRecord buildEpochRewardExpansion(
        const MintAuthorizationRecord& authorization,
        const InflationEpochSnapshot& epoch
    );

    static bool sameEpoch(
        const InflationEpochSnapshot& left,
        const InflationEpochSnapshot& right
    );

    static bool sameAuthorization(
        const MintAuthorizationRecord& left,
        const MintAuthorizationRecord& right
    );

    static bool sameExpansion(
        const SupplyExpansionRecord& left,
        const SupplyExpansionRecord& right
    );
};

} // namespace nodo::node

#endif
