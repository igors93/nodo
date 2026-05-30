#include "core/ValidatorProposalAdmission.hpp"

#include <sstream>
#include <utility>

namespace nodo::core {

std::string validatorProposalAdmissionStatusToString(
    ValidatorProposalAdmissionStatus status
) {
    switch (status) {
        case ValidatorProposalAdmissionStatus::ACCEPTED:
            return "ACCEPTED";
        case ValidatorProposalAdmissionStatus::INVALID_BLOCKCHAIN:
            return "INVALID_BLOCKCHAIN";
        case ValidatorProposalAdmissionStatus::INVALID_VALIDATOR_REGISTRY:
            return "INVALID_VALIDATOR_REGISTRY";
        case ValidatorProposalAdmissionStatus::INVALID_SIGNED_PROPOSAL:
            return "INVALID_SIGNED_PROPOSAL";
        case ValidatorProposalAdmissionStatus::UNREGISTERED_VALIDATOR:
            return "UNREGISTERED_VALIDATOR";
        case ValidatorProposalAdmissionStatus::INACTIVE_VALIDATOR:
            return "INACTIVE_VALIDATOR";
        case ValidatorProposalAdmissionStatus::VALIDATOR_PUBLIC_KEY_MISMATCH:
            return "VALIDATOR_PUBLIC_KEY_MISMATCH";
        case ValidatorProposalAdmissionStatus::INVALID_PROPOSAL_REGISTRY:
            return "INVALID_PROPOSAL_REGISTRY";
        case ValidatorProposalAdmissionStatus::REGISTRATION_FAILED:
            return "REGISTRATION_FAILED";
        case ValidatorProposalAdmissionStatus::DOUBLE_SIGN_CONFLICT:
            return "DOUBLE_SIGN_CONFLICT";
        default:
            return "INVALID_SIGNED_PROPOSAL";
    }
}

ValidatorProposalAdmissionResult::ValidatorProposalAdmissionResult()
    : m_status(ValidatorProposalAdmissionStatus::INVALID_SIGNED_PROPOSAL),
      m_reason("Uninitialized validator proposal admission result."),
      m_validatorAddress(""),
      m_blockHash(""),
      m_blockIndex(0),
      m_doubleSignEvidence() {}

ValidatorProposalAdmissionResult ValidatorProposalAdmissionResult::accepted(
    std::string validatorAddress,
    std::string blockHash,
    std::uint64_t blockIndex
) {
    ValidatorProposalAdmissionResult result;
    result.m_status = ValidatorProposalAdmissionStatus::ACCEPTED;
    result.m_reason = "";
    result.m_validatorAddress = std::move(validatorAddress);
    result.m_blockHash = std::move(blockHash);
    result.m_blockIndex = blockIndex;
    return result;
}

ValidatorProposalAdmissionResult ValidatorProposalAdmissionResult::rejected(
    ValidatorProposalAdmissionStatus status,
    std::string reason
) {
    ValidatorProposalAdmissionResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

ValidatorProposalAdmissionResult ValidatorProposalAdmissionResult::doubleSignConflict(
    std::string validatorAddress,
    std::uint64_t blockIndex,
    std::string blockHash,
    ValidatorDoubleSignEvidence evidence
) {
    ValidatorProposalAdmissionResult result;
    result.m_status = ValidatorProposalAdmissionStatus::DOUBLE_SIGN_CONFLICT;
    result.m_reason = "Validator double-sign conflict detected.";
    result.m_validatorAddress = std::move(validatorAddress);
    result.m_blockIndex = blockIndex;
    result.m_blockHash = std::move(blockHash);
    result.m_doubleSignEvidence = std::move(evidence);
    return result;
}

ValidatorProposalAdmissionStatus ValidatorProposalAdmissionResult::status() const {
    return m_status;
}

const std::string& ValidatorProposalAdmissionResult::reason() const {
    return m_reason;
}

const std::string& ValidatorProposalAdmissionResult::validatorAddress() const {
    return m_validatorAddress;
}

const std::string& ValidatorProposalAdmissionResult::blockHash() const {
    return m_blockHash;
}

std::uint64_t ValidatorProposalAdmissionResult::blockIndex() const {
    return m_blockIndex;
}

const ValidatorDoubleSignEvidence& ValidatorProposalAdmissionResult::doubleSignEvidence() const {
    return m_doubleSignEvidence;
}

bool ValidatorProposalAdmissionResult::accepted() const {
    return m_status == ValidatorProposalAdmissionStatus::ACCEPTED;
}

bool ValidatorProposalAdmissionResult::rejected() const {
    return !accepted();
}

bool ValidatorProposalAdmissionResult::conflictDetected() const {
    return m_status == ValidatorProposalAdmissionStatus::DOUBLE_SIGN_CONFLICT;
}

std::string ValidatorProposalAdmissionResult::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorProposalAdmissionResult{"
        << "status=" << validatorProposalAdmissionStatusToString(m_status)
        << ";reason=" << m_reason
        << ";validatorAddress=" << m_validatorAddress
        << ";blockIndex=" << m_blockIndex
        << ";blockHash=" << m_blockHash
        << ";doubleSignEvidence="
        << (m_doubleSignEvidence.isValid() ? m_doubleSignEvidence.serialize() : "NONE")
        << "}";

    return oss.str();
}

ValidatorProposalAdmissionResult ValidatorProposalAdmissionPolicy::validateSignedProposal(
    const SignedProtectionBlockProposal& signedProposal,
    const Blockchain& blockchain,
    const ValidatorRegistry& validatorRegistry,
    const crypto::CryptoPolicy& cryptoPolicy,
    const crypto::SignatureProvider& signatureProvider
) const {
    if (blockchain.empty() || !blockchain.isValid()) {
        return ValidatorProposalAdmissionResult::rejected(
            ValidatorProposalAdmissionStatus::INVALID_BLOCKCHAIN,
            "Blockchain is empty or invalid."
        );
    }

    if (!validatorRegistry.isValid()) {
        return ValidatorProposalAdmissionResult::rejected(
            ValidatorProposalAdmissionStatus::INVALID_VALIDATOR_REGISTRY,
            "Validator registry is invalid."
        );
    }

    if (!signedProposal.isValidForBlockchain(
            blockchain,
            cryptoPolicy,
            signatureProvider
        )) {
        return ValidatorProposalAdmissionResult::rejected(
            ValidatorProposalAdmissionStatus::INVALID_SIGNED_PROPOSAL,
            "Signed proposal failed block/signature verification."
        );
    }

    const ValidatorBlockProposalSignature& proposalSignature =
        signedProposal.proposalSignature();

    const std::string& validatorAddress =
        proposalSignature.validatorAddress();

    const ValidatorRegistryEntry* validatorEntry =
        validatorRegistry.entryForAddress(validatorAddress);

    if (validatorEntry == nullptr) {
        return ValidatorProposalAdmissionResult::rejected(
            ValidatorProposalAdmissionStatus::UNREGISTERED_VALIDATOR,
            "Validator address is not registered."
        );
    }

    if (!validatorEntry->active()) {
        return ValidatorProposalAdmissionResult::rejected(
            ValidatorProposalAdmissionStatus::INACTIVE_VALIDATOR,
            "Validator is registered but not active."
        );
    }

    if (!validatorRegistry.verifyValidatorIdentity(
            validatorAddress,
            proposalSignature.validatorPublicKey()
        )) {
        return ValidatorProposalAdmissionResult::rejected(
            ValidatorProposalAdmissionStatus::VALIDATOR_PUBLIC_KEY_MISMATCH,
            "Signed proposal public key is not bound to the validator address."
        );
    }

    return ValidatorProposalAdmissionResult::accepted(
        validatorAddress,
        signedProposal.proposal().block().hash(),
        signedProposal.proposal().block().index()
    );
}

ValidatorProposalAdmissionResult ValidatorProposalAdmissionPolicy::admitAndRegisterSignedProposal(
    const SignedProtectionBlockProposal& signedProposal,
    const Blockchain& blockchain,
    const ValidatorRegistry& validatorRegistry,
    ValidatorProposalRegistry& proposalRegistry,
    const crypto::CryptoPolicy& cryptoPolicy,
    const crypto::SignatureProvider& signatureProvider
) const {
    if (!proposalRegistry.isValid()) {
        return ValidatorProposalAdmissionResult::rejected(
            ValidatorProposalAdmissionStatus::INVALID_PROPOSAL_REGISTRY,
            "Proposal registry is invalid before admission."
        );
    }

    const ValidatorProposalAdmissionResult validation =
        validateSignedProposal(
            signedProposal,
            blockchain,
            validatorRegistry,
            cryptoPolicy,
            signatureProvider
        );

    if (!validation.accepted()) {
        return validation;
    }

    const ValidatorProposalRegistrationResult registration =
        proposalRegistry.registerSignedProposal(
            signedProposal,
            blockchain,
            cryptoPolicy,
            signatureProvider
        );

    if (registration.conflictDetected()) {
        return ValidatorProposalAdmissionResult::doubleSignConflict(
            validation.validatorAddress(),
            validation.blockIndex(),
            validation.blockHash(),
            registration.doubleSignEvidence()
        );
    }

    if (!registration.successWithoutConflict()) {
        return ValidatorProposalAdmissionResult::rejected(
            ValidatorProposalAdmissionStatus::REGISTRATION_FAILED,
            registration.reason()
        );
    }

    return validation;
}

} // namespace nodo::core
