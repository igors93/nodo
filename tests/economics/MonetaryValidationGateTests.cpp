#include "economics/MonetaryValidationGate.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

const std::string kPolicyVersion = "NODO_MONETARY_POLICY_V1";
const std::string kChainId = "nodo-testnet-gate-1";

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
        "gate test authorization",
        "GENESIS_COMMITTEE"
    );
}

nodo::economics::MintRecord makeMint(
    const std::string& mintId,
    const std::string& authId,
    std::int64_t rawUnits,
    std::uint64_t epoch,
    std::uint64_t blockIndex,
    const std::string& blockHash
) {
    return nodo::economics::MintRecord(
        mintId, authId, "nodo1gate001",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        epoch, blockIndex, blockHash, 1900000001
    );
}

void testGateAcceptsValidAuthorizedMintDelta() {
    const nodo::economics::SupplyDelta delta(
        5, "gate-hash-A", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(300),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1300),
        {makeMint("gate-mint-001", "gate-auth-001", 300, 1, 5, "gate-hash-A")},
        {}
    );
    const auto result = nodo::economics::MonetaryValidationGate::validate(
        testPolicy(), delta, {makeAuth("gate-auth-001", 500)}
    );
    assert(result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryValidationGateStatus::ACCEPTED);
    assert(result.reason().empty());
}

void testGateRejectsUnauthorizedMintDelta() {
    const nodo::economics::SupplyDelta delta(
        5, "gate-hash-B", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("gate-mint-002", "no-such-auth", 100, 1, 5, "gate-hash-B")},
        {}
    );
    const auto result = nodo::economics::MonetaryValidationGate::validate(
        testPolicy(), delta, {}
    );
    assert(!result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryValidationGateStatus::REJECTED_BY_FIREWALL);
    assert(!result.reason().empty());
    assert(result.reason().find("no-such-auth") != std::string::npos);
}

void testGateAcceptsBurnOnlyDelta() {
    const nodo::economics::SupplyDelta delta(
        8, "gate-hash-C", 2,
        nodo::utils::Amount::fromRawUnits(5000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(200),
        nodo::utils::Amount::fromRawUnits(4800),
        {},
        {nodo::economics::BurnRecord(
            "gate-burn-001", 8, 2, "nodo1burner001",
            nodo::utils::Amount::fromRawUnits(200),
            "fee", nodo::economics::BurnType::FEE_BURN
        )}
    );
    const auto result = nodo::economics::MonetaryValidationGate::validate(
        testPolicy(), delta, {}
    );
    assert(result.isAccepted());
}

void testGatePreservesFirewallRejectionReason() {
    // Mint with wrong policy version — firewall returns MINT_POLICY_VERSION_MISMATCH.
    const nodo::economics::MintAuthorization wrongVersionAuth(
        "wrong-version-auth", "BAD_POLICY_V0", 1, 10,
        nodo::utils::Amount::fromRawUnits(1000), "reason", "approver"
    );
    const nodo::economics::SupplyDelta delta(
        5, "gate-hash-D", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(50),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1050),
        {makeMint("gate-mint-003", "wrong-version-auth", 50, 1, 5, "gate-hash-D")},
        {}
    );
    const auto result = nodo::economics::MonetaryValidationGate::validate(
        testPolicy(), delta, {wrongVersionAuth}
    );
    assert(!result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryValidationGateStatus::REJECTED_BY_FIREWALL);
    // Reason must come from the firewall and mention the policy version mismatch.
    assert(result.reason().find("policyVersion") != std::string::npos ||
           result.reason().find("policy") != std::string::npos);
}

void testGateSerializeContainsKeyFields() {
    const nodo::economics::SupplyDelta delta =
        nodo::economics::SupplyDelta::noOp(
            1, "gate-hash-noop", 1, nodo::utils::Amount::fromRawUnits(100)
        );
    const auto result = nodo::economics::MonetaryValidationGate::validate(
        testPolicy(), delta, {}
    );
    const std::string s = result.serialize();
    assert(!s.empty());
    assert(s.find("ACCEPTED") != std::string::npos);
}

void testGateStatusToString() {
    assert(nodo::economics::monetaryValidationGateStatusToString(
               nodo::economics::MonetaryValidationGateStatus::ACCEPTED) == "ACCEPTED");
    assert(nodo::economics::monetaryValidationGateStatusToString(
               nodo::economics::MonetaryValidationGateStatus::REJECTED_BY_FIREWALL) ==
           "REJECTED_BY_FIREWALL");
}

} // namespace

int main() {
    testGateAcceptsValidAuthorizedMintDelta();
    testGateRejectsUnauthorizedMintDelta();
    testGateAcceptsBurnOnlyDelta();
    testGatePreservesFirewallRejectionReason();
    testGateSerializeContainsKeyFields();
    testGateStatusToString();
    return 0;
}
