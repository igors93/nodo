#ifndef NODO_P2P_AUTHENTICATED_SESSION_TRANSPORT_HPP
#define NODO_P2P_AUTHENTICATED_SESSION_TRANSPORT_HPP

#include <cstdint>
#include <string>

namespace nodo::p2p {

class AuthenticatedSessionTransport {
public:
  virtual ~AuthenticatedSessionTransport() = default;

  virtual bool stageOutboundSession(const std::string &localNodeId,
                                    const std::string &remoteNodeId,
                                    const std::string &sharedSecret,
                                    std::int64_t now) = 0;

  virtual bool activateOutboundSession(const std::string &localNodeId,
                                       const std::string &remoteNodeId) = 0;

  virtual bool establishInboundSession(const std::string &localNodeId,
                                       const std::string &remoteNodeId,
                                       const std::string &sharedSecret,
                                       std::int64_t now) = 0;

  virtual bool removeSession(const std::string &localNodeId,
                             const std::string &remoteNodeId) = 0;

  virtual bool rejectPendingConnection(const std::string &localNodeId,
                                       const std::string &remoteNodeId) = 0;

  virtual bool hasOutboundSession(const std::string &localNodeId,
                                  const std::string &remoteNodeId) const = 0;

  virtual bool hasInboundSession(const std::string &localNodeId,
                                 const std::string &remoteNodeId) const = 0;
};

} // namespace nodo::p2p

#endif
