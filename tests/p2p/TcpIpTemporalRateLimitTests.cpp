#include "p2p/TcpTransport.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {

using namespace nodo::p2p;

std::unique_ptr<TcpTransport> connectClient(
    TcpTransport& server,
    int index
) {
    auto client = std::make_unique<TcpTransport>();
    const std::string nodeId = "temporal-client-" + std::to_string(index);
    assert(client->bind(nodeId, "127.0.0.1", 0).success());
    client->registerPeerEndpoint("server", server.localEndpoint());
    assert(client->connect(nodeId, "server").success());
    (void)server.poll("server");
    return client;
}

} // namespace

int main() {
    assert(!TcpIpRateLimitPolicy(
        0,
        1,
        std::chrono::milliseconds(100),
        std::chrono::milliseconds(10),
        std::chrono::milliseconds(20)
    ).isValid());
    assert(!TcpIpRateLimitPolicy(
        2,
        1,
        std::chrono::milliseconds(100),
        std::chrono::milliseconds(30),
        std::chrono::milliseconds(20)
    ).isValid());

    const TcpIpRateLimitPolicy temporalPolicy(
        1,
        1,
        std::chrono::milliseconds(500),
        std::chrono::milliseconds(30),
        std::chrono::milliseconds(80)
    );
    TcpTransport server(TcpCandidatePolicy(
        8,
        8,
        std::chrono::milliseconds(2'000),
        temporalPolicy
    ));
    assert(server.bind("server", "127.0.0.1", 0).success());

    std::vector<std::unique_ptr<TcpTransport>> clients;
    clients.push_back(connectClient(server, 0));
    assert(server.pendingCandidateCount() == 1);
    assert(server.temporalRateLimitedConnectionCount() == 0);

    clients.push_back(connectClient(server, 1));
    assert(server.temporalRateLimitedConnectionCount() == 1);
    assert(server.ipBackedOff("127.0.0.1"));
    const auto firstBackoff =
        server.ipBackoffRemaining("127.0.0.1");
    assert(firstBackoff.count() > 0);
    assert(firstBackoff.count() <= 30);

    clients.push_back(connectClient(server, 2));
    assert(server.temporalRateLimitedConnectionCount() == 2);
    assert(server.ipBackoffRemaining("127.0.0.1") <= firstBackoff);

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    clients.push_back(connectClient(server, 3));
    assert(server.temporalRateLimitedConnectionCount() == 3);
    assert(server.ipBackoffRemaining("127.0.0.1").count() > 30);

    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    clients.push_back(connectClient(server, 4));
    assert(server.temporalRateLimitedConnectionCount() == 4);
    assert(server.ipBackoffRemaining("127.0.0.1").count() > 60);

    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    clients.push_back(connectClient(server, 5));
    assert(server.temporalRateLimitedConnectionCount() == 5);
    assert(server.ipBackoffRemaining("127.0.0.1").count() > 60);

    std::this_thread::sleep_for(std::chrono::milliseconds(320));
    clients.push_back(connectClient(server, 6));
    assert(!server.ipBackedOff("127.0.0.1"));
    assert(server.pendingCandidateCount() == 2);
    assert(server.ipAdmissionStateCount() == 1);

    return 0;
}
