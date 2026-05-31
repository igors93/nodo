#include "node/TestnetReadiness.hpp"

#include "config/NetworkParameters.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::config::NetworkParameters;
using nodo::node::BlockAnnouncement;
using nodo::node::BlockSyncPlan;
using nodo::node::BlockSyncRequest;
using nodo::node::NodeHealthReport;
using nodo::node::PeerHandshake;
using nodo::node::PeerHandshakeValidation;
using nodo::node::TestnetReadiness;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

NetworkParameters networkParameters() {
    return NetworkParameters::developmentLocal();
}

PeerHandshake localHandshake(
    std::uint64_t height = 10
) {
    return TestnetReadiness::buildLocalHandshake(
        networkParameters(),
        "local-peer",
        "genesis-testnet-readiness",
        height,
        "local-finalized-hash",
        1900000000
    );
}

PeerHandshake remoteHandshake(
    std::uint64_t height = 10
) {
    return TestnetReadiness::buildLocalHandshake(
        networkParameters(),
        "remote-peer",
        "genesis-testnet-readiness",
        height,
        "remote-finalized-hash",
        1900000001
    );
}

void testAcceptsCompatibleHandshake() {
    const PeerHandshake local =
        localHandshake();

    const PeerHandshake remote =
        remoteHandshake();

    const PeerHandshakeValidation validation =
        TestnetReadiness::validateHandshake(
            local,
            remote
        );

    requireCondition(
        validation.accepted() &&
        validation.localPeerId() == "local-peer" &&
        validation.remotePeerId() == "remote-peer",
        "Compatible peers should pass the testnet handshake."
    );
}

void testRejectsMismatchedChain() {
    const PeerHandshake local =
        localHandshake();

    const NetworkParameters incompatibleNetwork(
        "other-chain",
        networkParameters().networkName(),
        networkParameters().protocolVersion(),
        networkParameters().epochDurationSeconds(),
        networkParameters().minimumValidatorCount(),
        networkParameters().quorumThresholdNumerator(),
        networkParameters().quorumThresholdDenominator(),
        networkParameters().maxTransactionsPerBlock(),
        networkParameters().maxPeerCount(),
        networkParameters().maxMempoolTransactions(),
        networkParameters().minimumFeeRawUnits(),
        networkParameters().targetBlockTimeSeconds(),
        networkParameters().finalityDepth(),
        networkParameters().signatureAlgorithm(),
        networkParameters().storageFormatVersion()
    );

    const PeerHandshake remote =
        TestnetReadiness::buildLocalHandshake(
            incompatibleNetwork,
            "remote-peer",
            "genesis-testnet-readiness",
            10,
            "remote-finalized-hash",
            1900000001
        );

    const PeerHandshakeValidation validation =
        TestnetReadiness::validateHandshake(
            local,
            remote
        );

    requireCondition(
        !validation.accepted() &&
        validation.status() == "REJECTED",
        "Peers from a different chain id must be rejected."
    );
}

void testBuildsBoundedSyncPlanAndRequest() {
    const PeerHandshake local =
        localHandshake(10);

    const PeerHandshake remote =
        remoteHandshake(700);

    const PeerHandshakeValidation validation =
        TestnetReadiness::validateHandshake(
            local,
            remote
        );

    const BlockSyncPlan plan =
        TestnetReadiness::buildSyncPlan(
            validation,
            local,
            remote
        );

    requireCondition(
        plan.needsSync() &&
        plan.nextHeight() == 11 &&
        plan.targetHeight() == 522 &&
        plan.maxWindow() == nodo::node::NODO_SYNC_MAX_BLOCK_WINDOW,
        "Sync plan should request a bounded block window."
    );

    const BlockSyncRequest request =
        TestnetReadiness::buildSyncRequest(
            validation,
            plan
        );

    requireCondition(
        request.isValid() &&
        request.fromHeight() == 11 &&
        request.toHeight() == 522 &&
        request.requestedBlockCount() == nodo::node::NODO_SYNC_MAX_BLOCK_WINDOW,
        "Sync request should mirror the bounded sync plan."
    );
}

void testBuildsBlockAnnouncement() {
    const BlockAnnouncement announcement =
        TestnetReadiness::buildBlockAnnouncement(
            networkParameters(),
            "local-peer",
            11,
            "block-hash-11",
            "block-hash-10"
        );

    requireCondition(
        announcement.isValid() &&
        announcement.blockHeight() == 11 &&
        announcement.chainId() == networkParameters().chainId(),
        "Block announcement should commit to chain id and block hashes."
    );
}

void testBuildsHealthReport() {
    const PeerHandshake local =
        localHandshake(10);

    const PeerHandshake remote =
        remoteHandshake(10);

    const PeerHandshakeValidation validation =
        TestnetReadiness::validateHandshake(
            local,
            remote
        );

    const BlockSyncPlan plan =
        TestnetReadiness::buildSyncPlan(
            validation,
            local,
            remote
        );

    const NodeHealthReport report =
        TestnetReadiness::buildHealthReport(
            local,
            {validation},
            plan,
            true
        );

    requireCondition(
        report.ready() &&
        report.connectedPeerCount() == 1 &&
        report.handshakeReady() &&
        report.syncReady() &&
        report.blockGossipReady() &&
        report.transactionGossipReady(),
        "Node should be testnet-ready when handshake, sync and gossip checks pass."
    );
}

void testHealthReportStaysNotReadyWhenSyncIsRequired() {
    const PeerHandshake local =
        localHandshake(10);

    const PeerHandshake remote =
        remoteHandshake(11);

    const PeerHandshakeValidation validation =
        TestnetReadiness::validateHandshake(
            local,
            remote
        );

    const BlockSyncPlan plan =
        TestnetReadiness::buildSyncPlan(
            validation,
            local,
            remote
        );

    const NodeHealthReport report =
        TestnetReadiness::buildHealthReport(
            local,
            {validation},
            plan,
            true
        );

    requireCondition(
        !report.ready() &&
        report.status() == "NOT_READY" &&
        !report.syncReady(),
        "Node should not be ready while it still needs block sync."
    );
}

} // namespace

int main() {
    try {
        testAcceptsCompatibleHandshake();
        testRejectsMismatchedChain();
        testBuildsBoundedSyncPlanAndRequest();
        testBuildsBlockAnnouncement();
        testBuildsHealthReport();
        testHealthReportStaysNotReadyWhenSyncIsRequired();

        std::cout << "Nodo testnet readiness tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo testnet readiness tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
