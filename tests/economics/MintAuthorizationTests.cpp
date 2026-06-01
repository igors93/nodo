#include "economics/MintAuthorization.hpp"
#include "economics/MonetaryPolicy.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

nodo::economics::MintAuthorization validAuth() {
    return nodo::economics::MintAuthorization(
        "auth-001",
        "NODO_MONETARY_POLICY_V1",
        1,
        10,
        nodo::utils::Amount::fromRawUnits(1000),
        "genesis allocation",
        "GENESIS_COMMITTEE"
    );
}

void testValidAuthorizationPasses() {
    const auto auth = validAuth();
    assert(auth.isValid());
    assert(auth.rejectionReason().empty());
    assert(auth.authorizationId() == "auth-001");
    assert(auth.policyVersion() == "NODO_MONETARY_POLICY_V1");
    assert(auth.epoch() == 1);
    assert(auth.expiresAtEpoch() == 10);
    assert(auth.maxMintAmount() == nodo::utils::Amount::fromRawUnits(1000));
    assert(auth.reason() == "genesis allocation");
    assert(auth.approvedBy() == "GENESIS_COMMITTEE");
}

void testEmptyAuthorizationIdRejected() {
    const nodo::economics::MintAuthorization auth(
        "", "NODO_MONETARY_POLICY_V1", 1, 10,
        nodo::utils::Amount::fromRawUnits(1000), "reason", "approver"
    );
    assert(!auth.isValid());
    assert(!auth.rejectionReason().empty());
}

void testEmptyPolicyVersionRejected() {
    const nodo::economics::MintAuthorization auth(
        "auth-002", "", 1, 10,
        nodo::utils::Amount::fromRawUnits(1000), "reason", "approver"
    );
    assert(!auth.isValid());
    assert(!auth.rejectionReason().empty());
}

void testZeroMaxMintAmountRejected() {
    const nodo::economics::MintAuthorization auth(
        "auth-003", "NODO_MONETARY_POLICY_V1", 1, 10,
        nodo::utils::Amount::fromRawUnits(0), "reason", "approver"
    );
    assert(!auth.isValid());
    assert(!auth.rejectionReason().empty());
}

void testEmptyReasonRejected() {
    const nodo::economics::MintAuthorization auth(
        "auth-004", "NODO_MONETARY_POLICY_V1", 1, 10,
        nodo::utils::Amount::fromRawUnits(1000), "", "approver"
    );
    assert(!auth.isValid());
}

void testEmptyApprovedByRejected() {
    const nodo::economics::MintAuthorization auth(
        "auth-005", "NODO_MONETARY_POLICY_V1", 1, 10,
        nodo::utils::Amount::fromRawUnits(1000), "reason", ""
    );
    assert(!auth.isValid());
}

void testExpiresBeforeEpochRejected() {
    const nodo::economics::MintAuthorization auth(
        "auth-006", "NODO_MONETARY_POLICY_V1",
        5, 3,  // expiresAtEpoch < epoch
        nodo::utils::Amount::fromRawUnits(1000), "reason", "approver"
    );
    assert(!auth.isValid());
    assert(auth.rejectionReason().find("expiresAtEpoch") != std::string::npos);
}

void testActiveAtStartEpoch() {
    const auto auth = validAuth();
    assert(auth.isActiveAtEpoch(1));
}

void testActiveAtExpiryEpoch() {
    const auto auth = validAuth();
    assert(auth.isActiveAtEpoch(10));
}

void testActiveAtMiddleEpoch() {
    const auto auth = validAuth();
    assert(auth.isActiveAtEpoch(5));
}

void testInactiveBeforeStartEpoch() {
    const auto auth = validAuth();
    assert(!auth.isActiveAtEpoch(0));
}

void testInactiveAfterExpiryEpoch() {
    const auto auth = validAuth();
    assert(!auth.isActiveAtEpoch(11));
}

void testSingleEpochAuthorizationActiveOnlyOnThatEpoch() {
    const nodo::economics::MintAuthorization auth(
        "auth-single", "NODO_MONETARY_POLICY_V1",
        7, 7,  // active only at epoch 7
        nodo::utils::Amount::fromRawUnits(500), "single epoch", "approver"
    );
    assert(auth.isValid());
    assert(auth.isActiveAtEpoch(7));
    assert(!auth.isActiveAtEpoch(6));
    assert(!auth.isActiveAtEpoch(8));
}

void testSerializationIncludesKeyFields() {
    const auto auth = validAuth();
    const std::string s = auth.serialize();
    assert(!s.empty());
    assert(s.find("auth-001") != std::string::npos);
    assert(s.find("NODO_MONETARY_POLICY_V1") != std::string::npos);
    assert(s.find("GENESIS_COMMITTEE") != std::string::npos);
    assert(s.find("1000") != std::string::npos);
}

// Item 4: createGenesisAuthorization factory tests.

nodo::economics::MonetaryPolicy testPolicyForFactory() {
    return nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-localnet-1",
        nodo::utils::Amount::fromRawUnits(1000000)
    );
}

