#ifndef NODO_NODE_PEER_HANDSHAKE_AUTO_REGISTRAR_HPP
#define NODO_NODE_PEER_HANDSHAKE_AUTO_REGISTRAR_HPP

#include "node/ChainSyncMessages.hpp"
#include "crypto/KeyPair.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/Peer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

struct HandshakeRegistrationResult {
    std::string peerId;
    bool        registered = false;
    bool        alreadyKnown = false;
    std::string reason;
};

/*
 * PeerHandshakeAutoRegistrar processes PEER_HELLO messages from the gossip inbox
 * and automatically registers the sending peer in the local GossipMesh.
 *
 * On successful registration, sends CHAIN_STATUS back so the new peer knows
 * our current chain height and can decide to request block sync.
 */
class PeerHandshakeAutoRegistrar {
public:
    /*
     * Process all PEER_HELLO messages in the gossip inbox.
     * For each valid hello:
     *   1. Validate hello (networkId, chainId, genesisId must match).
     *   2. Register peer in gossip.peerRegistry().
     *   3. Broadcast our CHAIN_STATUS back.
     * Returns list of registration results.
     */
    static std::vector<HandshakeRegistrationResult> processInbox(
        p2p::GossipMesh&              gossip,
        const ChainStatusMessage&     localChainStatus,
        std::int64_t                  now
    );

    /*
     * Build and broadcast a PEER_HELLO to a specific peer for outbound
     * connection initiation.
     */
    static p2p::GossipDeliveryReport sendHello(
        p2p::GossipMesh&          gossip,
        const p2p::PeerMetadata&  localPeer,
        const ChainStatusMessage& localChainStatus,
        const crypto::KeyPair&    nodeIdentityKey,
        std::int64_t              now
    );

    static p2p::GossipDeliveryReport sendHelloTo(
        p2p::GossipMesh&          gossip,
        const std::string&        targetNodeId,
        const p2p::PeerMetadata&  localPeer,
        const ChainStatusMessage& localChainStatus,
        const crypto::KeyPair&    nodeIdentityKey,
        std::int64_t              now
    );
};

} // namespace nodo::node

#endif
