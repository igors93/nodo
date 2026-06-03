// P1 tests: AuditAssignment determinism with same/different seed.
#include "node/AuditAssignment.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::node::AuditAssignment;
using nodo::node::AuditAssignmentCalculator;
using nodo::node::AuditAssignmentTargetType;

// Test 21: AuditAssignment is deterministic with the same seed.
void testAuditAssignmentDeterministic() {
    const std::string seed = "blockhash-seed-001";
    const std::vector<std::string> validators = {"validator-A", "validator-B", "validator-C"};

    const AuditAssignment a1 = AuditAssignmentCalculator::buildAssignment(
        "assign-001", 100, 1,
        AuditAssignmentTargetType::BLOCK_ARTIFACT,
        "target-digest-abc",
        seed, validators
    );
    const AuditAssignment a2 = AuditAssignmentCalculator::buildAssignment(
        "assign-001", 100, 1,
        AuditAssignmentTargetType::BLOCK_ARTIFACT,
        "target-digest-abc",
        seed, validators
    );

    assert(a1.isValid());
    assert(a2.isValid());
    assert(a1.validatorAddress() == a2.validatorAddress());
    assert(a1.assignmentProof() == a2.assignmentProof());
}

// Test 22: AuditAssignment changes when seed changes.
void testAuditAssignmentChangesWithSeed() {
    const std::vector<std::string> validators = {"validator-A", "validator-B", "validator-C"};

    const AuditAssignment a1 = AuditAssignmentCalculator::buildAssignment(
        "assign-002", 100, 1,
        AuditAssignmentTargetType::BLOCK_ARTIFACT,
        "target-digest-abc",
        "seed-version-1", validators
    );
    const AuditAssignment a2 = AuditAssignmentCalculator::buildAssignment(
        "assign-002", 100, 1,
        AuditAssignmentTargetType::BLOCK_ARTIFACT,
        "target-digest-abc",
        "seed-version-2", validators  // different seed
    );

    assert(a1.isValid());
    assert(a2.isValid());
    // Different seeds should (with overwhelming probability) produce different assignments.
    // Note: with only 3 validators, collisions are possible but unlikely for different seeds.
    // The key property is that assignmentProof is different.
    assert(a1.assignmentProof() != a2.assignmentProof());
}

// Test 23: AuditAssignment changes when target changes.
void testAuditAssignmentChangesWithTarget() {
    const std::vector<std::string> validators = {"validator-A", "validator-B", "validator-C"};
    const std::string seed = "same-seed";

    const AuditAssignment a1 = AuditAssignmentCalculator::buildAssignment(
        "assign-003", 100, 1,
        AuditAssignmentTargetType::BLOCK_ARTIFACT,
        "target-abc",
        seed, validators
    );
    const AuditAssignment a2 = AuditAssignmentCalculator::buildAssignment(
        "assign-003", 100, 1,
        AuditAssignmentTargetType::TREASURY_SECTION,  // different target type
        "target-abc",
        seed, validators
    );

    assert(a1.isValid());
    assert(a2.isValid());
    assert(a1.assignmentProof() != a2.assignmentProof());
}

// Test 24: AuditAssignment with empty validator set returns invalid assignment.
void testAuditAssignmentEmptyValidatorsInvalid() {
    const AuditAssignment a = AuditAssignmentCalculator::buildAssignment(
        "assign-004", 100, 1,
        AuditAssignmentTargetType::BLOCK_ARTIFACT,
        "target-digest",
        "seed-abc",
        {}  // empty validator set
    );
    assert(!a.isValid());
}

} // namespace

int main() {
    testAuditAssignmentDeterministic();
    testAuditAssignmentChangesWithSeed();
    testAuditAssignmentChangesWithTarget();
    testAuditAssignmentEmptyValidatorsInvalid();
    return 0;
}
