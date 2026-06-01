#include "p2p/TcpTransport.hpp"

#include "p2p/TcpTransportFrameCodec.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace nodo::p2p {

namespace {

constexpr int INVALID_FD = -1;
constexpr int LISTEN_BACKLOG = 16;
constexpr std::uint32_t MAX_WIRE_FRAME_BYTES =
    static_cast<std::uint32_t>(TcpTransportFrameCodec::MAX_TCP_FRAME_BYTES);

bool setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void closeSocket(int& fd) {
    if (fd != INVALID_FD) {
        ::close(fd);
        fd = INVALID_FD;
    }
}

std::uint32_t decodeU32BigEndian(const unsigned char* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

std::array<unsigned char, 4> encodeU32BigEndian(std::uint32_t value) {
    return {
        static_cast<unsigned char>((value >> 24) & 0xFF),
        static_cast<unsigned char>((value >> 16) & 0xFF),
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>(value & 0xFF)
    };
}

bool writeAll(int fd, const unsigned char* data, std::size_t size) {
    std::size_t written = 0;

    while (written < size) {
        const ssize_t result = ::send(
            fd,
            data + written,
            size - written,
            MSG_NOSIGNAL
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        if (result == 0) {
            return false;
        }

        written += static_cast<std::size_t>(result);
    }

    return true;
}

bool readAll(int fd, unsigned char* data, std::size_t size) {
    std::size_t read = 0;

    while (read < size) {
        const ssize_t result = ::recv(
            fd,
            data + read,
            size - read,
            0
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        if (result == 0) {
            return false;
        }

        read += static_cast<std::size_t>(result);
    }

    return true;
}

bool socketHasReadableData(int fd) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    const int result = select(
        fd + 1,
        &readSet,
        nullptr,
        nullptr,
        &timeout
    );

    return result > 0 && FD_ISSET(fd, &readSet);
}

std::string errnoMessage(const std::string& prefix) {
    std::ostringstream output;
    output << prefix << ": " << std::strerror(errno);
    return output.str();
}

} // namespace

TcpTransport::TcpTransport()
    : m_listenFd(INVALID_FD),
      m_localNodeId(),
      m_localEndpoint(),
      m_peerEndpoints(),
      m_connectionsByPeer(),
      m_unidentifiedInboundFds() {}

TcpTransport::~TcpTransport() {
    closeAll();
}

TransportResult TcpTransport::bind(
    const std::string& localNodeId,
    const PeerEndpoint& endpoint
) {
    if (!isSafeNodeId(localNodeId)) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Local node id is invalid."
        );
    }

    if (!isSafeHost(endpoint.host())) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Local TCP endpoint is invalid."
        );
    }

    if (m_listenFd != INVALID_FD) {
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP transport is already bound."
        );
    }

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_FD) {
        return TransportResult(
            TransportStatus::REJECTED,
            errnoMessage("Unable to create TCP socket")
        );
    }

    int reuse = 1;
    (void)::setsockopt(
        fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    );

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port());

    if (::inet_pton(AF_INET, endpoint.host().c_str(), &address.sin_addr) != 1) {
        int closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP endpoint host must be an IPv4 address for this runtime phase."
        );
    }

    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        int closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            errnoMessage("Unable to bind TCP socket")
        );
    }

    if (::listen(fd, LISTEN_BACKLOG) != 0) {
        int closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            errnoMessage("Unable to listen on TCP socket")
        );
    }

    if (!setNonBlocking(fd)) {
        int closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            errnoMessage("Unable to set TCP socket nonblocking")
        );
    }

    sockaddr_in boundAddress;
    std::memset(&boundAddress, 0, sizeof(boundAddress));
    socklen_t boundLength = sizeof(boundAddress);
    if (::getsockname(
            fd,
            reinterpret_cast<sockaddr*>(&boundAddress),
            &boundLength
        ) != 0) {
        int closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            errnoMessage("Unable to read bound TCP endpoint")
        );
    }

    m_listenFd = fd;
    m_localNodeId = localNodeId;
    m_localEndpoint = PeerEndpoint(
        endpoint.host(),
        ntohs(boundAddress.sin_port)
    );

    return TransportResult(
        TransportStatus::SENT,
        "TCP transport bound and listening."
    );
}

TransportResult TcpTransport::bind(
    const std::string& localNodeId,
    const std::string& host,
    std::uint16_t port
) {
    return bind(localNodeId, PeerEndpoint(host, port));
}

bool TcpTransport::listening() const {
    return m_listenFd != INVALID_FD;
}

const std::string& TcpTransport::localNodeId() const {
    return m_localNodeId;
}

const PeerEndpoint& TcpTransport::localEndpoint() const {
    return m_localEndpoint;
}

std::uint16_t TcpTransport::localPort() const {
    return m_localEndpoint.port();
}

void TcpTransport::registerPeerEndpoint(
    const std::string& remoteNodeId,
    const PeerEndpoint& endpoint
) {
    if (isSafeNodeId(remoteNodeId) && endpoint.isValid()) {
        m_peerEndpoints[remoteNodeId] = endpoint;
    }
}

