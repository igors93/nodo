#include "node/MonetaryArtifactValidator.hpp"
#include "economics/MintAuthorization.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"
#include "node/ControlledIssuance.hpp"
#include "node/FeeEconomics.hpp"
#include "node/MonetaryFirewall.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

const std::string kChainId = "nodo-artifact-test-1";
const std::string kPolicyVersion = "NODO_MONETARY_POLICY_V1";

nodo::economics::MonetaryPolicy testPolicy() {
    return nodo::economics::MonetaryPolicy::localnetDefault(
        kChainId, nodo::utils::Amount::fromRawUnits(1000000)
    );
}

nodo::economics::MintAuthorization makeAuth(
    const std::string& authId,
    std::int64_t maxRawUnits
) {
    return nodo::economics::MintAuthorization(
        authId, kPolicyVersion, 1, 10,
        nodo::utils::Amount::fromRawUnits(maxRawUnits),
        "artifact test authorization",
        "GENESIS_COMMITTEE"
    );
}

nodo::economics::MintRecord makeMint(
    const std::string& mintId,
    const std::string& authId,
    std::int64_t rawUnits,
    std::uint64_t blockHeight,
    const std::string& blockHash
) {
    return nodo::economics::MintRecord(
        mintId, authId, "nodo1artifact001",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, blockHeight, blockHash, 1900000001
    );
}

nodo::economics::SupplyDelta makeBurnDelta() {
    const nodo::economics::BurnRecord burn(
        "artifact-fee-burn-001",
        10,
        0,
        "nodo_fee_pool",
        nodo::utils::Amount::fromRawUnits(20),
        "fee burn",
        nodo::economics::BurnType::FEE_BURN
    );

    return nodo::economics::SupplyDelta(
        10,
        "artifact-hash-burn",
        0,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(20),
        nodo::utils::Amount::fromRawUnits(980),
        {},
        {burn}
    );
}

nodo::node::FeeBurnRecord makeFeeBurnRecord(
    std::int64_t burnRawUnits
) {
    return nodo::node::FeeBurnRecord(
        "ACTIVE",
        10,
        nodo::utils::Amount::fromRawUnits(burnRawUnits),
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(1000 - burnRawUnits),
        nodo::node::FeeEconomics::FEE_BURN_REASON,
        "fee-balance-digest"
    );
}

nodo::node::SupplyExpansionRecord makeNoSupplyExpansionRecord() {
    return nodo::node::SupplyExpansionRecord(
        "NONE",
        10,
        nodo::utils::Amount::fromRawUnits(0),
        nodo::node::ControlledIssuance::NO_RECIPIENT_ADDRESS,
        "NO_ACTIVE_MINT_AUTHORIZATION",
        "policy-id",
        nodo::node::ControlledIssuance::NO_SUPPLY_EXPANSION_REASON,
        "source-authorization-digest"
    );
}

nodo::node::MonetaryFirewallAudit makeFirewallAudit(
    std::int64_t supplyBefore,
    std::int64_t minted,
    std::int64_t burned,
    std::int64_t treasuryDelta,
    std::int64_t supplyAfter
) {
    const nodo::node::MonetaryPolicy policy =
        nodo::node::MonetaryPolicy::protocolDefault();

    return nodo::node::MonetaryFirewallAudit(
        "PASS",
        nodo::node::SupplyLedgerSnapshot(
            10,
            nodo::utils::Amount::fromRawUnits(supplyBefore),
            nodo::utils::Amount::fromRawUnits(minted),
            nodo::utils::Amount::fromRawUnits(burned),
            nodo::utils::Amount::fromRawUnits(treasuryDelta),
            nodo::utils::Amount::fromRawUnits(supplyAfter)
        ),
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(minted),
        policy.deterministicId(),
        nodo::node::MonetaryFirewall::ZERO_MINT_REASON
    );
}

void testValidateSupplyDeltaAcceptsNoOpDelta() {
    const auto delta = nodo::economics::SupplyDelta::noOp(
        10, "artifact-hash-A", 1, nodo::utils::Amount::fromRawUnits(1000)
    );
    const auto result = nodo::node::MonetaryArtifactValidator::validateSupplyDelta(
        testPolicy(), delta, {}
    );
    assert(result.accepted());
}

void testValidateSupplyDeltaRejectsInvalidDelta() {
    // Empty blockHash makes SupplyDelta invalid.
    const nodo::economics::SupplyDelta badDelta(
        1, "", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1000),
        {}, {}
    );
    const auto result = nodo::node::MonetaryArtifactValidator::validateSupplyDelta(
        testPolicy(), badDelta, {}
    );
    assert(!result.accepted());
    assert(!result.reason().empty());
}

void testValidateSupplyDeltaRejectsUnauthorizedMint() {
    const nodo::economics::SupplyDelta delta(
        5, "artifact-hash-B", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(200),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1200),
        {makeMint("art-mint-001", "no-such-auth", 200, 5, "artifact-hash-B")},
        {}
    );
    const auto result = nodo::node::MonetaryArtifactValidator::validateSupplyDelta(
        testPolicy(), delta, {}
    );
    assert(!result.accepted());
    assert(result.reason().find("gate rejected") != std::string::npos ||
           result.reason().find("no-such-auth") != std::string::npos);
}

void testValidateSupplyDeltaAcceptsAuthorizedMint() {
    const nodo::economics::SupplyDelta delta(
        5, "artifact-hash-C", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("art-mint-002", "art-auth-001", 100, 5, "artifact-hash-C")},
        {}
    );
    const auto result = nodo::node::MonetaryArtifactValidator::validateSupplyDelta(
        testPolicy(), delta, {makeAuth("art-auth-001", 500)}
    );
    assert(result.accepted());
}

