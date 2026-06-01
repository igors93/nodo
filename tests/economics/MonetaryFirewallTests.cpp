#include "economics/MonetaryFirewall.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

const std::string kPolicyVersion = "NODO_MONETARY_POLICY_V1";
const std::string kChainId = "nodo-testnet-1";

nodo::economics::MonetaryPolicy testPolicy() {
    return nodo::economics::MonetaryPolicy::localnetDefault(
        kChainId, nodo::utils::Amount::fromRawUnits(1000000)
    );
}

nodo::economics::MintAuthorization makeAuth(
    const std::string& authId,
    std::uint64_t epoch,
    std::uint64_t expiresAtEpoch,
    std::int64_t maxRawUnits
) {
    return nodo::economics::MintAuthorization(
        authId,
        kPolicyVersion,
        epoch,
        expiresAtEpoch,
        nodo::utils::Amount::fromRawUnits(maxRawUnits),
        "test authorization",
        "GENESIS_COMMITTEE"
    );
}

nodo::economics::MintRecord makeMint(
    const std::string& mintId,
    const std::string& authId,
    std::int64_t rawUnits,
    std::uint64_t epoch = 1
) {
    return nodo::economics::MintRecord(
        mintId,
        authId,
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        epoch, 5, "block-hash-test", 1900000001
    );
}

nodo::economics::BurnRecord makeBurn(
    const std::string& burnId,
    std::int64_t rawUnits,
    std::uint64_t blockHeight = 5
) {
    return nodo::economics::BurnRecord(
        burnId, blockHeight, 1, "nodo1sender001",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        "fee", nodo::economics::BurnType::FEE_BURN
    );
}

nodo::economics::SupplyDelta noOpDelta(
    nodo::utils::Amount supply,
    std::uint64_t epoch = 1
) {
    return nodo::economics::SupplyDelta::noOp(1, "block-hash-A", epoch, supply);
}

// ---- Tests ----

void testValidNoOpDeltaPasses() {
    const nodo::economics::MonetaryFirewallContext ctx(
        testPolicy(),
        noOpDelta(nodo::utils::Amount::fromRawUnits(1000)),
        {}
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryFirewallStatus::ACCEPTED);
    assert(result.reason().empty());
}

void testValidBurnOnlyDeltaPasses() {
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-B", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(50),
        nodo::utils::Amount::fromRawUnits(950),
        {},
        {makeBurn("burn-001", 50)}
    );
    const nodo::economics::MonetaryFirewallContext ctx(testPolicy(), delta, {});
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(result.isAccepted());
}

