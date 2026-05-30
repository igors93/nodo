#ifndef NODO_CORE_VALIDATOR_PROPOSAL_ADMISSION_HPP
#define NODO_CORE_VALIDATOR_PROPOSAL_ADMISSION_HPP

#include "core/Blockchain.hpp"
#include "core/ValidatorBlockProposalSignature.hpp"
#include "core/ValidatorProposalRegistry.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <string>

namespace nodo::core {

/*
 * ValidatorProposalAdmissionStatus describes the full admission decision for a
 * signed block proposal.
 *
 * A proposal can have a valid signature and still be rejected if its validator
 * is not registered, not active, or not bound to the same public key used in the
 * signature.
 */
enum class ValidatorProposalAdmissionStatus {
    ACCEPTED,
    INVALID_BLOCKCHAIN,
    INVALID_VALIDATOR_REGISTRY,
    INVALID_SIGNED_PROPOSAL,
    UNREGISTERED_VALIDATOR,
    INACTIVE_VALIDATOR,
    VALIDATOR_PUBLIC_KEY_MISMATCH,
    INVALID_PROPOSAL_REGISTRY,
    REGISTRATION_FAILED,
    DOUBLE_SIGN_CONFLICT
};

std::string validatorProposalAdmissionStatusToString(
    ValidatorProposalAdmissionStatus status
);

/*
 * ValidatorProposalAdmissionResult is intentionally small and deterministic.
 *
 * It can be logged, serialized into diagnostics, and later turned into an
 * on-chain consensus/audit record if needed.
 */
class ValidatorProposalAdmissionResult {
public:
    ValidatorProposalAdmissionResult();

    static ValidatorProposalAdmissionResult accepted(
        std::string validatorAddress,
        std::string blockHash,
        std::uint64_t blockIndex
    );

    static ValidatorProposalAdmissionResult rejected(
        ValidatorProposalAdmissionStatus status,
        std::string reason
    );

    static ValidatorProposalAdmissionResult doubleSignConflict(
        std::string validatorAddress,
        std::uint64_t blockIndex,
        std::string blockHash,
        ValidatorDoubleSignEvidence evidence
    );

    ValidatorProposalAdmissionStatus status() const;
    const std::string& reason() const;
    const std::string& validatorAddress() const;
    const std::string& blockHash() const;
    std::uint64_t blockIndex() const;
    const ValidatorDoubleSignEvidence& doubleSignEvidence() const;

    bool accepted() const;
    bool rejected() const;
    bool conflictDetected() const;

    std::string serialize() const;

private:
    ValidatorProposalAdmissionStatus m_status;
    std::string m_reason;
    std::string m_validatorAddress;
    std::string m_blockHash;
    std::uint64_t m_blockIndex;
    ValidatorDoubleSignEvidence m_doubleSignEvidence;
};

/*
 * ValidatorProposalAdmissionPolicy is the bridge between:
 *
 *   SignedProtectionBlockProposal
 *   ValidatorRegistry
 *   ValidatorProposalRegistry
 *
 * Security principle:
 * Before a signed proposal can enter the proposal registry, the validator must
 * be registered, active, and using the same public key bound to its address.
 */
class ValidatorProposalAdmissionPolicy {
public:
    ValidatorProposalAdmissionResult validateSignedProposal(
        const SignedProtectionBlockProposal& signedProposal,
        const Blockchain& blockchain,
        const ValidatorRegistry& validatorRegistry,
        const crypto::CryptoPolicy& cryptoPolicy,
        const crypto::SignatureProvider& signatureProvider
    ) const;

    ValidatorProposalAdmissionResult admitAndRegisterSignedProposal(
        const SignedProtectionBlockProposal& signedProposal,
        const Blockchain& blockchain,
        const ValidatorRegistry& validatorRegistry,
        ValidatorProposalRegistry& proposalRegistry,
        const crypto::CryptoPolicy& cryptoPolicy,
        const crypto::SignatureProvider& signatureProvider
    ) const;
};

} // namespace nodo::core

#endif
