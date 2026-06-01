#include "node/RuntimeSupplyState.hpp"
#include "economics/SupplyDelta.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

using nodo::utils::Amount;
using nodo::node::RuntimeSupplyState;

nodo::economics::BurnRecord feeBurn(
    const std::string& id,
    std::int64_t rawUnits,
    std::uint64_t blockHeight
) {
    return nodo::economics::BurnRecord(
        id, blockHeight, 0, "nodo_fee_pool",
        Amount::fromRawUnits(rawUnits),
        "fee burn", nodo::economics::BurnType::FEE_BURN
    );
}

nodo::economics::SupplyDelta burnDelta(
    std::uint64_t blockHeight,
    const std::string& hash,
    Amount supplyBefore,
    std::int64_t burnRaw
) {
    return nodo::economics::SupplyDelta(
        blockHeight, hash, 0,
        supplyBefore,
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(burnRaw),
        Amount::fromRawUnits(supplyBefore.rawUnits() - burnRaw),
        {},
        {feeBurn("burn-" + hash, burnRaw, blockHeight)}
    );
}

void testInitialSupplyEqualsGenesisSupply() {
    const RuntimeSupplyState state(Amount::fromRawUnits(1000000));
    assert(state.latestSupply() == Amount::fromRawUnits(1000000));
    assert(state.finalizedDeltaCount() == 0);
}

void testApplyDeltaAdvancesSupply() {
    RuntimeSupplyState state(Amount::fromRawUnits(1000));
    const auto delta = burnDelta(1, "hash-1", Amount::fromRawUnits(1000), 20);
    state.applyFinalizedDelta(delta);
    assert(state.latestSupply() == Amount::fromRawUnits(980));
    assert(state.finalizedDeltaCount() == 1);
}

void testApplyInvalidDeltaThrows() {
    RuntimeSupplyState state(Amount::fromRawUnits(1000));
    const nodo::economics::SupplyDelta invalidDelta(
        1,
        "",
        0,
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(1000),
        {},
        {}
    );

    bool threw = false;
    try {
        state.applyFinalizedDelta(invalidDelta);
    } catch (const std::invalid_argument& e) {
        threw = true;
        const std::string msg(e.what());
        assert(msg.find("invalid delta") != std::string::npos);
    }
    assert(threw);
    assert(state.latestSupply() == Amount::fromRawUnits(1000));
    assert(state.finalizedDeltaCount() == 0);
}

void testTwoSequentialDeltas() {
    RuntimeSupplyState state(Amount::fromRawUnits(1000));
    state.applyFinalizedDelta(burnDelta(1, "h1", Amount::fromRawUnits(1000), 20));
    state.applyFinalizedDelta(burnDelta(2, "h2", Amount::fromRawUnits(980), 10));
    assert(state.latestSupply() == Amount::fromRawUnits(970));
    assert(state.finalizedDeltaCount() == 2);
}

void testSupplyBefore1EqualsSupplyAfter0() {
    RuntimeSupplyState state(Amount::fromRawUnits(5000));
    state.applyFinalizedDelta(burnDelta(1, "h-a", Amount::fromRawUnits(5000), 50));
    const Amount supplyAfterBlock1 = state.latestSupply();
    assert(supplyAfterBlock1 == Amount::fromRawUnits(4950));
    // Next delta must start from 4950.
    state.applyFinalizedDelta(burnDelta(2, "h-b", supplyAfterBlock1, 30));
    assert(state.latestSupply() == Amount::fromRawUnits(4920));
}

void testContinuityBreakThrows() {
    RuntimeSupplyState state(Amount::fromRawUnits(1000));
    state.applyFinalizedDelta(burnDelta(1, "h1", Amount::fromRawUnits(1000), 20));
    // Wrong supplyBefore for block 2 should throw.
    bool threw = false;
    try {
        state.applyFinalizedDelta(
            burnDelta(2, "h2", Amount::fromRawUnits(1000), 10)  // wrong: should be 980
        );
    } catch (const std::invalid_argument& e) {
        threw = true;
        const std::string msg(e.what());
        assert(msg.find("continuity") != std::string::npos);
    }
    assert(threw);
}

void testSerialize() {
    const RuntimeSupplyState state(Amount::fromRawUnits(42000));
    const std::string s = state.serialize();
    assert(!s.empty());
    assert(s.find("42000") != std::string::npos);
}

} // namespace

int main() {
    testInitialSupplyEqualsGenesisSupply();
    testApplyDeltaAdvancesSupply();
    testApplyInvalidDeltaThrows();
    testTwoSequentialDeltas();
    testSupplyBefore1EqualsSupplyAfter0();
    testContinuityBreakThrows();
    testSerialize();
    return 0;
}
