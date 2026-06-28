#ifndef NODO_P2P_AUTHENTICATED_CONNECTION_TRANSPORT_HPP
#define NODO_P2P_AUTHENTICATED_CONNECTION_TRANSPORT_HPP

#include <cstdint>
#include <string>

namespace nodo::p2p {

using TransportConnectionId = std::uint64_t;

class AuthenticatedConnectionTransport {
public:
    virtual ~AuthenticatedConnectionTransport() = default;

    virtual bool authenticateConnection(
        TransportConnectionId connectionId,
        const std::string& remoteNodeId
    ) = 0;

    virtual bool rejectConnection(
        TransportConnectionId connectionId
    ) = 0;

    virtual bool isConnectionAuthenticated(
        TransportConnectionId connectionId,
        const std::string& remoteNodeId
    ) const = 0;
};

} // namespace nodo::p2p

#endif
