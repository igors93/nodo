#include "crypto/OutofProcessSigner.hpp"

#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/Hex.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <blst.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::crypto {

namespace {

constexpr const char* SIGNER_STATE_VERSION =
    "NODO_OUT_OF_PROCESS_SIGNER_STATE_V1";
constexpr const char* EMPTY_HASH_SENTINEL = "NONE";
constexpr std::size_t BLS_PRIVATE_KEY_SIZE = 32;
constexpr std::size_t BLS_PUBLIC_KEY_SIZE = 48;

std::string encodeStoredHash(const std::string& value) {
    return value.empty() ? EMPTY_HASH_SENTINEL : value;
}

std::string decodeStoredHash(const std::string& value) {
    return value == EMPTY_HASH_SENTINEL ? "" : value;
}

bool requestIsValid(const SignatureRequest& request) {
    return request.height > 0 &&
           !request.proposalHash.empty() &&
           request.proposalHash != EMPTY_HASH_SENTINEL &&
           !request.payloadToSign.empty();
}

bool canSignAtWatermark(
    std::uint64_t lastHeight,
    std::uint32_t lastRound,
    const std::string& lastHash,
    const SignatureRequest& request
) {
    if (request.height < lastHeight) {
        return false;
    }

    if (request.height > lastHeight) {
        return true;
    }

    if (request.round < lastRound) {
        return false;
    }

    if (request.round == lastRound &&
        !lastHash.empty() &&
        request.proposalHash != lastHash) {
        return false;
    }

    return true;
}

PublicKey publicKeyFromBlsPrivateKeyMaterial(
    const std::string& privateKeyMaterial
) {
    if (!hasHexByteSize(privateKeyMaterial, BLS_PRIVATE_KEY_SIZE)) {
        throw std::invalid_argument("Out-of-process signer requires a BLS12-381 private key.");
    }

    const std::vector<unsigned char> privateBytes =
        hexDecode(privateKeyMaterial);

    blst_scalar scalar;
    blst_scalar_from_bendian(&scalar, privateBytes.data());

    if (!blst_sk_check(&scalar)) {
        throw std::invalid_argument("Out-of-process signer private key is not a valid BLS scalar.");
    }

    blst_p1 publicPoint;
    blst_sk_to_pk_in_g1(&publicPoint, &scalar);

    std::array<unsigned char, BLS_PUBLIC_KEY_SIZE> publicBytes = {};
    blst_p1_compress(publicBytes.data(), &publicPoint);

    return PublicKey(
        CryptoAlgorithm::BLS12_381,
        hexEncode(publicBytes.data(), publicBytes.size())
    );
}

std::int64_t signingTimestampFor(
    const SignatureRequest& request
) {
    const std::uint64_t boundedHeight =
        std::min<std::uint64_t>(
            request.height,
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - 1)
        );

    return static_cast<std::int64_t>(boundedHeight) + 1;
}

bool signPayload(
    const std::string& privateKeyMaterial,
    const PublicKey& publicKey,
    const SignatureRequest& request,
    SigningDomain domain,
    std::string& signatureHexOut
) {
    const Bls12381SignatureProvider provider;
    const PrivateKey privateKey(
        CryptoAlgorithm::BLS12_381,
        privateKeyMaterial
    );

    try {
        const SignatureBundle bundle =
            SignatureBundle::createSignature(
                request.payloadToSign,
                publicKey,
                privateKey,
                signingTimestampFor(request),
                provider,
                domain
            );

        if (bundle.signatures().size() != 1U) {
            return false;
        }

        const Signature& signature =
            bundle.signatures().front();

        if (!provider.verify(request.payloadToSign, signature).success()) {
            return false;
        }

        signatureHexOut = signature.signatureHex();
        return true;
    } catch (...) {
        signatureHexOut.clear();
        return false;
    }
}

} // namespace

OutofProcessSigner::OutofProcessSigner(
    const std::string& keyId,
    const std::string& encryptedKeyEnvelope,
    const std::string& password,
    std::filesystem::path stateFile,
    bool inMemoryTestMode
) : m_keyId(keyId),
    m_stateFile(std::move(stateFile)),
    m_lastSignedProposalHeight(0),
    m_lastSignedProposalRound(0),
    m_lastSignedVoteHeight(0),
    m_lastSignedVoteRound(0) {

    if (m_stateFile.empty() && !inMemoryTestMode) {
        throw std::invalid_argument("OutofProcessSigner requires a persistent state file for real use.");
    }

    if (!decryptKey(encryptedKeyEnvelope, password)) {
        throw std::runtime_error("Failed to decrypt validator private key: incorrect password or corrupted key envelope");
    }

    m_validatorPublicKey =
        publicKeyFromBlsPrivateKeyMaterial(m_decryptedPrivateKey);
    m_validatorAddress =
        AddressDerivation::deriveFromPublicKey(m_validatorPublicKey).value();

    if (!loadSigningState()) {
        throw std::runtime_error("Failed to load out-of-process signer state.");
    }
}

bool OutofProcessSigner::decryptKey(const std::string& envelope, const std::string& password) {
    m_decryptedPrivateKey = KeyEncryptionService::decrypt(m_keyId, envelope, password);
    return !m_decryptedPrivateKey.empty();
}

