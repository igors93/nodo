#include "node/TestnetReadinessChecker.hpp"

#include "config/NetworkParameters.hpp"
#include "config/NetworkProfileRegistry.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/PublicKey.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::node::ReadinessDiagnostic;
using nodo::node::ReadinessStatus;
using nodo::node::TestnetReadinessChecker;
using nodo::node::TestnetReadinessCheckerConfig;

nodo::crypto::StoredKeyMetadata makeKey(const std::string& networkProfile = "localnet") {
    const nodo::crypto::PublicKey pk(
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        std::string(96, 'a')
    );
    return nodo::crypto::StoredKeyMetadata(
        "test-validator",
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        nodo::crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        nodo::crypto::KeyStoreKeyType::VALIDATOR,
        "BLST_BLS12_381_PROVIDER_V1",
        pk,
        "nodo1testaddress001",
        1900000000,
        networkProfile
    );
}

nodo::config::NetworkParameters localnetParams() {
    return nodo::config::NetworkParameters::developmentLocal();
}

nodo::config::NetworkParameters testnetCandidateParams() {
    return nodo::config::NetworkProfileRegistry::get("testnet-candidate");
}

void testLegacySignatureStillWorks() {
    const auto checks = TestnetReadinessChecker::check(
        localnetParams(), makeKey(), 1, true, 0
    );
    assert(!checks.empty());
    for (const auto& c : checks) {
        assert(!c.checkName().empty());
    }
}

void testP0GatesAllPassOnLocalnet() {
    const TestnetReadinessCheckerConfig config(
        1,      // connectedPeers
        true,   // genesisVerified
        0,      // finalizedHeight
        true,   // governanceLifecycleVerifierWired
        true,   // defenseModeInactive
        true,   // legacyPathsBlockedOnOfficialNetworks
        true    // treasuryReportConsistencyVerified
    );
    const auto checks = TestnetReadinessChecker::checkWithProtocolSafetyGates(
        localnetParams(), makeKey(), config
    );
    assert(!checks.empty());
    const auto status = TestnetReadinessChecker::summarize(checks);
    assert(status == ReadinessStatus::READY);
}

void testGovernanceLifecycleVerifierNotWiredFails() {
    const TestnetReadinessCheckerConfig config(
        1, true, 0,
        false,  // governanceLifecycleVerifierWired = false
        true, true, true
    );
    const auto checks = TestnetReadinessChecker::checkWithProtocolSafetyGates(
        localnetParams(), makeKey(), config
    );
    const auto status = TestnetReadinessChecker::summarize(checks);
    assert(status == ReadinessStatus::NOT_READY);

    bool foundFailedCheck = false;
    for (const auto& c : checks) {
        if (c.checkName() == "governance_lifecycle_verifier_wired" && !c.passed()) {
            foundFailedCheck = true;
        }
    }
    assert(foundFailedCheck);
}

void testDefenseModeActiveFailsOnOfficialNetwork() {
    const TestnetReadinessCheckerConfig config(
        1, true, 0,
        true,
        false,  // defenseModeInactive = false (defense is ACTIVE)
        true, true
    );
    const auto checks = TestnetReadinessChecker::checkWithProtocolSafetyGates(
        testnetCandidateParams(), makeKey("testnet-candidate"), config
    );
    const auto status = TestnetReadinessChecker::summarize(checks);
    assert(status == ReadinessStatus::NOT_READY);

    bool foundDefenseCheck = false;
    for (const auto& c : checks) {
        if (c.checkName() == "defense_mode_inactive" && !c.passed()) {
            foundDefenseCheck = true;
        }
    }
    assert(foundDefenseCheck);
}

void testDefenseModeActiveOkOnLocalnet() {
    const TestnetReadinessCheckerConfig config(
        1, true, 0, true,
        false,  // defense mode active
        true, true
    );
    const auto checks = TestnetReadinessChecker::checkWithProtocolSafetyGates(
        localnetParams(), makeKey(), config
    );
    // On localnet (non-official), defense mode active is not a fatal failure.
    bool defensePassed = false;
    for (const auto& c : checks) {
        if (c.checkName() == "defense_mode_inactive") {
            defensePassed = c.passed();
        }
    }
    assert(defensePassed);
}

void testTreasuryReportNotVerifiedFailsWhenBlocksExist() {
    const TestnetReadinessCheckerConfig config(
        1, true, 5,  // finalizedHeight=5 means blocks exist
        true, true, true,
        false   // treasuryReportConsistencyVerified = false
    );
    const auto checks = TestnetReadinessChecker::checkWithProtocolSafetyGates(
        localnetParams(), makeKey(), config
    );
    const auto status = TestnetReadinessChecker::summarize(checks);
    assert(status == ReadinessStatus::NOT_READY);
}

void testTreasuryReportNotVerifiedOkAtGenesis() {
    const TestnetReadinessCheckerConfig config(
        1, true, 0,  // finalizedHeight=0
        true, true, true,
        false   // treasuryReportConsistencyVerified = false
    );
    const auto checks = TestnetReadinessChecker::checkWithProtocolSafetyGates(
        localnetParams(), makeKey(), config
    );
    bool found = false;
    for (const auto& c : checks) {
        if (c.checkName() == "treasury_report_consistent") {
            assert(c.passed());
            found = true;
        }
    }
    assert(found);
}

} // namespace

int main() {
    testLegacySignatureStillWorks();
    testP0GatesAllPassOnLocalnet();
    testGovernanceLifecycleVerifierNotWiredFails();
    testDefenseModeActiveFailsOnOfficialNetwork();
    testDefenseModeActiveOkOnLocalnet();
    testTreasuryReportNotVerifiedFailsWhenBlocksExist();
    testTreasuryReportNotVerifiedOkAtGenesis();
    return 0;
}
