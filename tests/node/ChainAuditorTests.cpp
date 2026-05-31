#include "app/CommandLineInterface.hpp"
#include "node/ChainAuditor.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "p2p/PeerMessage.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

p2p::PeerInfo localPeer() {
    return p2p::PeerInfo(
        "chain-auditor-test-peer",
        "127.0.0.1:9000",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

node::RuntimeStateLoadResult loadedRuntime() {
    const config::GenesisConfig genesis =
        app::CommandLineInterface::developmentGenesisConfig();

    const node::NodeRuntimeStartResult start =
        node::NodeRuntimeFactory::startFromGenesis(
            node::NodeRuntimeConfig(
                genesis,
                localPeer(),
                genesis.networkParameters().maxPeerCount()
            )
        );

    if (!start.started()) {
        throw std::runtime_error(start.reason());
    }

    const node::NodeRuntime runtime =
        start.runtime();

    return node::RuntimeStateLoadResult::loaded(
        runtime,
        node::NodeRuntimeManifest::fromRuntime(
            runtime,
            kTimestamp,
            kTimestamp
        ),
        0,
        0
    );
}

void testAuditsLoadedRuntime() {
    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntime(
            loadedRuntime()
        );

    requireCondition(
        result.passed(),
        "Loaded valid runtime should pass chain audit."
    );
}

void testReportsLoaderFailure() {
    const node::RuntimeStateLoadResult load =
        node::RuntimeStateLoadResult::rejected(
            node::RuntimeStateLoadStatus::MANIFEST_MISMATCH,
            "manifest mismatch"
        );

    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntime(load);

    requireCondition(
        !result.passed() &&
        result.reason().find("manifest mismatch") != std::string::npos,
        "Chain auditor should report loader failures clearly."
    );
}

void testRejectsManifestRuntimeHeightMismatch() {
    const node::RuntimeStateLoadResult validLoad =
        loadedRuntime();

    const node::NodeRuntimeManifest badManifest(
        validLoad.manifest().chainId(),
        validLoad.manifest().networkName(),
        validLoad.manifest().protocolVersion(),
        validLoad.manifest().genesisConfigId(),
        validLoad.manifest().latestBlockHeight() + 1,
        validLoad.manifest().latestBlockHash(),
        validLoad.manifest().validatorCount(),
        validLoad.manifest().peerCount(),
        validLoad.manifest().createdAt(),
        validLoad.manifest().updatedAt()
    );

    const node::RuntimeStateLoadResult badLoad =
        node::RuntimeStateLoadResult::loaded(
            validLoad.runtime(),
            badManifest,
            validLoad.loadedBlockCount(),
            validLoad.loadedMempoolTransactionCount()
        );

    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntime(badLoad);

    requireCondition(
        !result.passed() &&
        result.reason().find("latestBlockHeight") != std::string::npos,
        "Chain auditor should reject manifest/runtime height mismatch."
    );
}

} // namespace

int main() {
    try {
        testAuditsLoadedRuntime();
        testReportsLoaderFailure();
        testRejectsManifestRuntimeHeightMismatch();

        std::cout << "Nodo chain auditor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo chain auditor tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