bool OutofProcessSigner::signBlockProposal(const SignatureRequest& request, std::string& signatureOut) {
    std::lock_guard<std::mutex> lock(m_mutex);
    signatureOut.clear();

    if (!requestIsValid(request) ||
        !canSignAtWatermark(
            m_lastSignedProposalHeight,
            m_lastSignedProposalRound,
            m_lastSignedProposalHash,
            request
        )) {
        return false;
    }

    std::string producedSignature;
    if (!signPayload(
            m_decryptedPrivateKey,
            m_validatorPublicKey,
            request,
            SigningDomain::VALIDATOR_BLOCK_PROPOSAL,
            producedSignature
        )) {
        return false;
    }

    const std::uint64_t previousHeight = m_lastSignedProposalHeight;
    const std::uint32_t previousRound = m_lastSignedProposalRound;
    const std::string previousHash = m_lastSignedProposalHash;
    m_lastSignedProposalHeight = request.height;
    m_lastSignedProposalRound = request.round;
    m_lastSignedProposalHash = request.proposalHash;

    if (!persistSigningState()) {
        m_lastSignedProposalHeight = previousHeight;
        m_lastSignedProposalRound = previousRound;
        m_lastSignedProposalHash = previousHash;
        return false;
    }

    signatureOut = std::move(producedSignature);
    return true;
}

bool OutofProcessSigner::signVote(const SignatureRequest& request, std::string& signatureOut) {
    std::lock_guard<std::mutex> lock(m_mutex);
    signatureOut.clear();

    if (!requestIsValid(request) ||
        !canSignAtWatermark(
            m_lastSignedVoteHeight,
            m_lastSignedVoteRound,
            m_lastSignedVoteHash,
            request
        )) {
        return false;
    }

    std::string producedSignature;
    if (!signPayload(
            m_decryptedPrivateKey,
            m_validatorPublicKey,
            request,
            SigningDomain::VALIDATOR_VOTE,
            producedSignature
        )) {
        return false;
    }

    const std::uint64_t previousHeight = m_lastSignedVoteHeight;
    const std::uint32_t previousRound = m_lastSignedVoteRound;
    const std::string previousHash = m_lastSignedVoteHash;
    m_lastSignedVoteHeight = request.height;
    m_lastSignedVoteRound = request.round;
    m_lastSignedVoteHash = request.proposalHash;

    if (!persistSigningState()) {
        m_lastSignedVoteHeight = previousHeight;
        m_lastSignedVoteRound = previousRound;
        m_lastSignedVoteHash = previousHash;
        return false;
    }

    signatureOut = std::move(producedSignature);
    return true;
}

const std::string& OutofProcessSigner::validatorAddress() const {
    return m_validatorAddress;
}

const PublicKey& OutofProcessSigner::validatorPublicKey() const {
    return m_validatorPublicKey;
}

bool OutofProcessSigner::loadSigningState() {
    if (m_stateFile.empty()) {
        return true;
    }

    if (!std::filesystem::exists(m_stateFile)) {
        return true;
    }

    try {
        const serialization::KeyValueFileDocument document =
            serialization::KeyValueFileCodec::parse(
                storage::AtomicFile::readTextFile(m_stateFile),
                SIGNER_STATE_VERSION
            );

        document.requireOnlyFields(
            {
                "proposalHeight",
                "proposalRound",
                "proposalHash",
                "voteHeight",
                "voteRound",
                "voteHash"
            }
        );

        m_lastSignedProposalHeight =
            static_cast<std::uint64_t>(std::stoull(document.requireField("proposalHeight")));
        m_lastSignedProposalRound =
            static_cast<std::uint32_t>(std::stoul(document.requireField("proposalRound")));
        m_lastSignedProposalHash =
            decodeStoredHash(document.requireField("proposalHash"));
        m_lastSignedVoteHeight =
            static_cast<std::uint64_t>(std::stoull(document.requireField("voteHeight")));
        m_lastSignedVoteRound =
            static_cast<std::uint32_t>(std::stoul(document.requireField("voteRound")));
        m_lastSignedVoteHash =
            decodeStoredHash(document.requireField("voteHash"));
    } catch (...) {
        return false;
    }

    return true;
}

bool OutofProcessSigner::persistSigningState() const {
    if (m_stateFile.empty()) {
        return true;
    }

    try {
        storage::AtomicFile::writeTextFile(
            m_stateFile,
            serialization::KeyValueFileCodec::serialize(
                SIGNER_STATE_VERSION,
                {
                    {"proposalHeight", std::to_string(m_lastSignedProposalHeight)},
                    {"proposalRound", std::to_string(m_lastSignedProposalRound)},
                    {"proposalHash", encodeStoredHash(m_lastSignedProposalHash)},
                    {"voteHeight", std::to_string(m_lastSignedVoteHeight)},
                    {"voteRound", std::to_string(m_lastSignedVoteRound)},
                    {"voteHash", encodeStoredHash(m_lastSignedVoteHash)}
                }
            )
        );
    } catch (...) {
        return false;
    }

    return true;
}

} // namespace nodo::crypto
