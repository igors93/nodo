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

    // Test 1: basic round state round-trips correctly.
    {
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
        assert(loaded->lockedBlockHash() == "");
        assert(loaded->lockedRound() == 0);
        assert(loaded->votedPrevote() == false);
        assert(loaded->votedPrecommit() == false);
    }

    // Test 2: lock/vote fields serialize and deserialize correctly.
    {
        const nodo::consensus::ConsensusRoundState stateWithLock(
            10,
            2,
            "validator-b",
            1900000001,
            "abc123blockhashhex",
            2,
            true,
            false
        );

        assert(nodo::consensus::ConsensusRecoveryStore::save(path, stateWithLock));

        const auto loaded =
            nodo::consensus::ConsensusRecoveryStore::load(path);
        assert(loaded.has_value());
        assert(loaded->height() == 10);
        assert(loaded->round() == 2);
        assert(loaded->proposerAddress() == "validator-b");
        assert(loaded->lockedBlockHash() == "abc123blockhashhex");
        assert(loaded->lockedRound() == 2);
        assert(loaded->votedPrevote() == true);
        assert(loaded->votedPrecommit() == false);
    }

    // Test 3: both voted flags true.
    {
        const nodo::consensus::ConsensusRoundState statePrecommitted(
            77,
            1,
            "validator-c",
            1900000002,
            "deadbeef01020304",
            1,
            true,
            true
        );

        assert(nodo::consensus::ConsensusRecoveryStore::save(path, statePrecommitted));

        const auto loaded =
            nodo::consensus::ConsensusRecoveryStore::load(path);
        assert(loaded.has_value());
        assert(loaded->votedPrevote() == true);
        assert(loaded->votedPrecommit() == true);
        assert(loaded->lockedBlockHash() == "deadbeef01020304");
        assert(loaded->lockedRound() == 1);
    }

    // Test 4: file with unexpected extra field is rejected.
    {
        {
            std::ofstream out(path, std::ios::binary | std::ios::app);
            out << "unexpected=field\n";
        }

        assert(!nodo::consensus::ConsensusRecoveryStore::load(path).has_value());
    }

    // Test 5: remove and confirm gone.
    assert(nodo::consensus::ConsensusRecoveryStore::remove(path));
    assert(!nodo::consensus::ConsensusRecoveryStore::load(path).has_value());

    return 0;
}
