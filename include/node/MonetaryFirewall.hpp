#ifndef NODO_NODE_MONETARY_FIREWALL_HPP
#define NODO_NODE_MONETARY_FIREWALL_HPP

#include "config/NetworkParameters.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

constexpr std::uint32_t NODO_MAX_ANNUAL_INFLATION_BASIS_POINTS = 400;

class MonetaryPolicy {
public:
    MonetaryPolicy();

    MonetaryPolicy(
        std::uint32_t maxAnnualInflationBasisPoints,
        std::string ruleId,
        std::string reason
    );

    std::uint32_t maxAnnualInflationBasisPoints() const;
    const std::string& ruleId() const;
    const std::string& reason() const;

    bool isValid() const;
    std::string deterministicId() const;
    std::string serialize() const;

    static MonetaryPolicy protocolDefault();

private:
    std::uint32_t m_maxAnnualInflationBasisPoints;
    std::string m_ruleId;
    std::string m_reason;
};

class SupplyLedgerSnapshot {
public:
    SupplyLedgerSnapshot();

    SupplyLedgerSnapshot(
        std::uint64_t blockHeight,
        utils::Amount supplyBefore,
        utils::Amount minted,
        utils::Amount burned,
        utils::Amount treasuryDelta,
        utils::Amount supplyAfter
    );

    std::uint64_t blockHeight() const;
    utils::Amount supplyBefore() const;
    utils::Amount minted() const;
    utils::Amount burned() const;
    utils::Amount treasuryDelta() const;
    utils::Amount supplyAfter() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::uint64_t m_blockHeight;
    utils::Amount m_supplyBefore;
    utils::Amount m_minted;
    utils::Amount m_burned;
    utils::Amount m_treasuryDelta;
    utils::Amount m_supplyAfter;
};

class MonetaryFirewallAudit {
public:
    MonetaryFirewallAudit();

    MonetaryFirewallAudit(
        std::string status,
        SupplyLedgerSnapshot supplyLedger,
        utils::Amount annualMintLimit,
        utils::Amount annualMintUsedBefore,
        utils::Amount annualMintUsedAfter,
        std::string policyId,
        std::string reason
    );

    static MonetaryFirewallAudit notEvaluated();

    const std::string& status() const;
    const SupplyLedgerSnapshot& supplyLedger() const;
    utils::Amount annualMintLimit() const;
    utils::Amount annualMintUsedBefore() const;
    utils::Amount annualMintUsedAfter() const;
    const std::string& policyId() const;
    const std::string& reason() const;

    bool passed() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    SupplyLedgerSnapshot m_supplyLedger;
    utils::Amount m_annualMintLimit;
    utils::Amount m_annualMintUsedBefore;
    utils::Amount m_annualMintUsedAfter;
    std::string m_policyId;
    std::string m_reason;
};

class MonetaryFirewall {
public:
    static constexpr const char* ZERO_MINT_REASON =
        "MONETARY_FIREWALL_ZERO_MINT";

    static constexpr const char* NOT_EVALUATED_REASON =
        "MONETARY_FIREWALL_NOT_EVALUATED";

    static utils::Amount genesisSupply(
        const config::GenesisConfig& genesisConfig
    );

    static utils::Amount annualMintLimit(
        utils::Amount baseSupply,
        const MonetaryPolicy& policy
    );

    static MonetaryFirewallAudit buildZeroMintAudit(
        const config::GenesisConfig& genesisConfig,
        std::uint64_t blockHeight
    );

    static MonetaryFirewallAudit buildAudit(
        const config::GenesisConfig& genesisConfig,
        std::uint64_t blockHeight,
        utils::Amount minted,
        utils::Amount burned,
        utils::Amount treasuryDelta,
        utils::Amount annualMintUsedBefore
    );

    static MonetaryFirewallAudit buildAuditWithSupplyBefore(
        std::uint64_t blockHeight,
        utils::Amount supplyBefore,
        utils::Amount minted,
        utils::Amount burned,
        utils::Amount treasuryDelta,
        utils::Amount annualMintUsedBefore
    );

    static bool sameAudit(
        const MonetaryFirewallAudit& left,
        const MonetaryFirewallAudit& right
    );
};

} // namespace nodo::node

#endif
