#include "config/GenesisRegistry.hpp"
#include "economics/BurnRecord.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"
#include "node/ChainAuditor.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeMonetaryReportService.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;
using nodo::utils::Amount;
using nodo::economics::BurnRecord;
using nodo::economics::BurnType;
using nodo::economics::MonetaryPolicy;
using nodo::economics::SupplyDelta;

constexpr std::int64_t kTimestamp = 1900000000;

p2p::PeerInfo localPeer() {
    return p2p::PeerInfo(
        "chain-auditor-report-test-peer",
        "127.0.0.1:9001",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

node::RuntimeStateLoadResult genesisLoad() {
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
        throw std::runtime_error(
            "genesisLoad: failed to start runtime: " + start.reason()
        );
    }

    const node::NodeRuntime rt = start.runtime();
    return node::RuntimeStateLoadResult::loaded(
        rt,
        node::NodeRuntimeManifest::fromRuntime(rt, kTimestamp, kTimestamp),
        0, 0
    );
}

// Build a RuntimeStateLoadResult with one finalized supply delta injected.
node::RuntimeStateLoadResult genesisLoadWithOneDelta() {
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
        throw std::runtime_error(
            "genesisLoadWithOneDelta: failed to start runtime: " + start.reason()
        );
    }

    node::NodeRuntime rt = start.runtime();

    // Derive genesis supply the same way ChainAuditor does.
    const Amount genesisSupply =
        node::MonetaryFirewall::genesisSupply(genesis);

    // Construct a minimal valid delta at block height 1.
    // supplyBefore must equal genesisSupply to pass SupplyAudit.
    const std::int64_t burnAmount = 100;
    const BurnRecord burn(
        "burn-audit-test", 1, 0, "fee_pool",
        Amount::fromRawUnits(burnAmount),
        "fee burn", BurnType::FEE_BURN
    );
    const SupplyDelta delta(
        1, "test-block-hash-for-audit", 0,
        genesisSupply,
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(burnAmount),
        Amount::fromRawUnits(genesisSupply.rawUnits() - burnAmount),
        {}, {burn}
    );

    // applyFinalizedDelta requires delta.supplyBefore() == latestSupply().
    // After startFromGenesis, latestSupply() == genesisSupply.
    rt.mutableSupplyState().applyFinalizedDelta(delta);

    // Manifest is re-derived from the modified runtime (blockchain still shows
    // genesis height, but supply state now has 1 finalized delta).
    const node::NodeRuntimeManifest manifest =
        node::NodeRuntimeManifest::fromRuntime(rt, kTimestamp, kTimestamp);

    return node::RuntimeStateLoadResult::loaded(rt, manifest, 0, 1);
}

std::filesystem::path tempReportPath() {
    return std::filesystem::temp_directory_path() /
           "nodo_chain_auditor_report_test.txt";
}

// auditLoadedRuntime on genesis runtime (no deltas) passes without a report path.
// Proof-of-Protection: no report required when no finalized state exists.
void testGenesisRuntimePassesWithoutReportPath() {
    const auto result = node::ChainAuditor::auditLoadedRuntime(genesisLoad());
    assert(result.passed());
}

// auditLoadedRuntimeDevMode passes for genesis runtime.
// The name makes the intentional skip of report verification explicit.
void testDevModePassesForGenesisRuntime() {
    const auto result = node::ChainAuditor::auditLoadedRuntimeDevMode(genesisLoad());
    assert(result.passed());
}

// Normal audit fails when finalized deltas exist and report path is missing.
// Proof-of-Protection: no state accepted without verifiable monetary evidence.
void testNormalAuditFailsMissingReportWhenDeltasExist() {
    const auto load = genesisLoadWithOneDelta();
    // No report path: audit must fail.
    const auto result = node::ChainAuditor::auditLoadedRuntime(load);
    assert(!result.passed());
    assert(result.reason().find("report") != std::string::npos);
}

// Normal audit passes when report exists and matches rebuilt report.
void testNormalAuditPassesWithMatchingReport() {
    const auto load = genesisLoadWithOneDelta();
    const auto& finalizedDeltas = load.runtime().supplyState().finalizedDeltas();
    assert(finalizedDeltas.size() == 1);

    const config::GenesisConfig genesis =
        config::GenesisRegistry::get("localnet").genesis();
    const Amount genesisSupply = node::MonetaryFirewall::genesisSupply(genesis);
    const MonetaryPolicy policy = MonetaryPolicy::localnetDefault(
        genesis.networkParameters().chainId(), genesisSupply
    );

    const auto path = tempReportPath();
    const auto svcResult = node::RuntimeMonetaryReportService::buildAndPersist(
        policy, finalizedDeltas, 0, path
    );
    assert(svcResult.succeeded());

    const auto auditResult = node::ChainAuditor::auditLoadedRuntime(load, path);
    assert(auditResult.passed());

    std::filesystem::remove(path);
}

// Normal audit fails when persisted report is tampered.
void testNormalAuditFailsTamperedReport() {
    const auto load = genesisLoadWithOneDelta();
    const auto& finalizedDeltas = load.runtime().supplyState().finalizedDeltas();

    const config::GenesisConfig genesis =
        config::GenesisRegistry::get("localnet").genesis();
    const Amount genesisSupply = node::MonetaryFirewall::genesisSupply(genesis);
    const MonetaryPolicy policy = MonetaryPolicy::localnetDefault(
        genesis.networkParameters().chainId(), genesisSupply
    );

    const auto path = tempReportPath();
    const auto svcResult = node::RuntimeMonetaryReportService::buildAndPersist(
        policy, finalizedDeltas, 0, path
    );
    assert(svcResult.succeeded());

    // Tamper the file: overwrite with garbage.
    {
        std::ofstream f(path);
        f << "NODO_EPOCH_MONETARY_REPORT\ntotalBurnedRawUnits=99999999\n";
    }

    const auto auditResult = node::ChainAuditor::auditLoadedRuntime(load, path);
    assert(!auditResult.passed());

    std::filesystem::remove(path);
}

// auditLoadedRuntimeDevMode skips report verification even when deltas exist.
// The explicit name makes the bypass visible at every call site.
void testDevModeSkipsReportVerificationWithDeltas() {
    const auto result = node::ChainAuditor::auditLoadedRuntimeDevMode(
        genesisLoadWithOneDelta()
    );
    assert(result.passed());
}

} // namespace

int main() {
    try {
        testGenesisRuntimePassesWithoutReportPath();
        testDevModePassesForGenesisRuntime();
        testNormalAuditFailsMissingReportWhenDeltasExist();
        testNormalAuditPassesWithMatchingReport();
        testNormalAuditFailsTamperedReport();
        testDevModeSkipsReportVerificationWithDeltas();

        std::cout << "Nodo chain auditor report verification tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo chain auditor report verification tests failed: "
                  << e.what() << "\n";
        return 1;
    }
}
