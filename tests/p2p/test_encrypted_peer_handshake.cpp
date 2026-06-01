#include "p2p/EncryptedPeerHandshake.hpp"

#include <cassert>

using namespace nodo::p2p;

int main() {
    const EncryptedPeerHandshakeHello hello =
        EncryptedPeerHandshakeManager::createHello(
            "node-a",
            "fingerprint-a",
            100
        );

    assert(hello.isValid());

    const EncryptedPeerHandshakeAccept accept =
        EncryptedPeerHandshakeManager::createAccept(
            hello,
            "node-b",
            "fingerprint-b",
            "shared-channel-secret",
            101
        );

    assert(accept.isValid());

    const EncryptedPeerHandshakeResult accepted =
        EncryptedPeerHandshakeManager::validateAccept(
            hello,
            accept,
            "node-b",
            "fingerprint-b",
            "shared-channel-secret"
        );

    assert(accepted.accepted());
    assert(!accepted.sessionId().empty());

    const EncryptedPeerHandshakeResult wrongSecret =
        EncryptedPeerHandshakeManager::validateAccept(
            hello,
            accept,
            "node-b",
            "fingerprint-b",
            "wrong-secret"
        );

    assert(!wrongSecret.accepted());

    const EncryptedPeerHandshakeResult wrongPeer =
        EncryptedPeerHandshakeManager::validateAccept(
            hello,
            accept,
            "node-c",
            "fingerprint-b",
            "shared-channel-secret"
        );

    assert(!wrongPeer.accepted());

    return 0;
}
