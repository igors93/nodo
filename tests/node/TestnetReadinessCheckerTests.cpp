#include "node/TestnetReadinessChecker.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/PublicKey.hpp"

#include <cassert>
#include <string>

namespace {

nodo::crypto::StoredKeyMetadata makeKey(const std::string& networkProfile) {
    const nodo::crypto::PublicKey pk(
        nodo::crypto::CryptoAlgorithm::BLS12_381,
        std::string(96, 'a')
    );
    return nodo::crypto::StoredKeyMetadata(
        "test-key-001",
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

void testReadyWhenAllCheckPass() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto key = makeKey("localnet");
    const auto checks = nodo::node::TestnetReadinessChecker::check(
        params, key, 3, true, 10
    );
    const auto status = nodo::node::TestnetReadinessChecker::summarize(checks);
    assert(status == nodo::node::ReadinessStatus::READY);
}

void testNotReadyWhenNoPeers() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto key = makeKey("localnet");
    const auto checks = nodo::node::TestnetReadinessChecker::check(
        params, key, 0, true, 0
    );
    const auto status = nodo::node::TestnetReadinessChecker::summarize(checks);
    assert(status == nodo::node::ReadinessStatus::NOT_READY);
}

void testNotReadyWhenGenesisNotVerified() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto key = makeKey("localnet");
    const auto checks = nodo::node::TestnetReadinessChecker::check(
        params, key, 1, false, 0
    );
    const auto status = nodo::node::TestnetReadinessChecker::summarize(checks);
    assert(status == nodo::node::ReadinessStatus::NOT_READY);
}

void testNotReadyWithLocalnetKeyOnTestnet() {
    const auto params = nodo::config::NetworkParameters::testnetCandidate();
    const auto key = makeKey("localnet"); // localnet key on testnet
    const auto checks = nodo::node::TestnetReadinessChecker::check(
        params, key, 3, true, 5
    );
    const auto status = nodo::node::TestnetReadinessChecker::summarize(checks);
    assert(status == nodo::node::ReadinessStatus::NOT_READY);
}

void testDiagnosticsHaveNonEmptyCheckNames() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto key = makeKey("localnet");
    const auto checks = nodo::node::TestnetReadinessChecker::check(
        params, key, 1, true, 1
    );
    assert(!checks.empty());
    for (const auto& c : checks) {
        assert(!c.checkName().empty());
        const std::string s = c.serialize();
        assert(!s.empty());
    }
}

void testStatusToString() {
    assert(nodo::node::readinessStatusToString(nodo::node::ReadinessStatus::READY) == "READY");
    assert(nodo::node::readinessStatusToString(nodo::node::ReadinessStatus::NOT_READY) == "NOT_READY");
}

// Protocol safety gates: localnet with all gates satisfied must be READY.
void testProtocolSafetyGatesPassOnLocalnet() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto key = makeKey("localnet");

    const nodo::node::TestnetReadinessCheckerConfig config(
        /*connectedPeers*/                   3,
        /*genesisVerified*/                  true,
        /*finalizedHeight*/                  0,
        /*governanceLifecycleVerifierWired*/ true,
        /*defenseModeInactive*/              true,
        /*legacyPathsBlockedOnOfficial*/     true,
        /*treasuryReportConsistencyVerified*/true,
        /*evidenceCaptureHealthy*/           true,
        /*chainAuditCompleted*/              false  // no blocks, so gate is skipped
    );

    const auto checks = nodo::node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto status = nodo::node::TestnetReadinessChecker::summarize(checks);
    assert(status == nodo::node::ReadinessStatus::READY);
}

// Protocol safety gates: official network with finalized blocks and no chain audit must be NOT_READY.
void testOfficialNetworkNotReadyWithoutChainAudit() {
    const auto params = nodo::config::NetworkParameters::testnetCandidate();
    const auto key    = makeKey("testnet-candidate");

    const nodo::node::TestnetReadinessCheckerConfig config(
        /*connectedPeers*/                   3,
        /*genesisVerified*/                  true,
        /*finalizedHeight*/                  10,   // has finalized blocks
        /*governanceLifecycleVerifierWired*/ true,
        /*defenseModeInactive*/              true,
        /*legacyPathsBlockedOnOfficial*/     true,
        /*treasuryReportConsistencyVerified*/true,
        /*evidenceCaptureHealthy*/           true,
        /*chainAuditCompleted*/              false  // audit not done
    );

    const auto checks = nodo::node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto status = nodo::node::TestnetReadinessChecker::summarize(checks);
    assert(status == nodo::node::ReadinessStatus::NOT_READY);

    // Verify the failing check is the chain audit gate.
    bool foundChainAuditCheck = false;
    bool chainAuditCheckFailed = false;
    for (const auto& c : checks) {
        if (c.checkName() == "chain_audit_completed") {
            foundChainAuditCheck = true;
            chainAuditCheckFailed = !c.passed();
        }
    }
    assert(foundChainAuditCheck && "chain_audit_completed check must be present");
    assert(chainAuditCheckFailed && "chain_audit_completed check must fail when audit not done");
}

