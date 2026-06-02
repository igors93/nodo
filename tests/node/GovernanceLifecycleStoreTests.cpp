#include "node/GovernanceLifecycleStore.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "node/GovernanceLifecycleCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <cassert>
#include <filesystem>
#include <string>

namespace {

using nodo::node::GovernanceLifecycleCodec;
using nodo::node::GovernanceLifecycleStore;
using nodo::node::GovernanceLifecycleStoreStatus;
using nodo::tests::fixtures::validLifecycle;

void testStoreRoundTripVerifiesLifecycle() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nodo_governance_lifecycle_store_test";
    std::filesystem::remove_all(root);

    const GovernanceLifecycleStore store(root);
    const auto lifecycle = validLifecycle();
    const auto saved = store.save(lifecycle);
    assert(saved.success());
    assert(saved.status() == GovernanceLifecycleStoreStatus::STORED);
    assert(std::filesystem::exists(store.pathForLifecycleId(lifecycle.lifecycleId())));

    const auto loaded = store.load(lifecycle.lifecycleId());
    assert(loaded.success());
    assert(loaded.status() == GovernanceLifecycleStoreStatus::LOADED);
    assert(loaded.lifecycle().lifecycleId() == lifecycle.lifecycleId());

    std::filesystem::remove_all(root);
}

void testMissingLifecycleReturnsClearStatus() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nodo_governance_lifecycle_store_missing";
    std::filesystem::remove_all(root);

    const GovernanceLifecycleStore store(root);
    const auto loaded = store.load("missing-lifecycle");
    assert(!loaded.success());
    assert(loaded.status() == GovernanceLifecycleStoreStatus::NOT_FOUND);

    std::filesystem::remove_all(root);
}

void testTamperedLifecycleFileRejected() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nodo_governance_lifecycle_store_tamper";
    std::filesystem::remove_all(root);

    const GovernanceLifecycleStore store(root);
    const auto lifecycle = validLifecycle();
    assert(store.save(lifecycle).success());

    std::string encoded = GovernanceLifecycleCodec::encode(lifecycle);
    const std::string from = "decision.decisionProof=" +
        lifecycle.decisionRecord().decisionProof() + "\n";
    const std::string to = "decision.decisionProof=tampered-proof\n";
    const std::size_t pos = encoded.find(from);
    assert(pos != std::string::npos);
    encoded.replace(pos, from.size(), to);
    nodo::storage::AtomicFile::writeTextFile(
        store.pathForLifecycleId(lifecycle.lifecycleId()),
        encoded
    );

    const auto loaded = store.load(lifecycle.lifecycleId());
    assert(!loaded.success());

    std::filesystem::remove_all(root);
}

} // namespace

int main() {
    testStoreRoundTripVerifiesLifecycle();
    testMissingLifecycleReturnsClearStatus();
    testTamperedLifecycleFileRejected();
    return 0;
}
