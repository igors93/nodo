#include "node/TestnetReadinessChecker.hpp"
#include "node/EvidenceCaptureHealth.hpp"
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

using nodo::node::EvidenceCaptureHealth;
using nodo::node::EvidenceCaptureStatus;
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

// Test 5: Evidence capture healthy state passes readiness.
void testHealthyCapturePassesReadiness() {
    const TestnetReadinessCheckerConfig config(
        1, true, 0, true, true, true, true,
        true  // evidenceCaptureHealthy
    );
    const auto checks = TestnetReadinessChecker::checkWithProtocolSafetyGates(
        localnetParams(), makeKey(), config
    );
    bool found = false;
    for (const auto& c : checks) {
        if (c.checkName() == "evidence_capture_available") {
            assert(c.passed());
            found = true;
        }
    }
    assert(found);
}

// Test 6: Evidence capture unavailable state fails readiness.
void testUnavailableCaptureFailsReadiness() {
    const TestnetReadinessCheckerConfig config(
        1, true, 0, true, true, true, true,
        false  // evidenceCaptureHealthy = false
    );
    const auto checks = TestnetReadinessChecker::checkWithProtocolSafetyGates(
        localnetParams(), makeKey(), config
    );
    bool found = false;
    for (const auto& c : checks) {
        if (c.checkName() == "evidence_capture_available") {
            assert(!c.passed());
            found = true;
        }
    }
    assert(found);
    assert(TestnetReadinessChecker::summarize(checks) == ReadinessStatus::NOT_READY);
}

// Disabled capture (default EvidenceCaptureHealth) gives healthy = true.
void testDisabledCaptureCountsAsHealthy() {
    EvidenceCaptureHealth health;
    assert(health.status() == EvidenceCaptureStatus::DISABLED);
    assert(health.isHealthy());

    // Default evidenceCaptureHealthy = true in config → passes gate.
    const TestnetReadinessCheckerConfig config(
        1, true, 0, true, true, true, true
        // evidenceCaptureHealthy defaults to true
    );
    const auto checks = TestnetReadinessChecker::checkWithProtocolSafetyGates(
        localnetParams(), makeKey(), config
    );
    for (const auto& c : checks) {
        if (c.checkName() == "evidence_capture_available") {
            assert(c.passed());
        }
    }
}

// Test 7: Evidence persistence failure appears in diagnostics (serialized output).
void testCapturePersistenceFailureAppearsInDiagnostics() {
    EvidenceCaptureHealth health;
    health.recordFailure("disk full", 1900000001);
    assert(health.status() == EvidenceCaptureStatus::PERSIST_FAILURE);
    assert(!health.isHealthy());

    const std::string serialized = health.serialize();
    assert(!serialized.empty());
    assert(serialized.find("PERSIST_FAILURE") != std::string::npos ||
           serialized.find("disk full") != std::string::npos);
}

} // namespace

int main() {
    testHealthyCapturePassesReadiness();
    testUnavailableCaptureFailsReadiness();
    testDisabledCaptureCountsAsHealthy();
    testCapturePersistenceFailureAppearsInDiagnostics();
    return 0;
}
