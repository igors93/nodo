#include "config/NetworkParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTs = 1900000000LL;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

config::GenesisConfig genesisWithFee(std::uint64_t minFeeRaw) {
    config::NetworkParameters params = config::NetworkParameters::developmentLocal();
    // Build a params with the desired minFee by re-constructing.
    // developmentLocal() has minFeeRaw == 0; we test the governance override below.
    return config::GenesisConfig(
        params,
        kTs,
        { config::BootstrapValidatorConfig(
            crypto::KeyPair::createDeterministicBls12381KeyPair("gap5-val").publicKey(),
            1, 1, "gap5-val-meta"
          ) },
        {},
        "gap5-test"
    );
}

node::NodeRuntime startRuntime(const config::GenesisConfig& genesis) {
    const p2p::PeerInfo peer("gap5-node", "127.0.0.1:19997", "nodo/test", 0, kTs);
    const auto result = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peer, 10)
    );
    if (!result.started()) {
        throw std::runtime_error("NodeRuntimeFactory failed: " + result.reason());
    }
    return result.runtime();
}

void testEffectiveFeeDefaultsToGenesisValue() {
    const config::GenesisConfig genesis = genesisWithFee(0);
    node::NodeRuntime runtime = startRuntime(genesis);

    const std::uint64_t genesisMinFee =
        genesis.networkParameters().minimumFeeRawUnits();
    const std::uint64_t effective = runtime.effectiveMinimumFeeRawUnits();

    require(effective == genesisMinFee,
            "Effective fee must equal genesis fee when no governance override exists.");
}

void testGovernanceOverrideChangesEffectiveFee() {
    const config::GenesisConfig genesis = genesisWithFee(0);
    node::NodeRuntime runtime = startRuntime(genesis);

    // Apply a governance parameter change directly through the executor.
    const std::string proposalId = "gov-prop-fee-001";
    const std::string payload = "target=MINIMUM_FEE_RAW;value=500;effectiveHeight=1";
    const auto result = runtime.mutableGovernanceExecutor().executeProposal(
        proposalId, payload, 1, kTs
    );

    require(result.isApplied(),
            "Governance proposal must be applied when effectiveHeight <= currentHeight.");

    const std::uint64_t effective = runtime.effectiveMinimumFeeRawUnits();
    require(effective == 500,
            "Effective fee must equal the governance-applied value (500).");
}

void testGovernanceOverrideDoesNotAffectUnrelatedParams() {
    const config::GenesisConfig genesis = genesisWithFee(0);
    node::NodeRuntime runtime = startRuntime(genesis);

    // Apply a change to EPOCH_DURATION_SECONDS — should not affect fee.
    runtime.mutableGovernanceExecutor().executeProposal(
        "gov-prop-epoch-001",
        "target=EPOCH_DURATION_SECONDS;value=7200;effectiveHeight=1",
        1, kTs
    );

    const std::uint64_t effective = runtime.effectiveMinimumFeeRawUnits();
    const std::uint64_t genesis_fee = genesis.networkParameters().minimumFeeRawUnits();
    require(effective == genesis_fee,
            "Effective fee must remain at genesis value when only epoch duration is governed.");
}

void testDoubleGovernanceApplicationIsIdempotent() {
    const config::GenesisConfig genesis = genesisWithFee(0);
    node::NodeRuntime runtime = startRuntime(genesis);

    const std::string proposalId = "gov-prop-fee-002";
    const std::string payload = "target=MINIMUM_FEE_RAW;value=1000;effectiveHeight=1";

    runtime.mutableGovernanceExecutor().executeProposal(proposalId, payload, 1, kTs);
    runtime.mutableGovernanceExecutor().executeProposal(proposalId, payload, 2, kTs + 1);

    require(runtime.effectiveMinimumFeeRawUnits() == 1000,
            "Fee must be 1000 even after duplicate proposal execution attempt.");
}

} // namespace

int main() {
    try {
        testEffectiveFeeDefaultsToGenesisValue();
        testGovernanceOverrideChangesEffectiveFee();
        testGovernanceOverrideDoesNotAffectUnrelatedParams();
        testDoubleGovernanceApplicationIsIdempotent();

        std::cout << "Nodo Gap5 governance-effective-params tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo Gap5 governance-effective-params tests failed: "
                  << e.what() << "\n";
        return 1;
    }
}
