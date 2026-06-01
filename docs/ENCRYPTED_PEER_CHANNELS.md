# Encrypted Peer Channels

This phase adds the first encrypted and authenticated peer-channel boundary for Nodo testnet networking.

It does not claim Nodo is ready for public mainnet networking. It gives the project a clean layer where peer-to-peer traffic can be sealed before it is handed to an underlying transport such as loopback or TCP.

## Added modules

- `p2p::EncryptedPeerChannelFrame`
- `p2p::EncryptedPeerSession`
- `p2p::EncryptedPeerChannelCodec`
- `p2p::EncryptedPeerTransport`
- `p2p::EncryptedPeerHandshakeHello`
- `p2p::EncryptedPeerHandshakeAccept`
- `p2p::EncryptedPeerHandshakeManager`

## What works now

A pair of local peers can:

1. create a handshake challenge;
2. validate a handshake response;
3. derive a deterministic session id from both peer ids and a shared testnet secret;
4. seal a `NetworkEnvelope` into an encrypted channel frame;
5. authenticate that frame before decrypting it;
6. reject tampered frames;
7. reject replayed sequence numbers;
8. wrap an existing `Transport` so gossip can use encrypted delivery without changing consensus code.

## Why this is a separate layer

The transport layer should move bytes.
The gossip layer should route valid messages.
The consensus layer should decide protocol state.
The encrypted peer channel sits between transport and gossip, so cryptographic framing does not leak into consensus rules.

## Security note

This phase is a testnet hardening boundary. It adds confidentiality, message authentication and replay protection to the project architecture using the existing canonical encoding and hash boundaries.

For public network use, this boundary should later be backed by an audited key exchange and AEAD construction, and peer identity should be bound to production validator or node keys.

## Not included yet

- production-grade Noise/TLS/libp2p secure transport;
- certificate or validator-key based peer authentication;
- automatic secure-session negotiation inside the TCP runtime;
- persistent secure-session resumption;
- key rotation;
- public testnet peer discovery.

Those should come after this boundary is stable and tested.
