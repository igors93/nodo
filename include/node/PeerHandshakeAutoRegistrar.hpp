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
 * PeerHandshakeAutoRegistrar drives the challenge/response handshake and only
 * registers a peer after consuming its one-time challenge nonce.
 *
 * On successful registration, sends CHAIN_STATUS back so the new peer knows
 * our current chain height and can decide to request block sync.
 */
class PeerHandshakeAutoRegistrar {
public:
    /*
     * Process all PEER_HELLO and PEER_CHALLENGE messages in the gossip inbox.
     * For each valid challenge response:
     *   1. Validate hello (networkId, chainId, genesisId must match).
     *   2. Register peer in gossip.peerRegistry().
     *   3. Broadcast our CHAIN_STATUS back.
     * Returns list of registration results.
     */
    static std::vector<HandshakeRegistrationResult> processInbox(
        p2p::GossipMesh&              gossip,
        const p2p::PeerMetadata&      localPeer,
        const ChainStatusMessage&     localChainStatus,
        const crypto::KeyPair&        nodeIdentityKey,
        std::int64_t                  now
    );

    /*
     * Start an authenticated handshake by issuing a fresh one-time challenge.
     */
    static p2p::GossipDeliveryReport initiateHandshake(
        p2p::GossipMesh&          gossip,
        const std::string&        targetNodeId,
        std::int64_t              now,
        p2p::TransportConnectionId connectionId = 0
    );
};

} // namespace nodo::node

#endif
