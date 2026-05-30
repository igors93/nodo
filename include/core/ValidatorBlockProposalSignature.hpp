#ifndef NODO_CORE_VALIDATOR_BLOCK_PROPOSAL_SIGNATURE_HPP
#define NODO_CORE_VALIDATOR_BLOCK_PROPOSAL_SIGNATURE_HPP

#include "core/ProtectionBlockProposal.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

/*
 * ValidatorBlockProposalSignature binds a block proposal to the validator that
 * proposed it.
 *
 * Security principle:
 * A proposal signature must commit to the exact chain tip, block hash, block
 * index, previous hash, ledger build result and validator identity.
 *
 * This prevents replaying a valid signature on another block or another chain
 * tip.
 */
class ValidatorBlockProposalSignature {
public:
    ValidatorBlockProposalSignature();

    ValidatorBlockProposalSignature(
        std::string validatorAddress,
        crypto::PublicKey validatorPublicKey,
        crypto::SignatureBundle signatureBundle,
        std::string signedBlockHash,
        std::uint64_t signedBlockIndex,
        std::string signedPreviousHash,
        std::uint64_t signedChainSizeBeforeProposal,
        std::string signedExpectedPreviousHash,
        std::int64_t proposedAt
    );

    const std::string& validatorAddress() const;
    const crypto::PublicKey& validatorPublicKey() const;
    const crypto::SignatureBundle& signatureBundle() const;
    const std::string& signedBlockHash() const;
    std::uint64_t signedBlockIndex() const;
    const std::string& signedPreviousHash() const;
    std::uint64_t signedChainSizeBeforeProposal() const;
    const std::string& signedExpectedPreviousHash() const;
    std::int64_t proposedAt() const;

    std::string signingPayload(
        const ProtectionBlockProposal& proposal
    ) const;

    bool matchesProposal(
        const ProtectionBlockProposal& proposal
    ) const;

    bool isStructurallyValid(
        const crypto::CryptoPolicy& policy
    ) const;

    bool verifyForProposal(
        const ProtectionBlockProposal& proposal,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    ) const;

    std::string serialize() const;

    static std::string buildSigningPayload(
        const ProtectionBlockProposal& proposal,
        const std::string& validatorAddress,
        const crypto::PublicKey& validatorPublicKey,
        std::int64_t proposedAt
    );

    static ValidatorBlockProposalSignature createSignature(
        const ProtectionBlockProposal& proposal,
        const std::string& validatorAddress,
        const crypto::PublicKey& validatorPublicKey,
        const crypto::PrivateKey& validatorPrivateKey,
        std::int64_t proposedAt,
        const crypto::SignatureProvider& provider
    );

    static ValidatorBlockProposalSignature createDevelopmentSignature(
        const ProtectionBlockProposal& proposal,
        const std::string& validatorAddress,
        const crypto::PublicKey& validatorPublicKey,
        const crypto::PrivateKey& validatorPrivateKey,
        std::int64_t proposedAt
    );

private:
    std::string m_validatorAddress;
    crypto::PublicKey m_validatorPublicKey;
    crypto::SignatureBundle m_signatureBundle;
    std::string m_signedBlockHash;
    std::uint64_t m_signedBlockIndex;
    std::string m_signedPreviousHash;
    std::uint64_t m_signedChainSizeBeforeProposal;
    std::string m_signedExpectedPreviousHash;
    std::int64_t m_proposedAt;
};

/*
 * SignedProtectionBlockProposal is the object future consensus code should
 * verify before accepting a proposed reward block.
 *
 * It keeps the unsigned proposal and its validator signature together.
 */
class SignedProtectionBlockProposal {
public:
    SignedProtectionBlockProposal(
        ProtectionBlockProposal proposal,
        ValidatorBlockProposalSignature proposalSignature
    );

    const ProtectionBlockProposal& proposal() const;
    const ValidatorBlockProposalSignature& proposalSignature() const;

    bool isValidForBlockchain(
        const Blockchain& blockchain,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    ) const;

    void appendToBlockchain(
        Blockchain& blockchain,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    ) const;

    std::string serialize() const;

private:
    ProtectionBlockProposal m_proposal;
    ValidatorBlockProposalSignature m_proposalSignature;
};

/*
 * ValidatorBlockProposalSigner is a small helper that keeps proposal signing
 * code out of consensus/proposal construction code.
 */
class ValidatorBlockProposalSigner {
public:
    static SignedProtectionBlockProposal signProposal(
        const ProtectionBlockProposal& proposal,
        const std::string& validatorAddress,
        const crypto::PublicKey& validatorPublicKey,
        const crypto::PrivateKey& validatorPrivateKey,
        std::int64_t proposedAt,
        const crypto::SignatureProvider& provider
    );

    static SignedProtectionBlockProposal signProposalForDevelopment(
        const ProtectionBlockProposal& proposal,
        const std::string& validatorAddress,
        const crypto::PublicKey& validatorPublicKey,
        const crypto::PrivateKey& validatorPrivateKey,
        std::int64_t proposedAt
    );
};

} // namespace nodo::core

#endif
