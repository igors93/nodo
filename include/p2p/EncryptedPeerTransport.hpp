#ifndef NODO_P2P_ENCRYPTED_PEER_TRANSPORT_HPP
#define NODO_P2P_ENCRYPTED_PEER_TRANSPORT_HPP

#include "p2p/AuthenticatedSessionTransport.hpp"
#include "p2p/EncryptedPeerChannel.hpp"
#include "p2p/Transport.hpp"

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace nodo::p2p {

class EncryptedPeerTransport final : public Transport,
                                     public AuthenticatedSessionTransport {
public:
  explicit EncryptedPeerTransport(Transport &underlyingTransport);

  bool establishSession(const std::string &localNodeId,
                        const std::string &remoteNodeId,
                        const std::string &sharedSecret, std::int64_t now);

  bool stageOutboundSession(const std::string &localNodeId,
                            const std::string &remoteNodeId,
                            const std::string &sharedSecret,
                            std::int64_t now) override;

  bool activateOutboundSession(const std::string &localNodeId,
                               const std::string &remoteNodeId) override;

  bool establishInboundSession(const std::string &localNodeId,
                               const std::string &remoteNodeId,
                               const std::string &sharedSecret,
                               std::int64_t now) override;

  bool removeSession(const std::string &localNodeId,
                     const std::string &remoteNodeId) override;

  bool rejectPendingConnection(const std::string &localNodeId,
                               const std::string &remoteNodeId) override;

  bool hasOutboundSession(const std::string &localNodeId,
                          const std::string &remoteNodeId) const override;

  bool hasInboundSession(const std::string &localNodeId,
                         const std::string &remoteNodeId) const override;

  bool hasSession(const std::string &localNodeId,
                  const std::string &remoteNodeId) const;

  std::size_t sessionCount() const;

  std::size_t rejectedFrameCount() const;

  void clearSessions();

  TransportResult connect(const std::string &localNodeId,
                          const std::string &remoteNodeId) override;

  TransportResult disconnect(const std::string &localNodeId,
                             const std::string &remoteNodeId) override;

  bool connected(const std::string &localNodeId,
                 const std::string &remoteNodeId) const override;

  TransportResult send(const TransportMessage &message) override;

  std::optional<TransportMessage> poll(const std::string &localNodeId) override;

private:
  mutable std::mutex m_mutex;
  Transport &m_underlyingTransport;
  std::map<std::string, EncryptedPeerSession> m_outboundSessions;
  std::map<std::string, EncryptedPeerSession> m_inboundSessions;
  std::map<std::string, EncryptedPeerSession> m_stagedOutboundSessions;
  std::map<std::string, TransportConnectionId> m_pendingConnections;
  AuthenticatedConnectionTransport *m_connectionTransport;
  std::size_t m_rejectedFrameCount;

  static std::string directionKey(const std::string &localNodeId,
                                  const std::string &remoteNodeId);

  static bool isHandshakeMessage(NetworkMessageType type);

  EncryptedPeerSession *outboundSession(const std::string &localNodeId,
                                        const std::string &remoteNodeId);

  const EncryptedPeerSession *
  outboundSession(const std::string &localNodeId,
                  const std::string &remoteNodeId) const;

  EncryptedPeerSession *inboundSession(const std::string &localNodeId,
                                       const std::string &remoteNodeId);

  const EncryptedPeerSession *
  inboundSession(const std::string &localNodeId,
                 const std::string &remoteNodeId) const;
};

} // namespace nodo::p2p

#endif
