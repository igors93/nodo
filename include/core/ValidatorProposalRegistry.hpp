#ifndef NODO_CORE_VALIDATOR_PROPOSAL_REGISTRY_HPP
#define NODO_CORE_VALIDATOR_PROPOSAL_REGISTRY_HPP

#include "core/ValidatorBlockProposalSignature.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * ValidatorProposalRegistryEntry is the compact audit record for one verified
 * signed block proposal.
 *
 * Security principle:
 * Store the minimum fields needed to detect conflicting signatures without
 * keeping mutable references to the original proposal object.
 */
class ValidatorProposalRegistryEntry {
public:
    ValidatorProposalRegistryEntry();

    ValidatorProposalRegistryEntry(
        std::string validatorAddress,
        std::string validatorPublicKeyFingerprint,
        std::uint64_t blockIndex,
        std::string blockHash,
        std::string previousHash,
        std::uint64_t chainSizeBeforeProposal,
        std::string expectedPreviousHash,
        std::int64_t proposedAt,
        std::string signatureDigest
    );

    const std::string& validatorAddress() const;
    const std::string& validatorPublicKeyFingerprint() const;
    std::uint64_t blockIndex() const;
    const std::string& blockHash() const;
    const std::string& previousHash() const;
    std::uint64_t chainSizeBeforeProposal() const;
    const std::string& expectedPreviousHash() const;
    std::int64_t proposedAt() const;
    const std::string& signatureDigest() const;

    bool isValid() const;

    /*
     * Same proposal means same validator and same signed block target. We do
     * not treat a re-broadcast of the same block as double-signing.
     */
    bool sameProposalAs(const ValidatorProposalRegistryEntry& other) const;

    /*
     * Double-sign conflict means the same validator signed two different block
     * hashes for the same block height.
     */
    bool conflictsWith(const ValidatorProposalRegistryEntry& other) const;

    std::string serialize() const;

    static ValidatorProposalRegistryEntry fromSignedProposal(
        const SignedProtectionBlockProposal& signedProposal
    );

private:
    std::string m_validatorAddress;
    std::string m_validatorPublicKeyFingerprint;
    std::uint64_t m_blockIndex;
    std::string m_blockHash;
    std::string m_previousHash;
    std::uint64_t m_chainSizeBeforeProposal;
    std::string m_expectedPreviousHash;
    std::int64_t m_proposedAt;
    std::string m_signatureDigest;
};

class ValidatorDoubleSignEvidence {
public:
    ValidatorDoubleSignEvidence();

    ValidatorDoubleSignEvidence(
        ValidatorProposalRegistryEntry firstProposal,
        ValidatorProposalRegistryEntry conflictingProposal
    );

    const ValidatorProposalRegistryEntry& firstProposal() const;
    const ValidatorProposalRegistryEntry& conflictingProposal() const;

    const std::string& validatorAddress() const;
    std::uint64_t blockIndex() const;

    bool isValid() const;

    std::string serialize() const;

private:
    ValidatorProposalRegistryEntry m_firstProposal;
    ValidatorProposalRegistryEntry m_conflictingProposal;
};

enum class ValidatorProposalRegistrationStatus {
    ACCEPTED,
    DUPLICATE,
    DOUBLE_SIGN_CONFLICT,
    INVALID_PROPOSAL,
    INVALID_SIGNATURE,
    INVALID_REGISTRY
};

std::string validatorProposalRegistrationStatusToString(
    ValidatorProposalRegistrationStatus status
);

class ValidatorProposalRegistrationResult {
public:
    ValidatorProposalRegistrationResult();

    static ValidatorProposalRegistrationResult accepted(
        ValidatorProposalRegistryEntry entry
    );

    static ValidatorProposalRegistrationResult duplicate(
        ValidatorProposalRegistryEntry existingEntry
    );

    static ValidatorProposalRegistrationResult conflict(
        ValidatorDoubleSignEvidence evidence
    );

    static ValidatorProposalRegistrationResult invalid(
        ValidatorProposalRegistrationStatus status,
        std::string reason
    );

    ValidatorProposalRegistrationStatus status() const;
    const std::string& reason() const;
    const ValidatorProposalRegistryEntry& entry() const;
    const ValidatorDoubleSignEvidence& doubleSignEvidence() const;

    bool accepted() const;
    bool duplicate() const;
    bool conflictDetected() const;
    bool successWithoutConflict() const;

    std::string serialize() const;

private:
    ValidatorProposalRegistrationStatus m_status;
    std::string m_reason;
    ValidatorProposalRegistryEntry m_entry;
    ValidatorDoubleSignEvidence m_doubleSignEvidence;
};

/*
 * ValidatorProposalRegistry keeps the local set of verified signed proposals
 * and detects validator double-signing attempts.
 *
 * This is not yet consensus by itself. It is the safety layer future consensus
 * code can use before accepting votes or appending proposed blocks.
 */
class ValidatorProposalRegistry {
public:
    ValidatorProposalRegistry();

    ValidatorProposalRegistrationResult registerSignedProposal(
        const SignedProtectionBlockProposal& signedProposal,
        const Blockchain& blockchain,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    );

    const std::vector<ValidatorProposalRegistryEntry>& entries() const;
    const std::vector<ValidatorDoubleSignEvidence>& conflicts() const;

    bool hasProposal(
        const std::string& validatorAddress,
        std::uint64_t blockIndex,
        const std::string& blockHash
    ) const;

    std::size_t proposalCountForValidator(
        const std::string& validatorAddress
    ) const;

    std::size_t conflictCountForValidator(
        const std::string& validatorAddress
    ) const;

    bool hasDoubleSignConflict(
        const std::string& validatorAddress,
        std::uint64_t blockIndex
    ) const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::vector<ValidatorProposalRegistryEntry> m_entries;
    std::vector<ValidatorDoubleSignEvidence> m_conflicts;

    const ValidatorProposalRegistryEntry* findSameProposal(
        const ValidatorProposalRegistryEntry& candidate
    ) const;

    const ValidatorProposalRegistryEntry* findConflictingProposal(
        const ValidatorProposalRegistryEntry& candidate
    ) const;
};

} // namespace nodo::core

#endif
