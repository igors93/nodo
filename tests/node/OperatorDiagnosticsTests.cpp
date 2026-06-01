#include "node/OperatorDiagnostics.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

void testReportFieldsPopulated() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto report = nodo::node::OperatorDiagnostics::collect(
        params,
        100,
        4,
        3,
        true,
        true,
        {}
    );

    assert(report.networkName() == "nodo-localnet");
    assert(report.chainId() == "nodo-localnet-1");
    assert(report.finalizedHeight() == 100);
    assert(report.validatorCount() == 4);
    assert(report.connectedPeers() == 3);
    assert(report.genesisVerified());
    assert(report.keyPolicyPassed());
    assert(report.warnings().empty());
}

void testReadinessStatusReady() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto report = nodo::node::OperatorDiagnostics::collect(
        params, 1, 1, 1, true, true, {}
    );
    assert(report.readinessStatus() == "READY");
}

void testReadinessStatusNotReadyWhenGenesisNotVerified() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto report = nodo::node::OperatorDiagnostics::collect(
        params, 1, 1, 1, false, true, {}
    );
    assert(report.readinessStatus() == "NOT_READY");
}

void testReadinessStatusNotReadyWhenNoPeers() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto report = nodo::node::OperatorDiagnostics::collect(
        params, 1, 1, 0, true, true, {}
    );
    assert(report.readinessStatus() == "NOT_READY");
}

void testWarningsIncluded() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const std::vector<std::string> warnings = {
        "peer count is low",
        "genesis not fully verified"
    };
    const auto report = nodo::node::OperatorDiagnostics::collect(
        params, 0, 0, 0, false, false, warnings
    );
    assert(report.warnings().size() == 2);
}

void testSerializeContainsKeyFields() {
    const auto params = nodo::config::NetworkParameters::developmentLocal();
    const auto report = nodo::node::OperatorDiagnostics::collect(
        params, 42, 2, 1, true, true, {"test-warning"}
    );
    const std::string s = report.serialize();
    assert(!s.empty());
    assert(s.find("nodo-localnet") != std::string::npos);
    assert(s.find("42") != std::string::npos);
    assert(s.find("test-warning") != std::string::npos);
}

} // namespace

int main() {
    testReportFieldsPopulated();
    testReadinessStatusReady();
    testReadinessStatusNotReadyWhenGenesisNotVerified();
    testReadinessStatusNotReadyWhenNoPeers();
    testWarningsIncluded();
    testSerializeContainsKeyFields();
    return 0;
}
