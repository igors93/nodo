#include "node/ProductionKeySafetyGate.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/PublicKey.hpp"

#include <cassert>
#include <string>

namespace {

nodo::crypto::StoredKeyMetadata makeKey(
    const std::string& networkProfile,
    nodo::crypto::KeyEncryptionLevel encryptionLevel =
        nodo::crypto::KeyEncryptionLevel::PLAINTEXT
) {
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
        networkProfile,
        encryptionLevel
    );
}

void testLocalnetKeyApprovedOnLocalnet() {
    const auto key = makeKey("localnet");
    const auto result = nodo::node::ProductionKeySafetyGate::check(key, "localnet");
    assert(result.isApproved());
}

void testLocalnetKeyRejectedOnTestnet() {
    const auto key = makeKey("localnet");
    const auto result = nodo::node::ProductionKeySafetyGate::check(key, "testnet-candidate");
    assert(!result.isApproved());
    assert(result.status() ==
           nodo::node::KeySafetyStatus::REJECTED_PLAINTEXT_ON_OFFICIAL_NETWORK);
}

void testLocalnetKeyRejectedOnTestnetByName() {
    const auto key = makeKey("localnet");
    const auto result = nodo::node::ProductionKeySafetyGate::check(key, "testnet");
    assert(!result.isApproved());
}

void testMainnetAlwaysRejected() {
    // Even a testnet-capable key must be rejected on mainnet.
    const auto key = makeKey("testnet-candidate");
    const auto result = nodo::node::ProductionKeySafetyGate::check(key, "mainnet");
    assert(!result.isApproved());
    assert(result.status() == nodo::node::KeySafetyStatus::REJECTED_MAINNET_NOT_READY);
}

void testMainnetRejectedWithLocalnetKey() {
    const auto key = makeKey("localnet");
    const auto result = nodo::node::ProductionKeySafetyGate::check(key, "mainnet");
    assert(!result.isApproved());
    assert(result.status() == nodo::node::KeySafetyStatus::REJECTED_MAINNET_NOT_READY);
}

void testNetworkMismatchRejected() {
    // Key bound to testnet-candidate, but trying to start on a different official network.
    const auto key = makeKey("testnet-candidate");
    const auto result = nodo::node::ProductionKeySafetyGate::check(key, "nodo-other-network");
    // Key profile="testnet-candidate", target="nodo-other-network" → mismatch.
    assert(!result.isApproved());
    assert(result.status() == nodo::node::KeySafetyStatus::REJECTED_NETWORK_MISMATCH);
}

void testTestnetSafeKeyApprovedOnTestnetCandidate() {
    const auto key = makeKey(
        "testnet-candidate",
        nodo::crypto::KeyEncryptionLevel::TESTNET_SAFE
    );
    const auto result =
        nodo::node::ProductionKeySafetyGate::check(key, "testnet-candidate");
    assert(result.isApproved());
}

void testDevEncryptedKeyRejectedOnTestnetCandidate() {
    // DEV_ENCRYPTED is encrypted, but below the TESTNET_SAFE bar
    // KeyEncryptionPolicy requires for official networks — this is the gap
    // that used to pass silently before ProductionKeySafetyGate reused
    // KeyEncryptionPolicy::isAcceptable.
    const auto key = makeKey(
        "testnet-candidate",
        nodo::crypto::KeyEncryptionLevel::DEV_ENCRYPTED
    );
    const auto result =
        nodo::node::ProductionKeySafetyGate::check(key, "testnet-candidate");
    assert(!result.isApproved());
    assert(result.status() ==
           nodo::node::KeySafetyStatus::REJECTED_INSUFFICIENT_ENCRYPTION_LEVEL);
}

void testStatusToString() {
    assert(nodo::node::keySafetyStatusToString(nodo::node::KeySafetyStatus::APPROVED)
           == "APPROVED");
    assert(nodo::node::keySafetyStatusToString(
               nodo::node::KeySafetyStatus::REJECTED_MAINNET_NOT_READY)
           == "REJECTED_MAINNET_NOT_READY");
}

} // namespace

int main() {
    testLocalnetKeyApprovedOnLocalnet();
    testLocalnetKeyRejectedOnTestnet();
    testLocalnetKeyRejectedOnTestnetByName();
    testMainnetAlwaysRejected();
    testMainnetRejectedWithLocalnetKey();
    testNetworkMismatchRejected();
    testTestnetSafeKeyApprovedOnTestnetCandidate();
    testDevEncryptedKeyRejectedOnTestnetCandidate();
    testStatusToString();
    return 0;
}
