#include "consensus/ProposerSchedule.hpp"

#include "core/ValidatorRegistry.hpp"
#include "crypto/KeyPair.hpp"

#include <cassert>
#include <string>

namespace {

nodo::core::ValidatorRegistry makeRegistry(int count) {
    nodo::core::ValidatorRegistry registry;
    for (int i = 0; i < count; ++i) {
        const std::string seed = "proposer-test-validator-" + std::to_string(i);
        const auto kp = nodo::crypto::KeyPair::createDeterministicBls12381KeyPair(seed);
        const nodo::core::ValidatorRegistrationRecord record(
            kp.address().value(),
            kp.publicKey(),
            1,
            "meta-hash-" + std::to_string(i),
            1000
        );
        registry.registerValidator(record);
    }
    return registry;
}

void testProposerIsDeterministic() {
    const auto registry = makeRegistry(4);
    const std::string chainId = "nodo-testnet-1";

    const std::string p1 = nodo::consensus::ProposerSchedule::selectProposer(
        registry, chainId, 100, 1
    );
    const std::string p2 = nodo::consensus::ProposerSchedule::selectProposer(
        registry, chainId, 100, 1
    );
    assert(p1 == p2);
    assert(!p1.empty());
}

void testProposerChangesWithRound() {
    const auto registry = makeRegistry(4);
    const std::string chainId = "nodo-testnet-1";

    bool anyDifference = false;
    const std::string p0 = nodo::consensus::ProposerSchedule::selectProposer(
        registry, chainId, 1, 1
    );
    for (std::uint64_t r = 2; r <= 10; ++r) {
        const std::string p = nodo::consensus::ProposerSchedule::selectProposer(
            registry, chainId, 1, r
        );
        assert(!p.empty());
        if (p != p0) {
            anyDifference = true;
        }
    }
    assert(anyDifference);
}

void testProposerEmptyRegistryReturnsEmpty() {
    const nodo::core::ValidatorRegistry empty;
    const std::string result = nodo::consensus::ProposerSchedule::selectProposer(
        empty, "nodo-localnet-1", 1, 1
    );
    assert(result.empty());
}

void testProposerWithSingleValidator() {
    const auto registry = makeRegistry(1);
    for (int i = 1; i <= 5; ++i) {
        const std::string p = nodo::consensus::ProposerSchedule::selectProposer(
            registry, "nodo-localnet-1", static_cast<std::uint64_t>(i), 1
        );
        assert(!p.empty());
    }
}

void testSelectionKeyIsUnique() {
    const std::string k1 = nodo::consensus::ProposerSchedule::buildSelectionKey(
        "nodo-testnet-1", 100, 1
    );
    const std::string k2 = nodo::consensus::ProposerSchedule::buildSelectionKey(
        "nodo-testnet-1", 100, 2
    );
    const std::string k3 = nodo::consensus::ProposerSchedule::buildSelectionKey(
        "nodo-testnet-2", 100, 1
    );
    assert(k1 != k2);
    assert(k1 != k3);
}

} // namespace

int main() {
    testProposerIsDeterministic();
    testProposerChangesWithRound();
    testProposerEmptyRegistryReturnsEmpty();
    testProposerWithSingleValidator();
    testSelectionKeyIsUnique();
    return 0;
}
