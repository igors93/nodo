#include "crypto/KeyEncryptionPolicy.hpp"

#include <cassert>
#include <string>

namespace {

void testLocalnetAcceptsPlaintext() {
    assert(nodo::crypto::KeyEncryptionPolicy::isAcceptable(
        nodo::crypto::KeyEncryptionLevel::PLAINTEXT,
        "localnet"
    ));
    assert(nodo::crypto::KeyEncryptionPolicy::isAcceptable(
        nodo::crypto::KeyEncryptionLevel::PLAINTEXT,
        "nodo-localnet"
    ));
}

void testTestnetRejectsPlaintext() {
    assert(!nodo::crypto::KeyEncryptionPolicy::isAcceptable(
        nodo::crypto::KeyEncryptionLevel::PLAINTEXT,
        "testnet"
    ));
    assert(!nodo::crypto::KeyEncryptionPolicy::isAcceptable(
        nodo::crypto::KeyEncryptionLevel::PLAINTEXT,
        "nodo-testnet"
    ));
    assert(!nodo::crypto::KeyEncryptionPolicy::isAcceptable(
        nodo::crypto::KeyEncryptionLevel::PLAINTEXT,
        "nodo-testnet-candidate"
    ));
}

void testTestnetAcceptsTestnetSafe() {
    assert(nodo::crypto::KeyEncryptionPolicy::isAcceptable(
        nodo::crypto::KeyEncryptionLevel::TESTNET_SAFE,
        "testnet"
    ));
    assert(nodo::crypto::KeyEncryptionPolicy::isAcceptable(
        nodo::crypto::KeyEncryptionLevel::TESTNET_SAFE,
        "nodo-testnet-candidate"
    ));
}

void testMainnetAlwaysBlocked() {
    assert(!nodo::crypto::KeyEncryptionPolicy::isAcceptable(
        nodo::crypto::KeyEncryptionLevel::TESTNET_SAFE,
        "mainnet"
    ));
    assert(!nodo::crypto::KeyEncryptionPolicy::isAcceptable(
        nodo::crypto::KeyEncryptionLevel::TESTNET_SAFE,
        "nodo-mainnet"
    ));
    assert(nodo::crypto::KeyEncryptionPolicy::isMainnetBlocked("mainnet"));
    assert(nodo::crypto::KeyEncryptionPolicy::isMainnetBlocked("nodo-mainnet"));
    assert(!nodo::crypto::KeyEncryptionPolicy::isMainnetBlocked("testnet"));
    assert(!nodo::crypto::KeyEncryptionPolicy::isMainnetBlocked("localnet"));
}

void testOfficialNetworkDetection() {
    assert(nodo::crypto::KeyEncryptionPolicy::isOfficialNetwork("testnet"));
    assert(nodo::crypto::KeyEncryptionPolicy::isOfficialNetwork("mainnet"));
    assert(nodo::crypto::KeyEncryptionPolicy::isOfficialNetwork("nodo-testnet-candidate"));
    assert(!nodo::crypto::KeyEncryptionPolicy::isOfficialNetwork("localnet"));
    assert(!nodo::crypto::KeyEncryptionPolicy::isOfficialNetwork("nodo-localnet"));
}

void testToString() {
    assert(
        nodo::crypto::keyEncryptionLevelToString(nodo::crypto::KeyEncryptionLevel::PLAINTEXT)
        == "PLAINTEXT"
    );
    assert(
        nodo::crypto::keyEncryptionLevelToString(nodo::crypto::KeyEncryptionLevel::TESTNET_SAFE)
        == "TESTNET_SAFE"
    );
}

} // namespace

int main() {
    testLocalnetAcceptsPlaintext();
    testTestnetRejectsPlaintext();
    testTestnetAcceptsTestnetSafe();
    testMainnetAlwaysBlocked();
    testOfficialNetworkDetection();
    testToString();
    return 0;
}
