#include "core/StatePruner.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    // Records and retrieves state roots.
    {
        nodo::core::StatePruner pruner(10);
        pruner.recordStateRoot(1, "root-1");
        pruner.recordStateRoot(2, "root-2");
        pruner.recordStateRoot(3, "root-3");

        assert(pruner.hasStateRoot(1));
        assert(pruner.hasStateRoot(2));
        assert(pruner.hasStateRoot(3));
        assert(!pruner.hasStateRoot(4));
        assert(pruner.getStateRoot(1) == "root-1");
        assert(pruner.getStateRoot(3) == "root-3");
        assert(pruner.getStateRoot(99) == "");
        assert(pruner.prunedCount() == 0);
    }

    // Prunes entries older than the retention window.
    {
        nodo::core::StatePruner pruner(3);
        for (std::uint64_t h = 1; h <= 10; ++h) {
            pruner.recordStateRoot(h, "root-" + std::to_string(h));
        }
        // currentBlock=10, keepRecent=3 → pruneThreshold=7
        // Heights 1..6 should be pruned; 7..10 retained.
        pruner.pruneHistory(10);

        assert(!pruner.hasStateRoot(1));
        assert(!pruner.hasStateRoot(6));
        assert(pruner.hasStateRoot(7));
        assert(pruner.hasStateRoot(10));
        assert(pruner.prunedCount() == 6);
    }

    // No pruning when currentBlock <= keepRecentStatesCount.
    {
        nodo::core::StatePruner pruner(100);
        pruner.recordStateRoot(1, "root-1");
        pruner.recordStateRoot(50, "root-50");
        pruner.pruneHistory(50);
        assert(pruner.hasStateRoot(1));
        assert(pruner.prunedCount() == 0);
    }

    // Overwriting an existing entry updates the value.
    {
        nodo::core::StatePruner pruner(10);
        pruner.recordStateRoot(5, "original");
        pruner.recordStateRoot(5, "updated");
        assert(pruner.getStateRoot(5) == "updated");
    }

    std::cout << "StatePruner tests passed.\n";
    return 0;
}