void testGenesisAuthorizationHelperCreatesValidAuth() {
    const auto policy = testPolicyForFactory();
    const auto auth = nodo::economics::MintAuthorization::createGenesisAuthorization(
        policy, "genesis-auth-001", nodo::utils::Amount::fromRawUnits(500000)
    );
    assert(auth.isValid());
    assert(auth.rejectionReason().empty());
    assert(auth.authorizationId() == "genesis-auth-001");
    assert(auth.maxMintAmount() == nodo::utils::Amount::fromRawUnits(500000));
    assert(auth.epoch() == 0);
    assert(auth.expiresAtEpoch() == 0);
    assert(auth.approvedBy() == "GENESIS");
}

void testGenesisAuthorizationUsesPolicyVersion() {
    const auto policy = testPolicyForFactory();
    const auto auth = nodo::economics::MintAuthorization::createGenesisAuthorization(
        policy, "genesis-auth-002", nodo::utils::Amount::fromRawUnits(1000)
    );
    assert(auth.policyVersion() == policy.policyVersion());
    assert(auth.policyVersion() == "NODO_MONETARY_POLICY_V1");
}

void testGenesisAuthorizationMaxMintAmountMatchesInput() {
    const auto policy = testPolicyForFactory();
    const nodo::utils::Amount requestedAmount = nodo::utils::Amount::fromRawUnits(99999);
    const auto auth = nodo::economics::MintAuthorization::createGenesisAuthorization(
        policy, "genesis-auth-003", requestedAmount
    );
    assert(auth.maxMintAmount() == requestedAmount);
}

void testGenesisAuthorizationIsActiveAtEpochZero() {
    const auto policy = testPolicyForFactory();
    const auto auth = nodo::economics::MintAuthorization::createGenesisAuthorization(
        policy, "genesis-auth-004", nodo::utils::Amount::fromRawUnits(1000)
    );
    assert(auth.isActiveAtEpoch(0));
    assert(!auth.isActiveAtEpoch(1));
}

void testGenesisAuthorizationRejectsInvalidPolicy() {
    const nodo::economics::MonetaryPolicy invalidPolicy;  // default-constructed, empty
    bool threw = false;
    try {
        (void)nodo::economics::MintAuthorization::createGenesisAuthorization(
            invalidPolicy,
            "genesis-auth-bad",
            nodo::utils::Amount::fromRawUnits(1000)
        );
    } catch (const std::invalid_argument& e) {
        threw = true;
        const std::string msg(e.what());
        assert(msg.find("invalid policy") != std::string::npos ||
               msg.find("rejected") != std::string::npos);
    }
    assert(threw);
}

void testGenesisAuthorizationRejectsEmptyAuthorizationId() {
    const auto policy = testPolicyForFactory();
    bool threw = false;
    try {
        (void)nodo::economics::MintAuthorization::createGenesisAuthorization(
            policy, "", nodo::utils::Amount::fromRawUnits(1000)
        );
    } catch (const std::invalid_argument& e) {
        threw = true;
        const std::string msg(e.what());
        assert(msg.find("authorizationId") != std::string::npos ||
               msg.find("empty") != std::string::npos);
    }
    assert(threw);
}

void testGenesisAuthorizationRejectsNonPositiveMaxMintAmount() {
    const auto policy = testPolicyForFactory();
    bool threw = false;
    try {
        (void)nodo::economics::MintAuthorization::createGenesisAuthorization(
            policy, "genesis-auth-zero", nodo::utils::Amount::fromRawUnits(0)
        );
    } catch (const std::invalid_argument& e) {
        threw = true;
        const std::string msg(e.what());
        assert(msg.find("maxMintAmount") != std::string::npos ||
               msg.find("non-positive") != std::string::npos);
    }
    assert(threw);
}

} // namespace

int main() {
    testValidAuthorizationPasses();
    testEmptyAuthorizationIdRejected();
    testEmptyPolicyVersionRejected();
    testZeroMaxMintAmountRejected();
    testEmptyReasonRejected();
    testEmptyApprovedByRejected();
    testExpiresBeforeEpochRejected();
    testActiveAtStartEpoch();
    testActiveAtExpiryEpoch();
    testActiveAtMiddleEpoch();
    testInactiveBeforeStartEpoch();
    testInactiveAfterExpiryEpoch();
    testSingleEpochAuthorizationActiveOnlyOnThatEpoch();
    testSerializationIncludesKeyFields();
    // Item 4: factory tests
    testGenesisAuthorizationHelperCreatesValidAuth();
    testGenesisAuthorizationUsesPolicyVersion();
    testGenesisAuthorizationMaxMintAmountMatchesInput();
    testGenesisAuthorizationIsActiveAtEpochZero();
    // Correction 2: invalid-input guards
    testGenesisAuthorizationRejectsInvalidPolicy();
    testGenesisAuthorizationRejectsEmptyAuthorizationId();
    testGenesisAuthorizationRejectsNonPositiveMaxMintAmount();
    return 0;
}
