#ifndef NODO_ECONOMICS_MONETARY_POLICY_HPP
#define NODO_ECONOMICS_MONETARY_POLICY_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * MonetaryPolicy is the canonical monetary specification for one Nodo network.
 *
 * It defines:
 *   - what the unit of account is (NODO);
 *   - what the smallest unit is (raw units, analogous to satoshis);
 *   - how many base units make one NODO;
 *   - the initial supply at genesis;
 *   - the hard cap on annual inflation in basis points.
 *
 * Security principle:
 * The monetary policy is fixed at network launch and must be embedded in the
 * genesis config. Any node that computes a supply state must use the same
 * policy or the state root diverges. Nodes refuse to join a chain whose policy
 * is incompatible with their local configuration.
 *
 * Inflation cap:
 * 400 basis points (4%) is the maximum allowed annual inflation rate.
 * This matches the constant NODO_MAX_ANNUAL_INFLATION_BASIS_POINTS in
 * node::MonetaryFirewall. Both must stay in sync.
 *
 * Note on MintRecord.authorizationId:
 * The economics::MintRecord type intentionally tracks the sourceBlockHash as
 * the authorization anchor. A future task will migrate MintRecord to carry an
 * explicit authorizationId once node::ControlledIssuance is wired into the
 * production block pipeline.
 */
class MonetaryPolicy {
public:
    static constexpr std::uint32_t MAX_ANNUAL_INFLATION_BASIS_POINTS = 400;

    MonetaryPolicy();

    MonetaryPolicy(
        std::string policyVersion,
        std::string chainId,
        std::string unitName,
        std::string baseUnitName,
        std::uint64_t baseUnitsPerUnit,
        utils::Amount initialSupply,
        std::uint32_t maxAnnualInflationBasisPoints
    );

    static MonetaryPolicy localnetDefault(
        const std::string& chainId,
        utils::Amount initialSupply
    );

    const std::string& policyVersion() const;
    const std::string& chainId() const;
    const std::string& unitName() const;
    const std::string& baseUnitName() const;
    std::uint64_t baseUnitsPerUnit() const;
    utils::Amount initialSupply() const;
    std::uint32_t maxAnnualInflationBasisPoints() const;

    bool isValid() const;
    std::string rejectionReason() const;
    std::string serialize() const;

private:
    std::string m_policyVersion;
    std::string m_chainId;
    std::string m_unitName;
    std::string m_baseUnitName;
    std::uint64_t m_baseUnitsPerUnit;
    utils::Amount m_initialSupply;
    std::uint32_t m_maxAnnualInflationBasisPoints;
};

} // namespace nodo::economics

#endif