void testValidAuthorizedMintPasses() {
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-C", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(500),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1500),
        {makeMint("mint-001", "auth-001", 500)},
        {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(
        testPolicy(),
        delta,
        {makeAuth("auth-001", 1, 10, 1000)}
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(result.isAccepted());
}

void testInvalidPolicyRejected() {
    const nodo::economics::MonetaryPolicy invalidPolicy;  // default, empty
    const nodo::economics::MonetaryFirewallContext ctx(
        invalidPolicy, noOpDelta(nodo::utils::Amount::fromRawUnits(1000)), {}
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryFirewallStatus::INVALID_POLICY);
}

void testInvalidSupplyDeltaRejected() {
    // Delta with empty block hash is invalid
    const nodo::economics::SupplyDelta badDelta(
        1, "", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1000),
        {}, {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(testPolicy(), badDelta, {});
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryFirewallStatus::INVALID_SUPPLY_DELTA);
}

void testMintWithUnknownAuthorizationIdRejected() {
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-D", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("mint-001", "unknown-auth", 100)},
        {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(testPolicy(), delta, {});
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryFirewallStatus::UNAUTHORIZED_MINT);
    assert(result.reason().find("unknown-auth") != std::string::npos);
}

void testMintWithExpiredAuthorizationRejected() {
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-E", 5,  // epoch 5
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("mint-001", "auth-expired", 100, 5)},
        {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(
        testPolicy(),
        delta,
        {makeAuth("auth-expired", 1, 3, 1000)}  // expired at epoch 3
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryFirewallStatus::EXPIRED_MINT_AUTHORIZATION);
}

void testMintWithInvalidAuthorizationRejected() {
    // Auth with zero amount is invalid
    const nodo::economics::MintAuthorization badAuth(
        "auth-bad", kPolicyVersion, 1, 10,
        nodo::utils::Amount::fromRawUnits(0),  // invalid: zero amount
        "reason", "approver"
    );
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-F", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("mint-001", "auth-bad", 100)},
        {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(testPolicy(), delta, {badAuth});
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() == nodo::economics::MonetaryFirewallStatus::UNAUTHORIZED_MINT);
}

void testMintPolicyVersionMismatchRejected() {
    const nodo::economics::MintAuthorization wrongVersionAuth(
        "auth-wrong-version", "WRONG_POLICY_V99", 1, 10,
        nodo::utils::Amount::fromRawUnits(1000), "reason", "approver"
    );
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-G", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("mint-001", "auth-wrong-version", 100)},
        {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(
        testPolicy(), delta, {wrongVersionAuth}
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() ==
           nodo::economics::MonetaryFirewallStatus::MINT_POLICY_VERSION_MISMATCH);
}

void testMintAmountExceedingAuthorizationRejected() {
    // Auth allows max 200, but mint is for 300
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-H", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(300),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1300),
        {makeMint("mint-001", "auth-limited", 300)},
        {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(
        testPolicy(),
        delta,
        {makeAuth("auth-limited", 1, 10, 200)}  // max 200
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() ==
           nodo::economics::MonetaryFirewallStatus::MINT_AMOUNT_EXCEEDS_AUTHORIZATION);
}

void testMultipleMintsSameAuthWithinLimitPass() {
    // Two mints of 100 each = 200 total, auth allows 300
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-I", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(200),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1200),
        {
            makeMint("mint-001", "auth-shared", 100),
            makeMint("mint-002", "auth-shared", 100)
        },
        {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(
        testPolicy(),
        delta,
        {makeAuth("auth-shared", 1, 10, 300)}
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(result.isAccepted());
}

void testMultipleMintsSameAuthExceedingLimitFail() {
    // Two mints of 200 each = 400 total, auth allows 300
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-J", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(400),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1400),
        {
            makeMint("mint-001", "auth-shared", 200),
            makeMint("mint-002", "auth-shared", 200)
        },
        {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(
        testPolicy(),
        delta,
        {makeAuth("auth-shared", 1, 10, 300)}
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() ==
           nodo::economics::MonetaryFirewallStatus::MINT_AMOUNT_EXCEEDS_AUTHORIZATION);
}

void testDuplicateAuthorizationIdsRejected() {
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-K", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("mint-001", "auth-dup", 100)},
        {}
    );
    const nodo::economics::MonetaryFirewallContext ctx(
        testPolicy(),
        delta,
        {
            makeAuth("auth-dup", 1, 10, 200),
            makeAuth("auth-dup", 1, 10, 200)  // duplicate
        }
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    assert(!result.isAccepted());
    assert(result.status() ==
           nodo::economics::MonetaryFirewallStatus::DUPLICATE_MINT_AUTHORIZATION);
}

void testStatusToString() {
    assert(nodo::economics::monetaryFirewallStatusToString(
               nodo::economics::MonetaryFirewallStatus::ACCEPTED) == "ACCEPTED");
    assert(nodo::economics::monetaryFirewallStatusToString(
               nodo::economics::MonetaryFirewallStatus::UNAUTHORIZED_MINT) == "UNAUTHORIZED_MINT");
    assert(nodo::economics::monetaryFirewallStatusToString(
               nodo::economics::MonetaryFirewallStatus::EXPIRED_MINT_AUTHORIZATION)
           == "EXPIRED_MINT_AUTHORIZATION");
}

void testResultSerialize() {
    const nodo::economics::MonetaryFirewallContext ctx(
        testPolicy(),
        noOpDelta(nodo::utils::Amount::fromRawUnits(1000)),
        {}
    );
    const auto result = nodo::economics::MonetaryFirewall::validate(ctx);
    const std::string s = result.serialize();
    assert(!s.empty());
    assert(s.find("ACCEPTED") != std::string::npos);
}

} // namespace

int main() {
    testValidNoOpDeltaPasses();
    testValidBurnOnlyDeltaPasses();
    testValidAuthorizedMintPasses();
    testInvalidPolicyRejected();
    testInvalidSupplyDeltaRejected();
    testMintWithUnknownAuthorizationIdRejected();
    testMintWithExpiredAuthorizationRejected();
    testMintWithInvalidAuthorizationRejected();
    testMintPolicyVersionMismatchRejected();
    testMintAmountExceedingAuthorizationRejected();
    testMultipleMintsSameAuthWithinLimitPass();
    testMultipleMintsSameAuthExceedingLimitFail();
    testDuplicateAuthorizationIdsRejected();
    testStatusToString();
    testResultSerialize();
    return 0;
}
