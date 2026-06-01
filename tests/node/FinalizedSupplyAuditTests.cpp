#include "node/FinalizedSupplyAudit.hpp"

#include "economics/MintRecord.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using nodo::utils::Amount;

nodo::economics::MonetaryPolicy testPolicy() {
    return nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-finalized-supply-audit-test",
        Amount::fromRawUnits(1000)
    );
}

nodo::economics::SupplyDelta noOpDelta(
    std::uint64_t height,
    Amount supply
) {
    return nodo::economics::SupplyDelta::noOp(
        height,
        "block-hash-" + std::to_string(height),
        0,
        supply
    );
}

nodo::economics::BurnRecord feeBurn(
    std::uint64_t height,
    std::int64_t rawUnits
) {
    return nodo::economics::BurnRecord(
        "fee-burn-" + std::to_string(height),
        height,
        0,
        "nodo_fee_pool",
        Amount::fromRawUnits(rawUnits),
        "fee burn",
        nodo::economics::BurnType::FEE_BURN
    );
}

nodo::economics::SupplyDelta burnDelta(
    std::uint64_t height,
    Amount supplyBefore,
    std::int64_t burnRawUnits
) {
    return nodo::economics::SupplyDelta(
        height,
        "block-hash-" + std::to_string(height),
        0,
        supplyBefore,
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(burnRawUnits),
        Amount::fromRawUnits(supplyBefore.rawUnits() - burnRawUnits),
        {},
        {feeBurn(height, burnRawUnits)}
    );
}

nodo::core::LedgerRecord blockLedgerRecord(
    std::uint64_t height,
    std::int64_t timestamp
) {
    const std::string blockHash =
        "source-block-hash-" + std::to_string(height);

    const nodo::economics::MintRecord mint(
        "block-mint-" + std::to_string(height),
        "block-auth-" + std::to_string(height),
        "nodo_finalized_supply_audit_recipient",
        Amount::fromRawUnits(1),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        0,
        height,
        blockHash,
        timestamp
    );

    return nodo::core::LedgerRecord::fromMintRecord(
        mint,
        timestamp
    );
}

nodo::core::Block testBlock(
    std::uint64_t height
) {
    const std::int64_t timestamp =
        1900000000 + static_cast<std::int64_t>(height);

    return nodo::core::Block(
        height,
        "previous-hash-" + std::to_string(height),
        {blockLedgerRecord(height, timestamp)},
        timestamp
    );
}

nodo::node::FinalizedBlockArtifact artifactWithDelta(
    std::uint64_t height,
    nodo::economics::SupplyDelta delta
) {
    nodo::core::Block block =
        testBlock(height);

    return nodo::node::FinalizedBlockArtifact(
        block,
        "state-root-" + std::to_string(height),
        Amount::fromRawUnits(0),
        std::move(delta),
        {},
        {},
        {},
        {},
        {},
        {},
        {},
        nodo::node::MonetaryFirewallAudit::notEvaluated(),
        nodo::node::GenesisTreasurySnapshot::notEvaluated(),
        nodo::node::ProtectionRewardBudget::notEvaluated(),
        {},
        {},
        nodo::node::ProtectionRewardSummary::notEvaluated(),
        {},
        nodo::node::InflationEpochSnapshot::notEvaluated(),
        nodo::node::MintAuthorizationRecord(),
        nodo::node::SupplyExpansionRecord(),
        nodo::node::FeeEconomicBalance::notEvaluated(),
        nodo::node::FeeBurnRecord::notEvaluated(),
        nodo::node::TreasuryFeeRecord::notEvaluated(),
        {},
        {},
        nodo::node::SlashingEvidenceSummary::notEvaluated(),
        {},
        {},
        nodo::node::CryptographicSlashingSummary::notEvaluated(),
        nodo::node::GovernancePolicySnapshot::notEvaluated(),
        {},
        nodo::node::GovernanceSummary::notEvaluated(),
        {},
        nodo::node::EpochAccountingRecord::notEvaluated(),
        nodo::node::ValidatorLifecycleSummary::notEvaluated(),
        nodo::consensus::QuorumCertificate(),
        nodo::consensus::FinalizedBlockRecord()
    );
}

nodo::node::FinalizedBlockArtifact artifactWithMatchingDelta(
    std::uint64_t height,
    Amount supply
) {
    const nodo::core::Block block =
        testBlock(height);

    return artifactWithDelta(
        height,
        nodo::economics::SupplyDelta::noOp(
            height,
            block.hash(),
            0,
            supply
        )
    );
}

void testAuditDeltasPassesValidSequence() {
    const std::vector<nodo::economics::SupplyDelta> deltas = {
        noOpDelta(1, Amount::fromRawUnits(1000)),
        noOpDelta(2, Amount::fromRawUnits(1000)),
        noOpDelta(3, Amount::fromRawUnits(1000))
    };

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditDeltas(testPolicy(), deltas);

    assert(result.passed());
    assert(result.deltaCount() == 3);
    assert(result.finalSupply() == Amount::fromRawUnits(1000));
}

void testAuditDeltasRejectsContinuityReset() {
    const std::vector<nodo::economics::SupplyDelta> deltas = {
        burnDelta(1, Amount::fromRawUnits(1000), 20),
        noOpDelta(2, Amount::fromRawUnits(1000))
    };

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditDeltas(testPolicy(), deltas);

    assert(!result.passed());
    assert(result.reason().find("continuity") != std::string::npos);
    assert(result.failedBlockHeight() == 2);
}

