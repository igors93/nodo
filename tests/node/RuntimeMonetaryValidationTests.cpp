#include "node/RuntimeMonetaryValidation.hpp"
#include "config/NetworkParameters.hpp"
#include "core/Block.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "node/NodeRuntime.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::node::NodeRuntime;
using nodo::node::NodeRuntimeConfig;
using nodo::node::NodeRuntimeFactory;
using nodo::p2p::PeerInfo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

GenesisConfig minimalGenesisConfig() {
    const nodo::crypto::PublicKey validatorPubKey =
        nodo::crypto::KeyPair::createDeterministicBls12381KeyPair(
            "rmv-test-validator"
        ).publicKey();

    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {BootstrapValidatorConfig(validatorPubKey, 1, 1, "rmv-test-validator")},
        {GenesisAccountConfig("nodo1test001", Amount::fromRawUnits(1000000), 0)},
        "rmv-test-genesis"
    );
}

NodeRuntime startRuntime() {
    const auto result = NodeRuntimeFactory::startFromGenesis(
        NodeRuntimeConfig(
            minimalGenesisConfig(),
            PeerInfo("rmv-peer", "127.0.0.1:9400", "nodo/0.1", 0, kTimestamp),
            16
        )
    );
    assert(result.started());
    return result.runtime();
}

void testAcceptedResultReportsAccepted() {
    NodeRuntime runtime = startRuntime();
    const auto& genesisBlock = runtime.blockchain().blocks().front();

    const auto result = nodo::node::RuntimeMonetaryValidation::validateCandidate(
        runtime.config().genesisConfig(), genesisBlock, Amount::fromRawUnits(0)
    );

    assert(result.isAccepted());
    assert(result.status() == nodo::node::RuntimeMonetaryValidationStatus::ACCEPTED);
    assert(result.reason().empty());
}

void testNoOpCandidateWithZeroFeePassesGate() {
    NodeRuntime runtime = startRuntime();
    const auto& genesisBlock = runtime.blockchain().blocks().front();

    const auto result = nodo::node::RuntimeMonetaryValidation::validateCandidate(
        runtime.config().genesisConfig(), genesisBlock, Amount::fromRawUnits(0)
    );

    assert(result.isAccepted());
    assert(result.supplyDelta().isValid());
    assert(result.supplyDelta().mintedAmount().isZero());
    assert(result.supplyDelta().burnedAmount().isZero());
}

void testCandidateWithFeeBurnBuildsRealSupplyDelta() {
    NodeRuntime runtime = startRuntime();
    const auto& genesisBlock = runtime.blockchain().blocks().front();

    // Pass a concrete fee burn amount — this should build a burn-only SupplyDelta.
    const auto result = nodo::node::RuntimeMonetaryValidation::validateCandidate(
        runtime.config().genesisConfig(), genesisBlock, Amount::fromRawUnits(20)
    );

    assert(result.isAccepted());
    assert(result.supplyDelta().isValid());
    assert(result.supplyDelta().burnedAmount() == Amount::fromRawUnits(20));
    assert(result.supplyDelta().mintedAmount().isZero());
    assert(result.supplyDelta().burnRecords().size() == 1);
    assert(result.supplyDelta().mintRecords().empty());
}

void testResultSerializationIncludesStatus() {
    NodeRuntime runtime = startRuntime();
    const auto& genesisBlock = runtime.blockchain().blocks().front();

    const auto result = nodo::node::RuntimeMonetaryValidation::validateCandidate(
        runtime.config().genesisConfig(), genesisBlock, Amount::fromRawUnits(0)
    );

    const std::string s = result.serialize();
    assert(!s.empty());
    assert(s.find("ACCEPTED") != std::string::npos);
}

void testStatusToString() {
    assert(nodo::node::runtimeMonetaryValidationStatusToString(
               nodo::node::RuntimeMonetaryValidationStatus::ACCEPTED) == "ACCEPTED");
    assert(nodo::node::runtimeMonetaryValidationStatusToString(
               nodo::node::RuntimeMonetaryValidationStatus::MONETARY_CONTEXT_UNAVAILABLE) ==
           "MONETARY_CONTEXT_UNAVAILABLE");
    assert(nodo::node::runtimeMonetaryValidationStatusToString(
               nodo::node::RuntimeMonetaryValidationStatus::REJECTED_BY_GATE) ==
           "REJECTED_BY_GATE");
}

} // namespace

int main() {
    testAcceptedResultReportsAccepted();
    testNoOpCandidateWithZeroFeePassesGate();
    testCandidateWithFeeBurnBuildsRealSupplyDelta();
    testResultSerializationIncludesStatus();
    testStatusToString();
    return 0;
}
