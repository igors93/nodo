#include "node/MonetaryArtifactValidator.hpp"
#include "economics/MintAuthorization.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"

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

} // namespace

int main() {
    testValidateSupplyDeltaAcceptsNoOpDelta();
    testValidateSupplyDeltaRejectsInvalidDelta();
    testValidateSupplyDeltaRejectsUnauthorizedMint();
    testValidateSupplyDeltaAcceptsAuthorizedMint();
    testValidateSupplyDeltaRejectsInvalidPolicy();
    return 0;
}
