#include "config/GenesisRegistry.hpp"
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
        config::GenesisRegistry::get("localnet").genesis();

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
        validLoad.manifest().latestStateRoot(),
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

void testRejectsManifestStateRootMismatch() {
    const node::RuntimeStateLoadResult validLoad =
        loadedRuntime();

    const node::NodeRuntimeManifest badManifest(
        validLoad.manifest().chainId(),
        validLoad.manifest().networkName(),
        validLoad.manifest().protocolVersion(),
        validLoad.manifest().genesisConfigId(),
        validLoad.manifest().latestBlockHeight(),
        validLoad.manifest().latestBlockHash(),
        "1111111111111111111111111111111111111111111111111111111111111111",
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
        result.reason().find("latestStateRoot") != std::string::npos,
        "Chain auditor should reject manifest/runtime state-root mismatch."
    );
}

// Regression guard: a clean genesis runtime must always produce a stable
// artifact digest. If this test fails it means the artifact digest
// computation regressed to non-deterministic or empty output.
void testArtifactDigestIsStableOnCleanRuntime() {
    const node::RuntimeStateLoadResult load = loadedRuntime();

    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntime(load);

    requireCondition(
        result.passed(),
        "Clean genesis runtime must pass chain audit (artifact digest regression guard)."
    );
}

// Verifies that a loader failure propagates intact through the chain auditor.
// Ensures neither the auditor nor the loader silently swallows the reason.
void testLoaderRejectionPreservesReason() {
    const std::string expectedReason = "storage-corruption-sentinel";
    const node::RuntimeStateLoadResult rejected =
        node::RuntimeStateLoadResult::rejected(
            node::RuntimeStateLoadStatus::BLOCK_FILE_INVALID,
            expectedReason
        );

    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntime(rejected);

    requireCondition(
        !result.passed(),
        "Chain auditor must propagate loader rejection as audit failure."
    );
    requireCondition(
        result.reason().find(expectedReason) != std::string::npos,
        "Chain auditor must preserve the original loader rejection reason."
    );
}

// Verifies that the dev-mode path skips the monetary report requirement
// without crashing and still produces a passed result on a clean genesis.
void testDevModeAuditPassesOnCleanRuntime() {
    const node::ChainAuditResult result =
        node::ChainAuditor::auditLoadedRuntimeDevMode(loadedRuntime());

    requireCondition(
        result.passed(),
        "Dev-mode audit must pass on a clean genesis runtime."
    );
}

} // namespace

int main() {
    try {
        testAuditsLoadedRuntime();
        testReportsLoaderFailure();
        testRejectsManifestRuntimeHeightMismatch();
        testRejectsManifestStateRootMismatch();
        testArtifactDigestIsStableOnCleanRuntime();
        testLoaderRejectionPreservesReason();
        testDevModeAuditPassesOnCleanRuntime();

        std::cout << "Nodo chain auditor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo chain auditor tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
