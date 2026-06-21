#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ConsensusRoundManager.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

int main() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path()
        / "nodo-consensus-recovery-store-test.state";

    std::error_code cleanupError;
    std::filesystem::remove(path, cleanupError);
    std::filesystem::remove(path.string() + ".tmp", cleanupError);

    const nodo::consensus::ConsensusRoundState state(
        42,
        3,
        "validator-a",
        1900000000
    );

    assert(nodo::consensus::ConsensusRecoveryStore::save(path, state));

    const auto loaded =
        nodo::consensus::ConsensusRecoveryStore::load(path);
    assert(loaded.has_value());
    assert(loaded->height() == 42);
    assert(loaded->round() == 3);
    assert(loaded->proposerAddress() == "validator-a");

    {
        std::ofstream out(path, std::ios::binary | std::ios::app);
        out << "unexpected=field\n";
    }

    assert(!nodo::consensus::ConsensusRecoveryStore::load(path).has_value());
    assert(nodo::consensus::ConsensusRecoveryStore::remove(path));
    assert(!nodo::consensus::ConsensusRecoveryStore::load(path).has_value());

    return 0;
}
