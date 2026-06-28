#include "p2p/TcpTransport.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace nodo::p2p;

int main() {
    assert(!TcpCandidatePolicy(
        0, 1, std::chrono::milliseconds(10)).isValid());
    assert(!TcpCandidatePolicy(
        2, 3, std::chrono::milliseconds(10)).isValid());
    assert(!TcpCandidatePolicy(
        2, 1, std::chrono::milliseconds(0)).isValid());

    bool invalidPolicyRejected = false;
    try {
        TcpTransport invalid(TcpCandidatePolicy(
            0, 0, std::chrono::milliseconds(0)));
    } catch (const std::invalid_argument&) {
        invalidPolicyRejected = true;
    }
    assert(invalidPolicyRejected);

    TcpTransport server(TcpCandidatePolicy(
        4,
        2,
        std::chrono::milliseconds(30)
    ));
    assert(server.bind("server", "127.0.0.1", 0).success());

    std::vector<std::unique_ptr<TcpTransport>> silentClients;
    for (int index = 0; index < 3; ++index) {
        auto client = std::make_unique<TcpTransport>();
        const std::string nodeId = "silent-" + std::to_string(index);
        assert(client->bind(nodeId, "127.0.0.1", 0).success());
        client->registerPeerEndpoint("server", server.localEndpoint());
        assert(client->connect(nodeId, "server").success());
        silentClients.push_back(std::move(client));
    }

    (void)server.poll("server");
    assert(server.pendingCandidateCount() == 2);
    assert(server.pendingCandidateCountForIp("127.0.0.1") == 2);
    assert(server.rateLimitedCandidateCount() == 1);
    assert(server.expiredCandidateCount() == 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    (void)server.poll("server");
    assert(server.pendingCandidateCount() == 0);
    assert(server.pendingCandidateCountForIp("127.0.0.1") == 0);
    assert(server.expiredCandidateCount() == 2);

    TcpTransport recoveredClient;
    assert(recoveredClient.bind(
        "recovered-client", "127.0.0.1", 0).success());
    recoveredClient.registerPeerEndpoint("server", server.localEndpoint());
    assert(recoveredClient.connect(
        "recovered-client", "server").success());
    (void)server.poll("server");
    assert(server.pendingCandidateCount() == 1);
    assert(server.pendingCandidateCountForIp("127.0.0.1") == 1);

    return 0;
}
