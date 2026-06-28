#include "p2p/PeerSessionKeyAgreement.hpp"

#include <cassert>

using namespace nodo::p2p;

int main() {
    const auto challenger =
        PeerSessionKeyAgreement::generateEphemeralKeyPair();
    const auto challenged =
        PeerSessionKeyAgreement::generateEphemeralKeyPair();
    assert(challenger.has_value());
    assert(challenged.has_value());
    assert(challenger->isValid());
    assert(challenged->isValid());

    const PeerSessionContext context{
        "localnet",
        "chain-localnet",
        "1",
        "node-a",
        "node-b",
        std::string(64, 'a'),
        challenger->publicKeyHex,
        challenged->publicKeyHex
    };
    const auto challengerSecret =
        PeerSessionKeyAgreement::deriveSessionSecret(
            challenger->privateKeyHex,
            challenged->publicKeyHex,
            context
        );
    const auto challengedSecret =
        PeerSessionKeyAgreement::deriveSessionSecret(
            challenged->privateKeyHex,
            challenger->publicKeyHex,
            context
        );
    assert(challengerSecret.has_value());
    assert(challengedSecret.has_value());
    assert(*challengerSecret == *challengedSecret);
    assert(challengerSecret->size() == 64);

    PeerSessionContext tampered = context;
    tampered.challengedNodeId = "node-c";
    const auto tamperedSecret =
        PeerSessionKeyAgreement::deriveSessionSecret(
            challenger->privateKeyHex,
            challenged->publicKeyHex,
            tampered
        );
    assert(tamperedSecret.has_value());
    assert(*tamperedSecret != *challengerSecret);

    assert(!PeerSessionKeyAgreement::deriveSessionSecret(
        "00",
        challenged->publicKeyHex,
        context
    ).has_value());

    return 0;
}