void testValidateSupplyDeltaRejectsInvalidPolicy() {
    const nodo::economics::MonetaryPolicy invalidPolicy;  // default, empty
    const auto delta = nodo::economics::SupplyDelta::noOp(
        1, "artifact-hash-D", 1, nodo::utils::Amount::fromRawUnits(100)
    );
    const auto result = nodo::node::MonetaryArtifactValidator::validateSupplyDelta(
        invalidPolicy, delta, {}
    );
    assert(!result.accepted());
    assert(result.reason().find("invalid policy") != std::string::npos ||
           result.reason().find("policy") != std::string::npos);
}

void testValidateSupplyDeltaRejectsFeeBurnMismatch() {
    const auto delta = makeBurnDelta();
    const auto result =
        nodo::node::MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithFeeBurn(
            delta,
            makeFeeBurnRecord(19)
        );

    assert(!result.accepted());
    assert(result.reason().find("fee burn") != std::string::npos ||
           result.reason().find("below") != std::string::npos);
}

void testValidateSupplyDeltaAcceptsFeeBurnMatch() {
    const auto delta = makeBurnDelta();
    const auto result =
        nodo::node::MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithFeeBurn(
            delta,
            makeFeeBurnRecord(20)
        );

    assert(result.accepted());
}

void testValidateSupplyDeltaRejectsSupplyExpansionMismatch() {
    const nodo::economics::SupplyDelta delta(
        10,
        "artifact-hash-mint",
        1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(5),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1005),
        {makeMint("art-mint-expansion", "art-auth-expansion", 5, 10, "artifact-hash-mint")},
        {}
    );

    const auto result =
        nodo::node::MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithSupplyExpansion(
            delta,
            makeNoSupplyExpansionRecord()
        );

    assert(!result.accepted());
    assert(result.reason().find("SupplyExpansionRecord") != std::string::npos);
}

void testValidateSupplyDeltaRejectsMonetaryFirewallMismatch() {
    const auto delta = makeBurnDelta();
    const nodo::node::MonetaryFirewallAudit mismatchedAudit =
        nodo::node::MonetaryFirewall::buildAuditWithSupplyBefore(
            10,
            nodo::utils::Amount::fromRawUnits(900),
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(20),
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(0)
        );

    const auto result =
        nodo::node::MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithMonetaryFirewallAudit(
            delta,
            mismatchedAudit
        );

    assert(!result.accepted());
    assert(result.reason().find("MonetaryFirewallAudit") != std::string::npos);
}

void testValidateSupplyDeltaRejectsMonetaryFirewallMintedMismatch() {
    const auto delta = makeBurnDelta();
    const auto result =
        nodo::node::MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithMonetaryFirewallAudit(
            delta,
            makeFirewallAudit(1000, 1, 20, 0, 981)
        );

    assert(!result.accepted());
    assert(result.reason().find("MonetaryFirewallAudit") != std::string::npos);
}

void testValidateSupplyDeltaRejectsMonetaryFirewallBurnedMismatch() {
    const auto delta = makeBurnDelta();
    const auto result =
        nodo::node::MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithMonetaryFirewallAudit(
            delta,
            makeFirewallAudit(1000, 0, 19, 0, 981)
        );

    assert(!result.accepted());
    assert(result.reason().find("MonetaryFirewallAudit") != std::string::npos);
}

void testValidateSupplyDeltaRejectsMonetaryFirewallSupplyAfterMismatch() {
    const auto delta = makeBurnDelta();
    const auto result =
        nodo::node::MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithMonetaryFirewallAudit(
            delta,
            makeFirewallAudit(1000, 0, 20, 0, 979)
        );

    assert(!result.accepted());
    assert(result.reason().find("MonetaryFirewallAudit") != std::string::npos);
}

void testValidateSupplyDeltaAcceptsMonetaryFirewallMatch() {
    const auto delta = makeBurnDelta();
    const nodo::node::MonetaryFirewallAudit matchingAudit =
        nodo::node::MonetaryFirewall::buildAuditWithSupplyBefore(
            10,
            nodo::utils::Amount::fromRawUnits(1000),
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(20),
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(0)
        );

    const auto result =
        nodo::node::MonetaryArtifactValidator::validateSupplyDeltaConsistencyWithMonetaryFirewallAudit(
            delta,
            matchingAudit
        );

    assert(result.accepted());
}

} // namespace

int main() {
    testValidateSupplyDeltaAcceptsNoOpDelta();
    testValidateSupplyDeltaRejectsInvalidDelta();
    testValidateSupplyDeltaRejectsUnauthorizedMint();
    testValidateSupplyDeltaAcceptsAuthorizedMint();
    testValidateSupplyDeltaRejectsInvalidPolicy();
    testValidateSupplyDeltaRejectsFeeBurnMismatch();
    testValidateSupplyDeltaAcceptsFeeBurnMatch();
    testValidateSupplyDeltaRejectsSupplyExpansionMismatch();
    testValidateSupplyDeltaRejectsMonetaryFirewallMismatch();
    testValidateSupplyDeltaRejectsMonetaryFirewallMintedMismatch();
    testValidateSupplyDeltaRejectsMonetaryFirewallBurnedMismatch();
    testValidateSupplyDeltaRejectsMonetaryFirewallSupplyAfterMismatch();
    testValidateSupplyDeltaAcceptsMonetaryFirewallMatch();
    return 0;
}
