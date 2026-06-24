#include "economics/MonetaryPolicy.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

void testLocalnetDefaultIsValid() {
    const auto policy = nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-localnet-1",
        nodo::utils::Amount::fromRawUnits(1000000000)
    );
    assert(policy.isValid());
    assert(policy.rejectionReason().empty());
    assert(policy.policyVersion() == "NODO_MONETARY_POLICY_V1");
    assert(policy.chainId() == "nodo-localnet-1");
    assert(policy.unitName() == "NODO");
    assert(policy.baseUnitName() == "raw");
    assert(policy.baseUnitsPerUnit() == 100000000ULL);
    assert(policy.initialSupply() == nodo::utils::Amount::fromRawUnits(1000000000));
    assert(policy.maxAnnualInflationBasisPoints() == 400);
}

void testEmptyPolicyVersionRejected() {
    const nodo::economics::MonetaryPolicy policy(
        "",
        "nodo-localnet-1",
        "NODO",
        "raw",
        100000000,
        nodo::utils::Amount::fromRawUnits(1000),
        400
    );
    assert(!policy.isValid());
    assert(!policy.rejectionReason().empty());
}

void testEmptyChainIdRejected() {
    const nodo::economics::MonetaryPolicy policy(
        "NODO_MONETARY_POLICY_V1",
        "",
        "NODO",
        "raw",
        100000000,
        nodo::utils::Amount::fromRawUnits(1000),
        400
    );
    assert(!policy.isValid());
    assert(!policy.rejectionReason().empty());
}

void testWrongUnitNameRejected() {
    const nodo::economics::MonetaryPolicy policy(
        "NODO_MONETARY_POLICY_V1",
        "nodo-testnet-1",
        "NOTODO",
        "raw",
        100000000,
        nodo::utils::Amount::fromRawUnits(1000),
        400
    );
    assert(!policy.isValid());
}

void testZeroBaseUnitsRejected() {
    const nodo::economics::MonetaryPolicy policy(
        "NODO_MONETARY_POLICY_V1",
        "nodo-localnet-1",
        "NODO",
        "raw",
        0,
        nodo::utils::Amount::fromRawUnits(1000),
        400
    );
    assert(!policy.isValid());
    assert(!policy.rejectionReason().empty());
}

void testNegativeInitialSupplyRejected() {
    // Amount constructor now rejects negatives before MonetaryPolicy sees them.
    bool threw = false;
    try { (void)nodo::utils::Amount(-1); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}

void testInflationAbove400BasisPointsRejected() {
    const nodo::economics::MonetaryPolicy policy(
        "NODO_MONETARY_POLICY_V1",
        "nodo-localnet-1",
        "NODO",
        "raw",
        100000000,
        nodo::utils::Amount::fromRawUnits(1000),
        401
    );
    assert(!policy.isValid());
    assert(!policy.rejectionReason().empty());
}

void testExactly400BasisPointsAllowed() {
    const nodo::economics::MonetaryPolicy policy(
        "NODO_MONETARY_POLICY_V1",
        "nodo-localnet-1",
        "NODO",
        "raw",
        100000000,
        nodo::utils::Amount::fromRawUnits(1000),
        400
    );
    assert(policy.isValid());
}

void testZeroInitialSupplyIsValid() {
    const nodo::economics::MonetaryPolicy policy(
        "NODO_MONETARY_POLICY_V1",
        "nodo-localnet-1",
        "NODO",
        "raw",
        100000000,
        nodo::utils::Amount::fromRawUnits(0),
        100
    );
    assert(policy.isValid());
}

void testSerializationIncludesKeyFields() {
    const auto policy = nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-testnet-1",
        nodo::utils::Amount::fromRawUnits(5000)
    );
    const std::string s = policy.serialize();
    assert(!s.empty());
    assert(s.find("NODO_MONETARY_POLICY_V1") != std::string::npos);
    assert(s.find("nodo-testnet-1") != std::string::npos);
    assert(s.find("NODO") != std::string::npos);
    assert(s.find("5000") != std::string::npos);
}

} // namespace

int main() {
    testLocalnetDefaultIsValid();
    testEmptyPolicyVersionRejected();
    testEmptyChainIdRejected();
    testWrongUnitNameRejected();
    testZeroBaseUnitsRejected();
    testNegativeInitialSupplyRejected();
    testInflationAbove400BasisPointsRejected();
    testExactly400BasisPointsAllowed();
    testZeroInitialSupplyIsValid();
    testSerializationIncludesKeyFields();
    return 0;
}
