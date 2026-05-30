#include "consensus/ValidatorVoteRecord.hpp"

#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/DevelopmentSignatureProvider.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::consensus {

namespace {

constexpr const char* VOTE_PAYLOAD_VERSION =
    "NODO_VALIDATOR_VOTE_PAYLOAD_V1";

bool isSafeScalar(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        if (character == ';' ||
            character == '{' ||
            character == '}' ||
            character == '[' ||
            character == ']' ||
            character == '\n' ||
            character == '\r' ||
            character == '\t') {
            return false;
        }
    }

    return true;
}

bool isValidatorAddressBoundToPublicKey(
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey
) {
    const crypto::Address address =
        crypto::Address::fromString(validatorAddress);

    if (!address.isValid()) {
        return false;
    }

    return crypto::AddressDerivation::verifyAddressForPublicKey(
        address,
        validatorPublicKey
    );
}

} // namespace

std::string validatorVoteDecisionToString(
    ValidatorVoteDecision decision
) {
    switch (decision) {
        case ValidatorVoteDecision::APPROVE:
            return "APPROVE";
        case ValidatorVoteDecision::REJECT:
            return "REJECT";
        case ValidatorVoteDecision::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

ValidatorVoteRecord::ValidatorVoteRecord()
    : m_validatorAddress(""),
      m_validatorPublicKey(),
      m_blockIndex(0),
      m_blockHash(""),
      m_previousHash(""),
      m_round(0),
      m_decision(ValidatorVoteDecision::UNKNOWN),
      m_reasonHash(""),
      m_createdAt(0),
      m_signatureBundle() {}

ValidatorVoteRecord::ValidatorVoteRecord(
    std::string validatorAddress,
    crypto::PublicKey validatorPublicKey,
    std::uint64_t blockIndex,
    std::string blockHash,
    std::string previousHash,
    std::uint64_t round,
    ValidatorVoteDecision decision,
    std::string reasonHash,
    std::int64_t createdAt,
    crypto::SignatureBundle signatureBundle
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_validatorPublicKey(std::move(validatorPublicKey)),
      m_blockIndex(blockIndex),
      m_blockHash(std::move(blockHash)),
      m_previousHash(std::move(previousHash)),
      m_round(round),
      m_decision(decision),
      m_reasonHash(std::move(reasonHash)),
      m_createdAt(createdAt),
      m_signatureBundle(std::move(signatureBundle)) {}

const std::string& ValidatorVoteRecord::validatorAddress() const {
    return m_validatorAddress;
}

const crypto::PublicKey& ValidatorVoteRecord::validatorPublicKey() const {
    return m_validatorPublicKey;
}

std::uint64_t ValidatorVoteRecord::blockIndex() const {
    return m_blockIndex;
}

const std::string& ValidatorVoteRecord::blockHash() const {
    return m_blockHash;
}

const std::string& ValidatorVoteRecord::previousHash() const {
    return m_previousHash;
}

std::uint64_t ValidatorVoteRecord::round() const {
    return m_round;
}

ValidatorVoteDecision ValidatorVoteRecord::decision() const {
    return m_decision;
}

const std::string& ValidatorVoteRecord::reasonHash() const {
    return m_reasonHash;
}

std::int64_t ValidatorVoteRecord::createdAt() const {
    return m_createdAt;
}

const crypto::SignatureBundle& ValidatorVoteRecord::signatureBundle() const {
    return m_signatureBundle;
}

std::string ValidatorVoteRecord::signingPayload() const {
    return buildSigningPayload(
        m_validatorAddress,
        m_validatorPublicKey,
        m_blockIndex,
        m_blockHash,
        m_previousHash,
        m_round,
        m_decision,
        m_reasonHash,
        m_createdAt
    );
}

bool ValidatorVoteRecord::matchesBlock(
    std::uint64_t blockIndex,
    const std::string& blockHash,
    std::uint64_t round
) const {
    return m_blockIndex == blockIndex &&
           m_blockHash == blockHash &&
           m_round == round;
}

bool ValidatorVoteRecord::isStructurallyValid(
    const crypto::CryptoPolicy& policy
) const {
    if (!isSafeScalar(m_validatorAddress) ||
        !isSafeScalar(m_blockHash) ||
        !isSafeScalar(m_previousHash) ||
        !isSafeScalar(m_reasonHash)) {
        return false;
    }

    if (!m_validatorPublicKey.isValid()) {
        return false;
    }

    if (!isValidatorAddressBoundToPublicKey(
            m_validatorAddress,
            m_validatorPublicKey
        )) {
        return false;
    }

    if (m_blockIndex == 0 ||
        m_round == 0 ||
        m_createdAt <= 0) {
        return false;
    }

    if (m_decision == ValidatorVoteDecision::UNKNOWN) {
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

    return true;
}

bool ValidatorVoteRecord::verify(
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) const {
    if (!isStructurallyValid(policy)) {
        return false;
    }

    return m_signatureBundle.verifyForPolicy(
        signingPayload(),
        policy,
        crypto::SecurityContext::VALIDATOR_OPERATION,
        provider
    );
}

std::string ValidatorVoteRecord::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorVoteRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";validatorPublicKey=" << m_validatorPublicKey.serialize()
        << ";validatorPublicKeyFingerprint=" << m_validatorPublicKey.fingerprint()
        << ";blockIndex=" << m_blockIndex
        << ";blockHash=" << m_blockHash
        << ";previousHash=" << m_previousHash
        << ";round=" << m_round
        << ";decision=" << validatorVoteDecisionToString(m_decision)
        << ";reasonHash=" << m_reasonHash
        << ";createdAt=" << m_createdAt
        << ";signatureBundle=" << m_signatureBundle.serialize()
        << "}";

    return oss.str();
}

std::string ValidatorVoteRecord::buildSigningPayload(
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey,
    std::uint64_t blockIndex,
    const std::string& blockHash,
    const std::string& previousHash,
    std::uint64_t round,
    ValidatorVoteDecision decision,
    const std::string& reasonHash,
    std::int64_t createdAt
) {
    if (!isSafeScalar(validatorAddress) ||
        !isSafeScalar(blockHash) ||
        !isSafeScalar(previousHash) ||
        !isSafeScalar(reasonHash)) {
        throw std::invalid_argument("Unsafe scalar rejected by ValidatorVoteRecord.");
    }

    if (!validatorPublicKey.isValid()) {
        throw std::invalid_argument("Validator vote public key is invalid.");
    }

    if (!isValidatorAddressBoundToPublicKey(
            validatorAddress,
            validatorPublicKey
        )) {
        throw std::invalid_argument("Validator vote address is not bound to public key.");
    }

    if (blockIndex == 0 ||
        round == 0 ||
        createdAt <= 0) {
        throw std::invalid_argument("Validator vote numeric fields are invalid.");
    }

    if (decision == ValidatorVoteDecision::UNKNOWN) {
        throw std::invalid_argument("Validator vote decision is UNKNOWN.");
    }

    std::ostringstream oss;

    /*
     * Versioned canonical payload.
     *
     * This is what validators actually sign. Keep the field order stable.
     */
    oss << "ValidatorVoteSigningPayload{"
        << "version=" << VOTE_PAYLOAD_VERSION
        << ";validatorAddress=" << validatorAddress
        << ";validatorPublicKey=" << validatorPublicKey.serialize()
        << ";validatorPublicKeyFingerprint=" << validatorPublicKey.fingerprint()
        << ";blockIndex=" << blockIndex
        << ";blockHash=" << blockHash
        << ";previousHash=" << previousHash
        << ";round=" << round
        << ";decision=" << validatorVoteDecisionToString(decision)
        << ";reasonHash=" << reasonHash
        << ";createdAt=" << createdAt
        << "}";

    return oss.str();
}

ValidatorVoteRecord ValidatorVoteRecord::createVote(
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey,
    const crypto::PrivateKey& validatorPrivateKey,
    std::uint64_t blockIndex,
    const std::string& blockHash,
    const std::string& previousHash,
    std::uint64_t round,
    ValidatorVoteDecision decision,
    const std::string& reasonHash,
    std::int64_t createdAt,
    const crypto::SignatureProvider& provider
) {
    const std::string payload =
        buildSigningPayload(
            validatorAddress,
            validatorPublicKey,
            blockIndex,
            blockHash,
            previousHash,
            round,
            decision,
            reasonHash,
            createdAt
        );

    crypto::SignatureBundle signatureBundle =
        crypto::SignatureBundle::createSignature(
            payload,
            validatorPublicKey,
            validatorPrivateKey,
            createdAt,
            provider
        );

    return ValidatorVoteRecord(
        validatorAddress,
        validatorPublicKey,
        blockIndex,
        blockHash,
        previousHash,
        round,
        decision,
        reasonHash,
        createdAt,
        signatureBundle
    );
}

ValidatorVoteRecord ValidatorVoteRecord::createDevelopmentVote(
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey,
    const crypto::PrivateKey& validatorPrivateKey,
    std::uint64_t blockIndex,
    const std::string& blockHash,
    const std::string& previousHash,
    std::uint64_t round,
    ValidatorVoteDecision decision,
    const std::string& reasonHash,
    std::int64_t createdAt
) {
    const crypto::DevelopmentSignatureProvider provider;

    return createVote(
        validatorAddress,
        validatorPublicKey,
        validatorPrivateKey,
        blockIndex,
        blockHash,
        previousHash,
        round,
        decision,
        reasonHash,
        createdAt,
        provider
    );
}

} // namespace nodo::consensus