void testAuditDeltasRejectsInvalidDelta() {
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

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditDeltas(testPolicy(), {invalidDelta});

    assert(!result.passed());
    assert(result.reason().find("invalid") != std::string::npos);
    assert(result.failedBlockHeight() == 1);
}

void testAuditDeltasRejectsOutOfOrderHeight() {
    const std::vector<nodo::economics::SupplyDelta> deltas = {
        noOpDelta(1, Amount::fromRawUnits(1000)),
        noOpDelta(3, Amount::fromRawUnits(1000))
    };

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditDeltas(testPolicy(), deltas);

    assert(!result.passed());
    assert(result.reason().find("out-of-order") != std::string::npos);
    assert(result.failedBlockHeight() == 3);
}

void testDefaultArtifactWithoutSupplyDeltaIsInvalid() {
    const nodo::node::FinalizedBlockArtifact artifact;
    assert(!artifact.isValid());
    assert(!artifact.supplyDelta().isValid());

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditArtifacts(testPolicy(), {artifact});

    assert(!result.passed());
    assert(result.reason().find("missing SupplyDelta") != std::string::npos ||
           result.reason().find("invalid") != std::string::npos);
}

void testAuditArtifactsPassesMatchingSupplySequence() {
    const std::vector<nodo::node::FinalizedBlockArtifact> artifacts = {
        artifactWithMatchingDelta(1, Amount::fromRawUnits(1000)),
        artifactWithMatchingDelta(2, Amount::fromRawUnits(1000))
    };

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditArtifacts(testPolicy(), artifacts);

    assert(result.passed());
    assert(result.deltaCount() == 2);
    assert(result.finalSupply() == Amount::fromRawUnits(1000));
}

void testAuditArtifactsRejectsHeightMismatch() {
    const nodo::core::Block block =
        testBlock(1);

    const auto artifact = artifactWithDelta(
        1,
        nodo::economics::SupplyDelta::noOp(
            2,
            block.hash(),
            0,
            Amount::fromRawUnits(1000)
        )
    );

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditArtifacts(testPolicy(), {artifact});

    assert(!result.passed());
    assert(result.reason().find("identity") != std::string::npos);
}

void testAuditArtifactsRejectsHashMismatch() {
    const auto artifact = artifactWithDelta(
        1,
        nodo::economics::SupplyDelta::noOp(
            1,
            "wrong-block-hash",
            0,
            Amount::fromRawUnits(1000)
        )
    );

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditArtifacts(testPolicy(), {artifact});

    assert(!result.passed());
    assert(result.reason().find("identity") != std::string::npos);
}

void testAuditArtifactsRejectsOutOfOrderHeight() {
    const std::vector<nodo::node::FinalizedBlockArtifact> artifacts = {
        artifactWithMatchingDelta(1, Amount::fromRawUnits(1000)),
        artifactWithMatchingDelta(3, Amount::fromRawUnits(1000))
    };

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditArtifacts(testPolicy(), artifacts);

    assert(!result.passed());
    assert(result.reason().find("out-of-order") != std::string::npos);
}

// Artifact sequence must start at height 1, not 0.
// Genesis (height 0) is not stored as a FinalizedBlockArtifact.
void testAuditDeltasRejectsSequenceStartingAtHeightZero() {
    const std::vector<nodo::economics::SupplyDelta> deltas = {
        noOpDelta(0, Amount::fromRawUnits(1000))
    };

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditDeltas(testPolicy(), deltas);

    assert(!result.passed());
    assert(result.reason().find("out-of-order") != std::string::npos);
    assert(result.failedBlockHeight() == 0);
}

// A missing height in the sequence is rejected.
void testAuditDeltasRejectsMissingHeight() {
    const std::vector<nodo::economics::SupplyDelta> deltas = {
        noOpDelta(1, Amount::fromRawUnits(1000)),
        noOpDelta(3, Amount::fromRawUnits(1000))   // height 2 is missing
    };

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditDeltas(testPolicy(), deltas);

    assert(!result.passed());
    assert(result.failedBlockHeight() == 3);
}

// Artifact starting at height 0 (genesis) is rejected by auditArtifacts.
void testAuditArtifactsRejectsGenesisHeightArtifact() {
    // A delta at height 0 but pointing to block at height 0 — rejected because
    // FinalizedSupplyAudit expects artifacts to start at height 1.
    const nodo::node::FinalizedBlockArtifact artifact =
        artifactWithMatchingDelta(0, Amount::fromRawUnits(1000));

    const auto result =
        nodo::node::FinalizedSupplyAudit::auditArtifacts(testPolicy(), {artifact});

    assert(!result.passed());
    assert(result.reason().find("out-of-order") != std::string::npos ||
           result.reason().find("expected") != std::string::npos);
}

} // namespace

int main() {
    testAuditDeltasPassesValidSequence();
    testAuditDeltasRejectsContinuityReset();
    testAuditDeltasRejectsInvalidDelta();
    testAuditDeltasRejectsOutOfOrderHeight();
    testDefaultArtifactWithoutSupplyDeltaIsInvalid();
    testAuditArtifactsPassesMatchingSupplySequence();
    testAuditArtifactsRejectsHeightMismatch();
    testAuditArtifactsRejectsHashMismatch();
    testAuditArtifactsRejectsOutOfOrderHeight();
    testAuditDeltasRejectsSequenceStartingAtHeightZero();
    testAuditDeltasRejectsMissingHeight();
    testAuditArtifactsRejectsGenesisHeightArtifact();
    return 0;
}
