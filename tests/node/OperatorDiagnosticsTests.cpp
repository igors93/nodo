#include "node/OperatorDiagnostics.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

void testReportFieldsPopulated() {
  const auto params = nodo::config::NetworkParameters::developmentLocal();
  const auto report = nodo::node::OperatorDiagnostics::collect(
      params,
      "test-genesis-id", // registeredGenesisId
      "test-genesis-id", // manifestGenesisId
      "DEVELOPMENT_LOCAL", 100, "block-hash-100", 4, 3,
      true,       // genesisVerified
      true,       // genesisCompatible
      true,       // keyPolicyPassed
      "ACCEPTED", // latestImportStatus
      "",         // latestImportRejectionReason
      false,      // defenseRestrictionsActive
      {});

  assert(report.networkName() == "localnet");
  assert(report.chainId() == "nodo-localnet-1");
  assert(report.registeredGenesisId() == "test-genesis-id");
  assert(report.manifestGenesisId() == "test-genesis-id");
  assert(report.genesisId() == "test-genesis-id");
  assert(report.networkClass() == "DEVELOPMENT_LOCAL");
  assert(report.finalizedHeight() == 100);
  assert(report.validatorCount() == 4);
  assert(report.connectedPeers() == 3);
  assert(report.genesisVerified());
  assert(report.genesisCompatible());
  assert(report.keyPolicyPassed());
  assert(report.warnings().empty());
}

void testReadinessStatusReady() {
  const auto params = nodo::config::NetworkParameters::developmentLocal();
  const auto report = nodo::node::OperatorDiagnostics::collect(
      params, "g", "g", "DEVELOPMENT_LOCAL", 1, "h", 1, 1, true, true, true,
      "ACCEPTED", "", false, {});
  assert(report.readinessStatus() == "READY");
}

void testReadinessStatusNotReadyWhenGenesisNotVerified() {
  const auto params = nodo::config::NetworkParameters::developmentLocal();
  const auto report = nodo::node::OperatorDiagnostics::collect(
      params, "g", "g", "DEVELOPMENT_LOCAL", 1, "h", 1, 1, false, true, true,
      "ACCEPTED", "", false, {});
  assert(report.readinessStatus() == "NOT_READY");
}

void testReadinessStatusNotReadyWhenNoPeers() {
  const auto params = nodo::config::NetworkParameters::developmentLocal();
  const auto report = nodo::node::OperatorDiagnostics::collect(
      params, "g", "g", "DEVELOPMENT_LOCAL", 1, "h", 1, 0, true, true, true,
      "ACCEPTED", "", false, {});
  assert(report.readinessStatus() == "NOT_READY");
}

void testReadinessStatusNotReadyWhenGenesisIncompatible() {
  const auto params = nodo::config::NetworkParameters::developmentLocal();
  const auto report = nodo::node::OperatorDiagnostics::collect(
      params, "registered-genesis", "different-genesis", "DEVELOPMENT_LOCAL", 1,
      "h", 1, 1, true, false, true, "ACCEPTED", "", false, {});
  assert(report.readinessStatus() == "NOT_READY");
  assert(report.registeredGenesisId() == "registered-genesis");
  assert(report.manifestGenesisId() == "different-genesis");
  assert(!report.genesisCompatible());
}

void testWarningsIncluded() {
  const auto params = nodo::config::NetworkParameters::developmentLocal();
  const std::vector<std::string> warnings = {"peer count is low",
                                             "genesis not fully verified"};
  const auto report = nodo::node::OperatorDiagnostics::collect(
      params, "g", "g", "DEVELOPMENT_LOCAL", 0, "", 0, 0, false, false, false,
      "", "", false, warnings);
  assert(report.warnings().size() == 2);
}

void testSerializeContainsKeyFields() {
  const auto params = nodo::config::NetworkParameters::developmentLocal();
  const auto report = nodo::node::OperatorDiagnostics::collect(
      params, "my-genesis-id", "my-genesis-id", "DEVELOPMENT_LOCAL", 42,
      "hash-42", 2, 1, true, true, true, "ACCEPTED", "", false,
      {"test-warning"});
  const std::string s = report.serialize();
  assert(!s.empty());
  assert(s.find("localnet") != std::string::npos);
  assert(s.find("42") != std::string::npos);
  assert(s.find("test-warning") != std::string::npos);
  assert(s.find("DEVELOPMENT_LOCAL") != std::string::npos);
  assert(s.find("my-genesis-id") != std::string::npos);
}

void testLatestImportStatusExposed() {
  const auto params = nodo::config::NetworkParameters::developmentLocal();
  const auto report = nodo::node::OperatorDiagnostics::collect(
      params, "g", "g", "DEVELOPMENT_LOCAL", 5, "hash-5", 1, 1, true, true,
      true, "REJECTED", "HEIGHT_CONTINUITY_MISMATCH", false, {});
  assert(report.latestImportStatus() == "REJECTED");
  assert(report.latestImportRejectionReason() == "HEIGHT_CONTINUITY_MISMATCH");
}

void testDefenseRestrictionsExposed() {
  const auto params = nodo::config::NetworkParameters::developmentLocal();
  const auto report = nodo::node::OperatorDiagnostics::collect(
      params, "g", "g", "DEVELOPMENT_LOCAL", 1, "h", 1, 1, true, true, true,
      "ACCEPTED", "", true, {});
  assert(report.defenseRestrictionsActive());
  const std::string s = report.serialize();
  assert(s.find("defenseRestrictionsActive=yes") != std::string::npos);
}

} // namespace

int main() {
  testReportFieldsPopulated();
  testReadinessStatusReady();
  testReadinessStatusNotReadyWhenGenesisNotVerified();
  testReadinessStatusNotReadyWhenNoPeers();
  testReadinessStatusNotReadyWhenGenesisIncompatible();
  testWarningsIncluded();
  testSerializeContainsKeyFields();
  testLatestImportStatusExposed();
  testDefenseRestrictionsExposed();
  return 0;
}
