#include "p2p/TcpTransport.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace nodo::p2p;

namespace {

NetworkEnvelope handshakeEnvelope(
    const std::string& from,
    std::int64_t now
) {
    return NetworkEnvelope(
        "nodo-localnet",
        "localnet-chain",
        "1.0.0",
        NetworkMessageType::PEER_HELLO,
        from,
        now,
        30,
        "hello=" + from
    );
}

std::optional<TransportMessage> waitForMessage(
    TcpTransport& transport,
    const std::string& nodeId
) {
    for (int attempt = 0; attempt < 500; ++attempt) {
        std::optional<TransportMessage> message = transport.poll(nodeId);
        if (message.has_value()) {
            return message;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

std::unique_ptr<TcpTransport> connectClient(
    TcpTransport& server,
    const std::string& clientId,
    std::int64_t now
) {
    auto client = std::make_unique<TcpTransport>();
    assert(client->bind(clientId, "127.0.0.1", 0).success());
    client->registerPeerEndpoint("server", server.localEndpoint());
    assert(client->connect(clientId, "server").success());
    assert(client->send(TransportMessage(
        clientId,
        "server",
        handshakeEnvelope(clientId, now),
        now
    )).success());
    return client;
}

std::optional<TransportConnectionId> receiveCandidateHello(
    TcpTransport& server,
    const std::string& expectedPeer
) {
    const auto message = waitForMessage(server, "server");
    if (!message.has_value() ||
        message->fromNodeId() != expectedPeer ||
        !message->hasConnectionId()) {
        return std::nullopt;
    }
    return message->connectionId();
}

TcpCandidatePolicy policyWithSlots(
    const TcpConnectionSlotPolicy& slots,
    std::size_t candidateMaxTotal = 8,
    std::size_t candidateMaxPerIp = 8,
    std::size_t candidateMaxPerSubnet = 8,
    TcpIpRateLimitPolicy rate = TcpIpRateLimitPolicy()
) {
    return TcpCandidatePolicy(
        candidateMaxTotal,
        candidateMaxPerIp,
        candidateMaxPerSubnet,
        std::chrono::milliseconds(2'000),
        rate,
        slots
    );
}

void testPolicyValidation() {
    assert(TcpConnectionSlotPolicy().isValid());
    assert(!TcpConnectionSlotPolicy(0, 1, 1, 1, 1).isValid());
    assert(!TcpConnectionSlotPolicy(2, 3, 1, 1, 1).isValid());
    assert(!TcpConnectionSlotPolicy(2, 1, 3, 1, 1).isValid());
    assert(!TcpConnectionSlotPolicy(2, 1, 1, 3, 1).isValid());
    assert(!TcpConnectionSlotPolicy(2, 1, 1, 1, 3).isValid());
}

void testInboundSlotEvictsOldestInboundPeer() {
    TcpTransport server(policyWithSlots(
        TcpConnectionSlotPolicy(4, 1, 4, 4, 4)
    ));
    assert(server.bind("server", "127.0.0.1", 0).success());

    auto first = connectClient(server, "client-a", 1000);
    const auto firstConnection = receiveCandidateHello(server, "client-a");
    assert(firstConnection.has_value());
    assert(server.authenticateConnection(*firstConnection, "client-a"));
    assert(server.connected("server", "client-a"));
    assert(server.connectedInboundCount() == 1);

    auto second = connectClient(server, "client-b", 1001);
    const auto secondConnection = receiveCandidateHello(server, "client-b");
    assert(secondConnection.has_value());
    assert(server.authenticateConnection(*secondConnection, "client-b"));

    assert(!server.connected("server", "client-a"));
    assert(server.connected("server", "client-b"));
    assert(server.connectedInboundCount() == 1);
    assert(server.evictedConnectionCount() == 1);
}

void testOutboundSlotEvictsOldestOutboundPeer() {
    TcpTransport client(policyWithSlots(
        TcpConnectionSlotPolicy(4, 4, 1, 4, 4)
    ));
    TcpTransport serverA;
    TcpTransport serverB;
    assert(client.bind("client", "127.0.0.1", 0).success());
    assert(serverA.bind("server-a", "127.0.0.1", 0).success());
    assert(serverB.bind("server-b", "127.0.0.1", 0).success());

    client.registerPeerEndpoint("server-a", serverA.localEndpoint());
    client.registerPeerEndpoint("server-b", serverB.localEndpoint());

    assert(client.connect("client", "server-a").success());
    assert(client.connected("client", "server-a"));
    assert(client.connectedOutboundCount() == 1);

    assert(client.connect("client", "server-b").success());
    assert(!client.connected("client", "server-a"));
    assert(client.connected("client", "server-b"));
    assert(client.connectedOutboundCount() == 1);
    assert(client.evictedConnectionCount() == 1);
}

void testActivePerIpLimitRejectsSecondAuthenticatedPeer() {
    TcpTransport server(policyWithSlots(
        TcpConnectionSlotPolicy(4, 4, 4, 1, 4)
    ));
    assert(server.bind("server", "127.0.0.1", 0).success());

    auto first = connectClient(server, "ip-peer-a", 1100);
    const auto firstConnection = receiveCandidateHello(server, "ip-peer-a");
    assert(firstConnection.has_value());
    assert(server.authenticateConnection(*firstConnection, "ip-peer-a"));
    assert(server.connectedCountForIp("127.0.0.1") == 1);

    auto second = connectClient(server, "ip-peer-b", 1101);
    const auto secondConnection = receiveCandidateHello(server, "ip-peer-b");
    assert(secondConnection.has_value());
    assert(!server.authenticateConnection(*secondConnection, "ip-peer-b"));
    assert(server.connected("server", "ip-peer-a"));
    assert(!server.connected("server", "ip-peer-b"));
    assert(server.slotRejectedConnectionCount() > 0);
}

void testCandidateSubnetLimitRejectsExcessPendingConnections() {
    TcpTransport server(policyWithSlots(
        TcpConnectionSlotPolicy(8, 8, 8, 8, 8),
        8,
        8,
        1
    ));
    assert(server.bind("server", "127.0.0.1", 0).success());

    std::vector<std::unique_ptr<TcpTransport>> clients;
    for (int index = 0; index < 2; ++index) {
        auto client = std::make_unique<TcpTransport>();
        const std::string nodeId = "subnet-client-" + std::to_string(index);
        assert(client->bind(nodeId, "127.0.0.1", 0).success());
        client->registerPeerEndpoint("server", server.localEndpoint());
        assert(client->connect(nodeId, "server").success());
        clients.push_back(std::move(client));
    }

    (void)server.poll("server");
    assert(server.pendingCandidateCountForSubnet("127.0.0") == 1);
    assert(server.rateLimitedCandidateCount() == 1);
}

} // namespace

int main() {
    testPolicyValidation();
    testInboundSlotEvictsOldestInboundPeer();
    testOutboundSlotEvictsOldestOutboundPeer();
    testActivePerIpLimitRejectsSecondAuthenticatedPeer();
    testCandidateSubnetLimitRejectsExcessPendingConnections();
    return 0;
}
