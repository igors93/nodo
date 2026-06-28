#ifndef NODO_P2P_PEER_SESSION_KEY_AGREEMENT_HPP
#define NODO_P2P_PEER_SESSION_KEY_AGREEMENT_HPP

#include <optional>
#include <string>

namespace nodo::p2p {

struct PeerEphemeralKeyPair {
    std::string publicKeyHex;
    std::string privateKeyHex;

    bool isValid() const;
};

struct PeerSessionContext {
    std::string networkId;
    std::string chainId;
    std::string protocolVersion;
    std::string challengerNodeId;
    std::string challengedNodeId;
    std::string challengeNonce;
    std::string challengerEphemeralPublicKeyHex;
    std::string challengedEphemeralPublicKeyHex;

    bool isValid() const;
    std::string canonicalTranscript() const;
};

class PeerSessionKeyAgreement {
public:
    static constexpr std::size_t KEY_BYTES = 32;

    static std::optional<PeerEphemeralKeyPair> generateEphemeralKeyPair();

    static std::optional<std::string> deriveSessionSecret(
        const std::string& localPrivateKeyHex,
        const std::string& remotePublicKeyHex,
        const PeerSessionContext& context
    );

    static bool isValidPublicKey(const std::string& publicKeyHex);
    static bool isValidPrivateKey(const std::string& privateKeyHex);
};

} // namespace nodo::p2p

#endif