bool TcpTransport::hasPeerEndpoint(
    const std::string& remoteNodeId
) const {
    return m_peerEndpoints.find(remoteNodeId) != m_peerEndpoints.end();
}

std::vector<std::string> TcpTransport::connectedPeers() const {
    std::vector<std::string> peers;
    peers.reserve(m_connectionsByPeer.size());

    for (const auto& [peer, ignored] : m_connectionsByPeer) {
        (void)ignored;
        peers.push_back(peer);
    }

    return peers;
}

TransportResult TcpTransport::connect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    if (localNodeId != m_localNodeId || !listening()) {
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP transport must be bound before connecting."
        );
    }

    if (!isSafeNodeId(remoteNodeId) || remoteNodeId == localNodeId) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Remote node id is invalid."
        );
    }

    if (connected(localNodeId, remoteNodeId)) {
        return TransportResult(
            TransportStatus::SENT,
            "TCP peer is already connected."
        );
    }

    const auto endpoint = m_peerEndpoints.find(remoteNodeId);
    if (endpoint == m_peerEndpoints.end()) {
        return TransportResult(
            TransportStatus::NOT_CONNECTED,
            "No TCP endpoint registered for remote peer."
        );
    }

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_FD) {
        return TransportResult(
            TransportStatus::REJECTED,
            errnoMessage("Unable to create outbound TCP socket")
        );
    }

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint->second.port());

    if (::inet_pton(AF_INET, endpoint->second.host().c_str(), &address.sin_addr) != 1) {
        int closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            "Remote TCP endpoint host must be an IPv4 address."
        );
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        int closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::NOT_CONNECTED,
            errnoMessage("Unable to connect TCP peer")
        );
    }

    (void)setNonBlocking(fd);
    rememberConnection(remoteNodeId, fd);

    return TransportResult(
        TransportStatus::SENT,
        "TCP peer connected."
    );
}

TransportResult TcpTransport::disconnect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    if (localNodeId != m_localNodeId) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Local node id does not match bound TCP transport."
        );
    }

    closePeerConnection(remoteNodeId);

    return TransportResult(
        TransportStatus::SENT,
        "TCP peer disconnected."
    );
}

bool TcpTransport::connected(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    (void)localNodeId;
    return m_connectionsByPeer.find(remoteNodeId) != m_connectionsByPeer.end();
}

TransportResult TcpTransport::send(
    const TransportMessage& message
) {
    if (!message.isValid()) {
        return TransportResult(
            TransportStatus::INVALID_MESSAGE,
            "TCP transport message is invalid."
        );
    }

    if (message.fromNodeId() != m_localNodeId) {
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP transport refuses to send from a different local node id."
        );
    }

    if (!connected(m_localNodeId, message.toNodeId())) {
        const TransportResult connectionResult =
            connect(m_localNodeId, message.toNodeId());

        if (!connectionResult.success()) {
            return connectionResult;
        }
    }

    const auto found = m_connectionsByPeer.find(message.toNodeId());
    if (found == m_connectionsByPeer.end()) {
        return TransportResult(
            TransportStatus::NOT_CONNECTED,
            "TCP peer is not connected."
        );
    }

    const std::vector<unsigned char> frame =
        TcpTransportFrameCodec::encodeTransportMessage(message);

    if (frame.empty() || frame.size() > MAX_WIRE_FRAME_BYTES) {
        return TransportResult(
            TransportStatus::INVALID_MESSAGE,
            "TCP frame size is invalid."
        );
    }

    const auto lengthPrefix =
        encodeU32BigEndian(static_cast<std::uint32_t>(frame.size()));

    if (!writeAll(found->second, lengthPrefix.data(), lengthPrefix.size()) ||
        !writeAll(found->second, frame.data(), frame.size())) {
        closePeerConnection(message.toNodeId());
        return TransportResult(
            TransportStatus::NOT_CONNECTED,
            "Unable to write full TCP frame."
        );
    }

    return TransportResult(
        TransportStatus::SENT,
        "TCP frame sent."
    );
}

std::optional<TransportMessage> TcpTransport::poll(
    const std::string& localNodeId
) {
    if (localNodeId != m_localNodeId || !listening()) {
        return std::nullopt;
    }

    (void)acceptAvailableConnections();

    for (auto iterator = m_connectionsByPeer.begin();
         iterator != m_connectionsByPeer.end();) {
        const int fd = iterator->second;
        auto message = pollFd(fd, false);

        if (message.has_value()) {
            return message;
        }

        ++iterator;
    }

    for (auto iterator = m_unidentifiedInboundFds.begin();
         iterator != m_unidentifiedInboundFds.end();) {
        const int fd = *iterator;
        auto message = pollFd(fd, true);

        if (message.has_value()) {
            rememberConnection(message->fromNodeId(), fd);
            iterator = m_unidentifiedInboundFds.erase(iterator);
            return message;
        }

        ++iterator;
    }

    return std::nullopt;
}

