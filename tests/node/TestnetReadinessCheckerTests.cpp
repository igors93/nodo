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

} // namespace

int main() {
    testReadyWhenAllCheckPass();
    testNotReadyWhenNoPeers();
    testNotReadyWhenGenesisNotVerified();
    testNotReadyWithLocalnetKeyOnTestnet();
    testDiagnosticsHaveNonEmptyCheckNames();
    testStatusToString();
    return 0;
}
