#include "core/ValidatorBlockProposalSignature.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

namespace {

constexpr const char* PROPOSAL_SIGNATURE_PAYLOAD_VERSION =
    "NODO_VALIDATOR_BLOCK_PROPOSAL_SIGNATURE_V1";

bool isSafeAddress(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        if (character == ';' ||
            character == '{' ||
            character == '}' ||
            character == '\n' ||
            character == '\r' ||
            character == '\t') {
            return false;
        }
    }

    return true;
}

} // namespace

ValidatorBlockProposalSignature::ValidatorBlockProposalSignature()
    : m_validatorAddress(""),
      m_validatorPublicKey(),
      m_signatureBundle(),
      m_signedBlockHash(""),
      m_signedBlockIndex(0),
      m_signedPreviousHash(""),
      m_signedChainSizeBeforeProposal(0),
      m_signedExpectedPreviousHash(""),
      m_proposedAt(0) {}

ValidatorBlockProposalSignature::ValidatorBlockProposalSignature(
    std::string validatorAddress,
    crypto::PublicKey validatorPublicKey,
    crypto::SignatureBundle signatureBundle,
    std::string signedBlockHash,
    std::uint64_t signedBlockIndex,
    std::string signedPreviousHash,
    std::uint64_t signedChainSizeBeforeProposal,
    std::string signedExpectedPreviousHash,
    std::int64_t proposedAt
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_validatorPublicKey(std::move(validatorPublicKey)),
      m_signatureBundle(std::move(signatureBundle)),
      m_signedBlockHash(std::move(signedBlockHash)),
      m_signedBlockIndex(signedBlockIndex),
      m_signedPreviousHash(std::move(signedPreviousHash)),
      m_signedChainSizeBeforeProposal(signedChainSizeBeforeProposal),
      m_signedExpectedPreviousHash(std::move(signedExpectedPreviousHash)),
      m_proposedAt(proposedAt) {}

const std::string& ValidatorBlockProposalSignature::validatorAddress() const {
    return m_validatorAddress;
}

const crypto::PublicKey& ValidatorBlockProposalSignature::validatorPublicKey() const {
    return m_validatorPublicKey;
}

const crypto::SignatureBundle& ValidatorBlockProposalSignature::signatureBundle() const {
    return m_signatureBundle;
}

const std::string& ValidatorBlockProposalSignature::signedBlockHash() const {
    return m_signedBlockHash;
}

std::uint64_t ValidatorBlockProposalSignature::signedBlockIndex() const {
    return m_signedBlockIndex;
}

const std::string& ValidatorBlockProposalSignature::signedPreviousHash() const {
    return m_signedPreviousHash;
}

std::uint64_t ValidatorBlockProposalSignature::signedChainSizeBeforeProposal() const {
    return m_signedChainSizeBeforeProposal;
}

const std::string& ValidatorBlockProposalSignature::signedExpectedPreviousHash() const {
    return m_signedExpectedPreviousHash;
}

std::int64_t ValidatorBlockProposalSignature::proposedAt() const {
    return m_proposedAt;
}

std::string ValidatorBlockProposalSignature::signingPayload(
    const ProtectionBlockProposal& proposal
) const {
    return buildSigningPayload(
        proposal,
        m_validatorAddress,
        m_validatorPublicKey,
        m_proposedAt
    );
}

bool ValidatorBlockProposalSignature::matchesProposal(
    const ProtectionBlockProposal& proposal
) const {
    if (m_signedBlockHash != proposal.block().hash()) {
        return false;
    }

    if (m_signedBlockIndex != proposal.block().index()) {
        return false;
    }

    if (m_signedPreviousHash != proposal.block().previousHash()) {
        return false;
    }

    if (m_signedChainSizeBeforeProposal != proposal.chainSizeBeforeProposal()) {
        return false;
    }

    if (m_signedExpectedPreviousHash != proposal.expectedPreviousHash()) {
        return false;
    }

    return true;
}

bool ValidatorBlockProposalSignature::isStructurallyValid(
    const crypto::CryptoPolicy& policy
) const {
    if (!isSafeAddress(m_validatorAddress)) {
        return false;
    }

    if (!m_validatorPublicKey.isValid()) {
        return false;
    }

    if (m_signatureBundle.empty()) {
        return false;
    }

    if (!m_signatureBundle.isValidForPolicy(
            policy,
            crypto::SecurityContext::VALIDATOR_OPERATION
        )) {
        return false;
    }

    if (m_signedBlockHash.empty() ||
        m_signedPreviousHash.empty() ||
        m_signedExpectedPreviousHash.empty()) {
        return false;
    }

    if (m_proposedAt <= 0) {
        return false;
    }

    return true;
}

bool ValidatorBlockProposalSignature::verifyForProposal(
    const ProtectionBlockProposal& proposal,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) const {
    if (!matchesProposal(proposal)) {
        return false;
    }

    if (!isStructurallyValid(policy)) {
        return false;
    }

    /*
     * Provider verification is intentionally separate from structural policy
     * validation. A signature that looks allowed by policy must still verify
     * against the exact proposal payload.
     */
    return m_signatureBundle.verifyForPolicy(
        signingPayload(proposal),
        policy,
        crypto::SecurityContext::VALIDATOR_OPERATION,
        provider
    );
}

std::string ValidatorBlockProposalSignature::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorBlockProposalSignature{"
        << "validator=" << m_validatorAddress
        << ";publicKey=" << m_validatorPublicKey.serialize()
        << ";signedBlockHash=" << m_signedBlockHash
        << ";signedBlockIndex=" << m_signedBlockIndex
        << ";signedPreviousHash=" << m_signedPreviousHash
        << ";signedChainSizeBeforeProposal=" << m_signedChainSizeBeforeProposal
        << ";signedExpectedPreviousHash=" << m_signedExpectedPreviousHash
        << ";proposedAt=" << m_proposedAt
        << ";signatureBundle=" << m_signatureBundle.serialize()
        << "}";

    return oss.str();
}

std::string ValidatorBlockProposalSignature::buildSigningPayload(
    const ProtectionBlockProposal& proposal,
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey,
    std::int64_t proposedAt
) {
    if (!isSafeAddress(validatorAddress)) {
        throw std::invalid_argument("Validator address is unsafe for proposal signing.");
    }

    if (!validatorPublicKey.isValid()) {
        throw std::invalid_argument("Validator public key is invalid.");
    }

    if (proposedAt <= 0) {
        throw std::invalid_argument("Proposal signature timestamp must be positive.");
    }

    std::ostringstream oss;

    /*
     * Canonical payload:
     * Keep this deterministic and versioned. Changing this format changes what
     * validators are actually signing.
     */
    oss << "ValidatorBlockProposalSigningPayload{"
        << "version=" << PROPOSAL_SIGNATURE_PAYLOAD_VERSION
        << ";validator=" << validatorAddress
        << ";validatorPublicKey=" << validatorPublicKey.serialize()
        << ";validatorPublicKeyFingerprint=" << validatorPublicKey.fingerprint()
        << ";blockIndex=" << proposal.block().index()
        << ";blockHash=" << proposal.block().hash()
        << ";previousHash=" << proposal.block().previousHash()
        << ";chainSizeBeforeProposal=" << proposal.chainSizeBeforeProposal()
        << ";expectedPreviousHash=" << proposal.expectedPreviousHash()
        << ";ledgerBuildResult=" << proposal.ledgerBuildResult().serialize()
        << ";proposedAt=" << proposedAt
        << "}";

    return oss.str();
}

ValidatorBlockProposalSignature ValidatorBlockProposalSignature::createSignature(
    const ProtectionBlockProposal& proposal,
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey,
    const crypto::PrivateKey& validatorPrivateKey,
    std::int64_t proposedAt,
    const crypto::SignatureProvider& provider
) {
    const std::string payload =
        buildSigningPayload(
            proposal,
            validatorAddress,
            validatorPublicKey,
            proposedAt
        );

    crypto::SignatureBundle signatureBundle =
        crypto::SignatureBundle::createSignature(
            payload,
            validatorPublicKey,
            validatorPrivateKey,
            proposedAt,
            provider,
            crypto::SigningDomain::VALIDATOR_BLOCK_PROPOSAL
        );

    ValidatorBlockProposalSignature signature(
        validatorAddress,
        validatorPublicKey,
        signatureBundle,
        proposal.block().hash(),
        proposal.block().index(),
        proposal.block().previousHash(),
        proposal.chainSizeBeforeProposal(),
        proposal.expectedPreviousHash(),
        proposedAt
    );

    if (!signature.matchesProposal(proposal)) {
        throw std::logic_error("Created proposal signature does not match proposal.");
    }

    return signature;
}

SignedProtectionBlockProposal::SignedProtectionBlockProposal(
    ProtectionBlockProposal proposal,
    ValidatorBlockProposalSignature proposalSignature
)
    : m_proposal(std::move(proposal)),
      m_proposalSignature(std::move(proposalSignature)) {}

const ProtectionBlockProposal& SignedProtectionBlockProposal::proposal() const {
    return m_proposal;
}

const ValidatorBlockProposalSignature& SignedProtectionBlockProposal::proposalSignature() const {
    return m_proposalSignature;
}

bool SignedProtectionBlockProposal::isValidForBlockchain(
    const Blockchain& blockchain,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) const {
    if (!m_proposal.isValidForBlockchain(blockchain)) {
        return false;
    }

    return m_proposalSignature.verifyForProposal(
        m_proposal,
        policy,
        provider
    );
}

void SignedProtectionBlockProposal::appendToBlockchain(
    Blockchain& blockchain,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) const {
    if (!isValidForBlockchain(blockchain, policy, provider)) {
        throw std::invalid_argument("Invalid signed protection block proposal rejected.");
    }

    m_proposal.appendToBlockchain(blockchain);
}

std::string SignedProtectionBlockProposal::serialize() const {
    std::ostringstream oss;

    oss << "SignedProtectionBlockProposal{"
        << "proposal=" << m_proposal.serialize()
        << ";proposalSignature=" << m_proposalSignature.serialize()
        << "}";

    return oss.str();
}

SignedProtectionBlockProposal ValidatorBlockProposalSigner::signProposal(
    const ProtectionBlockProposal& proposal,
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey,
    const crypto::PrivateKey& validatorPrivateKey,
    std::int64_t proposedAt,
    const crypto::SignatureProvider& provider
) {
    ValidatorBlockProposalSignature signature =
        ValidatorBlockProposalSignature::createSignature(
            proposal,
            validatorAddress,
            validatorPublicKey,
            validatorPrivateKey,
            proposedAt,
            provider
        );

    return SignedProtectionBlockProposal(
        proposal,
        signature
    );
}

} // namespace nodo::core
