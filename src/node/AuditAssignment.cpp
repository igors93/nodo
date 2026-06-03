#include "node/AuditAssignment.hpp"

#include <functional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

std::string auditAssignmentTargetTypeToString(AuditAssignmentTargetType type) {
    switch (type) {
        case AuditAssignmentTargetType::BLOCK_ARTIFACT:        return "BLOCK_ARTIFACT";
        case AuditAssignmentTargetType::TREASURY_SECTION:      return "TREASURY_SECTION";
        case AuditAssignmentTargetType::GOVERNANCE_LIFECYCLE:  return "GOVERNANCE_LIFECYCLE";
        case AuditAssignmentTargetType::VALIDATOR_STATE:       return "VALIDATOR_STATE";
        default:                                                return "UNKNOWN";
    }
}

AuditAssignment::AuditAssignment()
    : m_blockHeight(0),
      m_epoch(0),
      m_targetType(AuditAssignmentTargetType::BLOCK_ARTIFACT),
      m_valid(false),
      m_rejectionReason("AuditAssignment: default-constructed.") {}

AuditAssignment::AuditAssignment(
    std::string assignmentId,
    std::uint64_t blockHeight,
    std::uint64_t epoch,
    AuditAssignmentTargetType targetType,
    std::string targetId,
    std::string validatorAddress,
    std::string sourceSeedDigest,
    std::string assignmentProof
)
    : m_assignmentId(std::move(assignmentId)),
      m_blockHeight(blockHeight),
      m_epoch(epoch),
      m_targetType(targetType),
      m_targetId(std::move(targetId)),
      m_validatorAddress(std::move(validatorAddress)),
      m_sourceSeedDigest(std::move(sourceSeedDigest)),
      m_assignmentProof(std::move(assignmentProof)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_assignmentId.empty()) {
        m_rejectionReason = "AuditAssignment: assignmentId must not be empty.";
        return;
    }
    if (m_targetId.empty()) {
        m_rejectionReason = "AuditAssignment: targetId must not be empty.";
        return;
    }
    if (m_validatorAddress.empty()) {
        m_rejectionReason = "AuditAssignment: validatorAddress must not be empty.";
        return;
    }
    if (m_sourceSeedDigest.empty()) {
        m_rejectionReason = "AuditAssignment: sourceSeedDigest must not be empty.";
        return;
    }
    if (m_assignmentProof.empty()) {
        m_rejectionReason = "AuditAssignment: assignmentProof must not be empty.";
        return;
    }

    const std::string expectedProof = AuditAssignmentCalculator::buildAssignmentProof(
        m_sourceSeedDigest,
        m_blockHeight,
        m_epoch,
        m_targetType,
        m_targetId,
        m_validatorAddress
    );
    if (m_assignmentProof != expectedProof) {
        m_rejectionReason = "AuditAssignment: assignmentProof does not match fields.";
        return;
    }

    m_valid = true;
}

const std::string& AuditAssignment::assignmentId() const { return m_assignmentId; }
std::uint64_t AuditAssignment::blockHeight() const { return m_blockHeight; }
std::uint64_t AuditAssignment::epoch() const { return m_epoch; }
AuditAssignmentTargetType AuditAssignment::targetType() const { return m_targetType; }
const std::string& AuditAssignment::targetId() const { return m_targetId; }
const std::string& AuditAssignment::validatorAddress() const { return m_validatorAddress; }
const std::string& AuditAssignment::sourceSeedDigest() const { return m_sourceSeedDigest; }
const std::string& AuditAssignment::assignmentProof() const { return m_assignmentProof; }
bool AuditAssignment::isValid() const { return m_valid; }
const std::string& AuditAssignment::rejectionReason() const { return m_rejectionReason; }

std::string AuditAssignment::serialize() const {
    std::ostringstream oss;
    oss << "AuditAssignment{"
        << "assignmentId=" << m_assignmentId
        << ";blockHeight=" << m_blockHeight
        << ";epoch=" << m_epoch
        << ";targetType=" << auditAssignmentTargetTypeToString(m_targetType)
        << ";targetId=" << m_targetId
        << ";validatorAddress=" << m_validatorAddress
        << ";sourceSeedDigest=" << m_sourceSeedDigest
        << ";assignmentProof=" << m_assignmentProof
        << "}";
    return oss.str();
}

std::string AuditAssignmentCalculator::buildAssignmentProof(
    const std::string& sourceSeedDigest,
    std::uint64_t blockHeight,
    std::uint64_t epoch,
    AuditAssignmentTargetType targetType,
    const std::string& targetId,
    const std::string& validatorAddress
) {
    return "audit-assignment:"
        + sourceSeedDigest + ":"
        + std::to_string(blockHeight) + ":"
        + std::to_string(epoch) + ":"
        + auditAssignmentTargetTypeToString(targetType) + ":"
        + targetId + ":"
        + validatorAddress;
}

std::string AuditAssignmentCalculator::selectValidator(
    const std::string& sourceSeedDigest,
    const std::string& targetId,
    const std::vector<std::string>& validatorAddresses
) {
    if (validatorAddresses.empty()) {
        return "";
    }
    // Deterministic selection: hash seed + targetId to derive an index.
    // std::hash is not cryptographic but is deterministic for the same process.
    // Production P1 should replace this with a cryptographic VRF.
    const std::size_t combined =
        std::hash<std::string>{}(sourceSeedDigest + ":" + targetId);
    return validatorAddresses[combined % validatorAddresses.size()];
}

AuditAssignment AuditAssignmentCalculator::buildAssignment(
    const std::string& assignmentId,
    std::uint64_t blockHeight,
    std::uint64_t epoch,
    AuditAssignmentTargetType targetType,
    const std::string& targetId,
    const std::string& sourceSeedDigest,
    const std::vector<std::string>& validatorAddresses
) {
    if (validatorAddresses.empty()) {
        // Return a default-constructed (invalid) assignment rather than throwing,
        // so callers can detect the failure without exception handling.
        return AuditAssignment();
    }

    const std::string validatorAddress = selectValidator(
        sourceSeedDigest, targetId, validatorAddresses
    );

    const std::string proof = buildAssignmentProof(
        sourceSeedDigest, blockHeight, epoch, targetType, targetId, validatorAddress
    );

    return AuditAssignment(
        assignmentId,
        blockHeight,
        epoch,
        targetType,
        targetId,
        validatorAddress,
        sourceSeedDigest,
        proof
    );
}

} // namespace nodo::node