// Protocol safety gates: official network with finalized blocks AND completed audit must pass the gate.
void testOfficialNetworkReadyWhenChainAuditCompleted() {
    const auto params = nodo::config::NetworkParameters::testnetCandidate();
    const auto key    = makeKey("testnet-candidate");

    const nodo::node::TestnetReadinessCheckerConfig config(
        /*connectedPeers*/                   3,
        /*genesisVerified*/                  true,
        /*finalizedHeight*/                  10,
        /*governanceLifecycleVerifierWired*/ true,
        /*defenseModeInactive*/              true,
        /*legacyPathsBlockedOnOfficial*/     true,
        /*treasuryReportConsistencyVerified*/true,
        /*evidenceCaptureHealthy*/           true,
        /*chainAuditCompleted*/              true   // audit done
    );

    const auto checks = nodo::node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );

    // chain_audit_completed gate must pass.
    for (const auto& c : checks) {
        if (c.checkName() == "chain_audit_completed") {
            assert(c.passed() && "chain_audit_completed gate must pass when audit is done");
        }
    }
}

// Protocol safety gates: localnet with no finalized blocks skips the chain audit gate.
void testChainAuditGateSkippedWithNoBlocks() {
    const auto params = nodo::config::NetworkParameters::testnetCandidate();
    const auto key    = makeKey("testnet-candidate");

    const nodo::node::TestnetReadinessCheckerConfig config(
        /*connectedPeers*/                   3,
        /*genesisVerified*/                  true,
        /*finalizedHeight*/                  0,    // no finalized blocks
        /*governanceLifecycleVerifierWired*/ true,
        /*defenseModeInactive*/              true,
        /*legacyPathsBlockedOnOfficial*/     true,
        /*treasuryReportConsistencyVerified*/true,
        /*evidenceCaptureHealthy*/           true,
        /*chainAuditCompleted*/              false  // not done, but no blocks
    );

    const auto checks = nodo::node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );

    // chain_audit_completed gate must pass because there are no finalized blocks.
    for (const auto& c : checks) {
        if (c.checkName() == "chain_audit_completed") {
            assert(c.passed() && "chain_audit_completed gate must pass when no finalized blocks exist");
        }
    }
}

// Protocol safety gates: unhealthy evidence capture must block readiness on any network.
void testEvidenceCaptureUnhealthyBlocksReadiness() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto key    = makeKey("localnet");

    const nodo::node::TestnetReadinessCheckerConfig config(
        /*connectedPeers*/                   3,
        /*genesisVerified*/                  true,
        /*finalizedHeight*/                  0,
        /*governanceLifecycleVerifierWired*/ true,
        /*defenseModeInactive*/              true,
        /*legacyPathsBlockedOnOfficial*/     true,
        /*treasuryReportConsistencyVerified*/true,
        /*evidenceCaptureHealthy*/           false, // unhealthy
        /*chainAuditCompleted*/              false
    );

    const auto checks = nodo::node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );
    const auto status = nodo::node::TestnetReadinessChecker::summarize(checks);
    assert(status == nodo::node::ReadinessStatus::NOT_READY);
}

// Protocol safety gates: verify the check list includes all expected gate names.
void testProtocolSafetyGateNamesArePresent() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto key    = makeKey("localnet");

    const nodo::node::TestnetReadinessCheckerConfig config(
        3, true, 0, true, true, true, true, true, false
    );

    const auto checks = nodo::node::TestnetReadinessChecker::checkWithProtocolSafetyGates(
        params, key, config
    );

    const std::vector<std::string> expectedGates = {
        "governance_lifecycle_verifier_wired",
        "defense_mode_inactive",
        "legacy_paths_blocked_on_official_network",
        "treasury_report_consistent",
        "evidence_capture_available",
        "chain_audit_completed"
    };

    for (const auto& gateName : expectedGates) {
        bool found = false;
        for (const auto& c : checks) {
            if (c.checkName() == gateName) {
                found = true;
                break;
            }
        }
        assert(found && ("Missing expected readiness gate: " + gateName).c_str());
    }
}

} // namespace

int main() {
    testReadyWhenAllCheckPass();
    testNotReadyWhenNoPeers();
    testNotReadyWhenGenesisNotVerified();
    testNotReadyWithLocalnetKeyOnTestnet();
    testDiagnosticsHaveNonEmptyCheckNames();
    testStatusToString();
    testProtocolSafetyGatesPassOnLocalnet();
    testOfficialNetworkNotReadyWithoutChainAudit();
    testOfficialNetworkReadyWhenChainAuditCompleted();
    testChainAuditGateSkippedWithNoBlocks();
    testEvidenceCaptureUnhealthyBlocksReadiness();
    testProtocolSafetyGateNamesArePresent();
    return 0;
}
