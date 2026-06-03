#include "node/AuditAssignment.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::node::AuditAssignment;
using nodo::node::AuditAssignmentCalculator;
using nodo::node::AuditAssignmentTargetType;
using nodo::node::auditAssignmentTargetTypeToString;

void testValidAssignment() {
    const std::string seedDigest = "prev-block-hash-abc";
    const std::string targetId   = "artifact-001";
    const std::string validator  = "validator-addr-A";
    const std::uint64_t height   = 100;
    const std::uint64_t epoch    = 1;

    const std::string proof = AuditAssignmentCalculator::buildAssignmentProof(
        seedDigest, height, epoch,
        AuditAssignmentTargetType::BLOCK_ARTIFACT,
        targetId, validator
    );

    const AuditAssignment assignment(
        "assign-001", height, epoch,
        AuditAssignmentTargetType::BLOCK_ARTIFACT,
        targetId, validator, seedDigest, proof
    );

    assert(assignment.isValid());
    assert(assignment.assignmentId() == "assign-001");
    assert(assignment.blockHeight() == 100);
    assert(assignment.validatorAddress() == validator);
}

void testEmptyFieldsRejected() {
    const std::string seedDigest = "seed";
    const std::string targetId   = "target";
    const std::string validator  = "validator";
    const std::string proof = AuditAssignmentCalculator::buildAssignmentProof(
        seedDigest, 1, 0, AuditAssignmentTargetType::BLOCK_ARTIFACT, targetId, validator
    );

    const AuditAssignment no_id("", 1, 0, AuditAssignmentTargetType::BLOCK_ARTIFACT,
                                 targetId, validator, seedDigest, proof);
    assert(!no_id.isValid());

    const AuditAssignment no_target("assign", 1, 0, AuditAssignmentTargetType::BLOCK_ARTIFACT,
                                     "", validator, seedDigest, proof);
    assert(!no_target.isValid());

    const AuditAssignment no_validator("assign", 1, 0, AuditAssignmentTargetType::BLOCK_ARTIFACT,
                                        targetId, "", seedDigest, proof);
    assert(!no_validator.isValid());
}

void testForgedProofRejected() {
    const AuditAssignment bad(
        "assign-001", 100, 1,
        AuditAssignmentTargetType::BLOCK_ARTIFACT,
        "target", "validator", "seed", "forged-proof"
    );
    assert(!bad.isValid());
}

void testProofIsDeterministic() {
    const std::string p1 = AuditAssignmentCalculator::buildAssignmentProof(
        "seed", 100, 1, AuditAssignmentTargetType::BLOCK_ARTIFACT, "target", "validator"
    );
    const std::string p2 = AuditAssignmentCalculator::buildAssignmentProof(
        "seed", 100, 1, AuditAssignmentTargetType::BLOCK_ARTIFACT, "target", "validator"
    );
    assert(p1 == p2);
    assert(!p1.empty());
}

void testProofChangesWithInputs() {
    const std::string p1 = AuditAssignmentCalculator::buildAssignmentProof(
        "seed-a", 100, 1, AuditAssignmentTargetType::BLOCK_ARTIFACT, "target", "validator"
    );
    const std::string p2 = AuditAssignmentCalculator::buildAssignmentProof(
        "seed-b", 100, 1, AuditAssignmentTargetType::BLOCK_ARTIFACT, "target", "validator"
    );
    assert(p1 != p2);
}

void testSelectValidatorDeterministic() {
    const std::vector<std::string> validators = {"addr-A", "addr-B", "addr-C"};
    const std::string v1 = AuditAssignmentCalculator::selectValidator("seed", "target", validators);
    const std::string v2 = AuditAssignmentCalculator::selectValidator("seed", "target", validators);
    assert(v1 == v2);
    assert(!v1.empty());
    // Must be one of the validators.
    assert(v1 == "addr-A" || v1 == "addr-B" || v1 == "addr-C");
}

void testSelectValidatorEmptyList() {
    const std::string v = AuditAssignmentCalculator::selectValidator("seed", "target", {});
    assert(v.empty());
}

void testBuildAssignmentNoValidatorsReturnsInvalid() {
    // Empty validator set produces an invalid assignment (no throw).
    const AuditAssignment a = AuditAssignmentCalculator::buildAssignment(
        "assign", 1, 0,
        AuditAssignmentTargetType::TREASURY_SECTION,
        "target", "seed", {}
    );
    assert(!a.isValid());
}

void testBuildAssignmentValid() {
    const std::vector<std::string> validators = {"addr-A", "addr-B"};
    const AuditAssignment assignment = AuditAssignmentCalculator::buildAssignment(
        "assign-built", 50, 1,
        AuditAssignmentTargetType::GOVERNANCE_LIFECYCLE,
        "lifecycle-001", "prev-block-hash", validators
    );
    assert(assignment.isValid());
    assert(assignment.targetType() == AuditAssignmentTargetType::GOVERNANCE_LIFECYCLE);
}

void testTargetTypeToString() {
    assert(auditAssignmentTargetTypeToString(AuditAssignmentTargetType::BLOCK_ARTIFACT) == "BLOCK_ARTIFACT");
    assert(auditAssignmentTargetTypeToString(AuditAssignmentTargetType::TREASURY_SECTION) == "TREASURY_SECTION");
    assert(auditAssignmentTargetTypeToString(AuditAssignmentTargetType::GOVERNANCE_LIFECYCLE) == "GOVERNANCE_LIFECYCLE");
    assert(auditAssignmentTargetTypeToString(AuditAssignmentTargetType::VALIDATOR_STATE) == "VALIDATOR_STATE");
}

} // namespace

int main() {
    testValidAssignment();
    testEmptyFieldsRejected();
    testForgedProofRejected();
    testProofIsDeterministic();
    testProofChangesWithInputs();
    testSelectValidatorDeterministic();
    testSelectValidatorEmptyList();
    testBuildAssignmentNoValidatorsReturnsInvalid();
    testBuildAssignmentValid();
    testTargetTypeToString();
    return 0;
}
