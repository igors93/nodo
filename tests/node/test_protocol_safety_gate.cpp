#include "node/ProtocolSafetyGate.hpp"
#include "crypto/KeyStore.hpp"

#include <cassert>

int main() {
    const nodo::config::NetworkParameters local =
        nodo::config::NetworkParameters::developmentLocal();

    const nodo::node::ProtocolSafetyDecision localDecision =
        nodo::node::ProtocolSafetyGate::evaluateLocalKeyCreation(
            local,
            nodo::crypto::KeyStore::LOCAL_NETWORK_PROFILE,
            true,
            true
        );

    assert(localDecision.allowed());

    const nodo::config::NetworkParameters testnet(
        "nodo-testnet-1",
        "nodo-testnet",
        "nodo/0.1",
        60,
        1,
        2,
        3,
        1000,
        128,
        10000,
        1,
        60,
        1,
        "NODO_CRYPTO_SUITE_V1",
        "NODO_STORAGE_V2"
    );

    const nodo::node::ProtocolSafetyDecision testnetDecision =
        nodo::node::ProtocolSafetyGate::evaluateLocalKeyCreation(
            testnet,
            nodo::crypto::KeyStore::LOCAL_NETWORK_PROFILE,
            true,
            true
        );

    assert(!testnetDecision.allowed());

    return 0;
}
