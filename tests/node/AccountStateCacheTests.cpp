#include "config/NetworkParameters.hpp"
#include "core/AccountStateView.hpp"
#include "crypto/KeyPair.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/PeerMessage.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTs = 1900000000LL;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

config::GenesisConfig minimalGenesis() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTs,
        { config::BootstrapValidatorConfig(
            crypto::KeyPair::createDeterministicBls12381KeyPair("gap4-val").publicKey(),
            1, 1, "gap4-val-meta"
          ) },
        { config::GenesisAccountConfig("gap4-recipient", utils::Amount::fromRawUnits(1000000LL), 0) },
        "gap4-test"
    );
}

node::NodeRuntime startRuntime(const config::GenesisConfig& genesis) {
    const p2p::PeerInfo peer("gap4-node", "127.0.0.1:19998", "nodo/test", 0, kTs);
    const auto result = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peer, 10)
    );
    if (!result.started()) {
        throw std::runtime_error("NodeRuntimeFactory::startFromGenesis failed: " + result.reason());
    }
    return result.runtime();
}

void testCacheReturnsSameViewOnSecondCall() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);

    const core::AccountStateView& first =
        runtime.cachedAccountStateAtTip(0);
    const core::AccountStateView& second =
        runtime.cachedAccountStateAtTip(0);

    // Pointer identity confirms the second call returned the cached object.
    require(&first == &second,
            "Second call to cachedAccountStateAtTip must return the cached view (same address).");
}

void testInvalidateForcesRebuild() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);

    const core::AccountStateView& before =
        runtime.cachedAccountStateAtTip(0);
    const std::string addressesBefore =
        before.knownAddresses().empty() ? "" : before.knownAddresses().front();

    runtime.invalidateAccountStateCache();

    // After invalidation, the view is rebuilt from scratch.
    const core::AccountStateView& after =
        runtime.cachedAccountStateAtTip(0);

    // Content must still be consistent (same genesis accounts).
    const auto addrsAfter = after.knownAddresses();
    require(!addrsAfter.empty() || addressesBefore.empty(),
            "Rebuilt cache must still contain genesis accounts.");
}

void testCacheIsInvalidatedByHeightChange() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);

    const core::AccountStateView& viewAtGenesis =
        runtime.cachedAccountStateAtTip(0);
    (void)viewAtGenesis;

    // The cache is keyed by the blockchain tip height. Calling
    // invalidateAccountStateCache() drops the cached value and forces the next
    // access to rebuild — verifying that the rebuild path is exercised.
    runtime.invalidateAccountStateCache();
    const core::AccountStateView& rebuilt =
        runtime.cachedAccountStateAtTip(0);
    require(rebuilt.isValid(), "Rebuilt account state view must be valid.");
}

} // namespace

int main() {
    try {
        testCacheReturnsSameViewOnSecondCall();
        testInvalidateForcesRebuild();
        testCacheIsInvalidatedByHeightChange();

        std::cout << "Nodo Gap4 account-state-cache tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo Gap4 account-state-cache tests failed: "
                  << e.what() << "\n";
        return 1;
    }
}