void TcpTransport::closeAll() {
    for (auto& [peer, fd] : m_connectionsByPeer) {
        (void)peer;
        closeSocket(fd);
    }
    m_connectionsByPeer.clear();

    for (int& fd : m_unidentifiedInboundFds) {
        closeSocket(fd);
    }
    m_unidentifiedInboundFds.clear();

    closeSocket(m_listenFd);
}

TransportResult TcpTransport::acceptAvailableConnections() {
    if (m_listenFd == INVALID_FD) {
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP listener is not bound."
        );
    }

    while (true) {
        sockaddr_in remoteAddress;
        std::memset(&remoteAddress, 0, sizeof(remoteAddress));
        socklen_t remoteLength = sizeof(remoteAddress);

        const int fd = ::accept(
            m_listenFd,
            reinterpret_cast<sockaddr*>(&remoteAddress),
            &remoteLength
        );

        if (fd == INVALID_FD) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            return TransportResult(
                TransportStatus::REJECTED,
                errnoMessage("Unable to accept TCP peer")
            );
        }

        (void)setNonBlocking(fd);
        m_unidentifiedInboundFds.push_back(fd);
    }

    return TransportResult(
        TransportStatus::SENT,
        "TCP accept cycle completed."
    );
}

std::optional<TransportMessage> TcpTransport::pollFd(
    int fd,
    bool unidentified
) {
    (void)unidentified;

    if (fd == INVALID_FD || !socketHasReadableData(fd)) {
        return std::nullopt;
    }

    int availableBytes = 0;
    if (::ioctl(fd, FIONREAD, &availableBytes) != 0 || availableBytes < 4) {
        return std::nullopt;
    }

    unsigned char header[4] = {0, 0, 0, 0};
    const ssize_t peeked = ::recv(fd, header, sizeof(header), MSG_PEEK);
    if (peeked <= 0) {
        return std::nullopt;
    }

    if (peeked < static_cast<ssize_t>(sizeof(header))) {
        return std::nullopt;
    }

    const std::uint32_t frameSize = decodeU32BigEndian(header);
    if (frameSize == 0 || frameSize > MAX_WIRE_FRAME_BYTES) {
        closeFd(fd);
        return std::nullopt;
    }

    if (availableBytes < static_cast<int>(sizeof(header) + frameSize)) {
        return std::nullopt;
    }

    unsigned char consumedHeader[4] = {0, 0, 0, 0};
    if (!readAll(fd, consumedHeader, sizeof(consumedHeader))) {
        closeFd(fd);
        return std::nullopt;
    }

    std::vector<unsigned char> frame(frameSize);
    if (!readAll(fd, frame.data(), frame.size())) {
        closeFd(fd);
        return std::nullopt;
    }

    try {
        TransportMessage message =
            TcpTransportFrameCodec::decodeTransportMessage(frame);

        if (message.toNodeId() != m_localNodeId) {
            return std::nullopt;
        }

        return message;
    } catch (...) {
        closeFd(fd);
        return std::nullopt;
    }
}

void TcpTransport::rememberConnection(
    const std::string& remoteNodeId,
    int fd
) {
    if (!isSafeNodeId(remoteNodeId) || fd == INVALID_FD) {
        return;
    }

    const auto existing = m_connectionsByPeer.find(remoteNodeId);
    if (existing != m_connectionsByPeer.end() && existing->second != fd) {
        int oldFd = existing->second;
        closeSocket(oldFd);
    }

    m_connectionsByPeer[remoteNodeId] = fd;
}

void TcpTransport::closeFd(int fd) {
    if (fd == INVALID_FD) {
        return;
    }

    for (auto iterator = m_connectionsByPeer.begin();
         iterator != m_connectionsByPeer.end();) {
        if (iterator->second == fd) {
            int socketFd = iterator->second;
            closeSocket(socketFd);
            iterator = m_connectionsByPeer.erase(iterator);
        } else {
            ++iterator;
        }
    }

    for (auto iterator = m_unidentifiedInboundFds.begin();
         iterator != m_unidentifiedInboundFds.end();) {
        if (*iterator == fd) {
            int socketFd = *iterator;
            closeSocket(socketFd);
            iterator = m_unidentifiedInboundFds.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void TcpTransport::closePeerConnection(
    const std::string& remoteNodeId
) {
    const auto found = m_connectionsByPeer.find(remoteNodeId);
    if (found == m_connectionsByPeer.end()) {
        return;
    }

    int fd = found->second;
    closeSocket(fd);
    m_connectionsByPeer.erase(found);
}

bool TcpTransport::isSafeNodeId(const std::string& nodeId) {
    if (nodeId.empty() || nodeId.size() > 160) {
        return false;
    }

    for (const char character : nodeId) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

bool TcpTransport::isSafeHost(const std::string& host) {
    return !host.empty() && host.size() <= 255;
}

} // namespace nodo::p2p
