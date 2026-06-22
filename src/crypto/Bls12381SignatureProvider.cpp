#include "crypto/Bls12381SignatureProvider.hpp"

#include "crypto/Hex.hpp"
#include "crypto/SigningPayload.hpp"
#include "crypto/hash.h"

#include <blst.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace nodo::crypto {

namespace {

constexpr std::size_t BLS_PRIVATE_KEY_SIZE = 32;
constexpr std::size_t BLS_PUBLIC_KEY_SIZE = 48;
constexpr std::size_t BLS_SIGNATURE_SIZE = 96;

std::vector<unsigned char> requireHexBytes(
    const std::string& hex,
    std::size_t expectedSize,
    const std::string& label
) {
    if (!hasHexByteSize(hex, expectedSize)) {
        throw std::invalid_argument(label + " has invalid hex size.");
    }

    return hexDecode(hex);
}

std::string bindMessage(
    const std::string& message,
    SigningDomain domain
) {
    return SigningPayload(
        CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        domain,
        message
    ).canonicalMessage();
}

std::string blsDst(
    SigningDomain domain
) {
    if (domain == SigningDomain::UNKNOWN) {
        throw std::invalid_argument("BLS domain cannot be UNKNOWN.");
    }

    return "NODO_BLS12381G2_XMD:SHA-256_SSWU_RO_"
        + signingDomainToString(domain);
}

blst_scalar scalarFromPrivateKey(
    const std::vector<unsigned char>& privateBytes
) {
    blst_scalar scalar;
    blst_scalar_from_bendian(&scalar, privateBytes.data());

    if (!blst_sk_check(&scalar)) {
        throw std::invalid_argument("BLS12-381 private key is invalid.");
    }

    return scalar;
}

std::string publicKeyFromScalar(
    const blst_scalar& scalar
) {
    blst_p1 publicPoint;
    blst_sk_to_pk_in_g1(&publicPoint, &scalar);

    std::array<unsigned char, BLS_PUBLIC_KEY_SIZE> output = {};
    blst_p1_compress(output.data(), &publicPoint);

    return hexEncode(output.data(), output.size());
}

std::array<unsigned char, BLS_PRIVATE_KEY_SIZE> randomIkm() {
    std::array<unsigned char, BLS_PRIVATE_KEY_SIZE> ikm = {};

    if (RAND_bytes(ikm.data(), static_cast<int>(ikm.size())) != 1) {
        throw std::runtime_error("OpenSSL RAND_bytes failed for BLS key generation.");
    }

    return ikm;
}

std::array<unsigned char, BLS_PRIVATE_KEY_SIZE> seedToIkm(
    const std::string& seed
) {
    if (seed.empty()) {
        throw std::invalid_argument("BLS deterministic seed cannot be empty.");
    }

    char hash[NODO_HASH_BUFFER_SIZE] = {0};
    const std::string material =
        "NODO_BLS12381_TEST_KEY_SEED_V1|" + seed;
    nodo_hash_string(material.c_str(), hash, sizeof(hash));

    const std::vector<unsigned char> bytes =
        hexDecode(std::string(hash));

    std::array<unsigned char, BLS_PRIVATE_KEY_SIZE> output = {};
    std::copy(
        bytes.begin(),
        bytes.begin() + static_cast<std::ptrdiff_t>(output.size()),
        output.begin()
    );

    return output;
}

KeyPair keyPairFromIkm(
    const std::array<unsigned char, BLS_PRIVATE_KEY_SIZE>& ikm
) {
    static constexpr unsigned char kInfo[] =
        "NODO_BLS12381_KEYGEN_V1";

    blst_scalar scalar;
    blst_keygen(
        &scalar,
        ikm.data(),
        ikm.size(),
        kInfo,
        sizeof(kInfo) - 1U
    );

    if (!blst_sk_check(&scalar)) {
        throw std::runtime_error("blst generated an invalid BLS private key.");
    }

    std::array<unsigned char, BLS_PRIVATE_KEY_SIZE> privateBytes = {};
    blst_bendian_from_scalar(privateBytes.data(), &scalar);

    return KeyPair(
        PublicKey(
            CryptoAlgorithm::BLS12_381,
            publicKeyFromScalar(scalar)
        ),
        PrivateKey(
            CryptoAlgorithm::BLS12_381,
            hexEncode(privateBytes.data(), privateBytes.size())
        )
    );
}

} // namespace

CryptoAlgorithm Bls12381SignatureProvider::algorithm() const {
    return CryptoAlgorithm::BLS12_381;
}

Signature Bls12381SignatureProvider::sign(
    const std::string& message,
    const PublicKey& publicKey,
    const PrivateKey& privateKey,
    std::int64_t timestamp,
    SigningDomain domain
) const {
    if (message.empty()) {
        throw std::invalid_argument("BLS signature message cannot be empty.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("BLS signature timestamp must be positive.");
    }

    if (domain == SigningDomain::UNKNOWN) {
        throw std::invalid_argument("BLS signature domain cannot be UNKNOWN.");
    }

    if (publicKey.algorithm() != algorithm() ||
        privateKey.algorithm() != algorithm()) {
        throw std::invalid_argument("BLS signature requires BLS12-381 keys.");
    }

    const std::vector<unsigned char> privateBytes =
        requireHexBytes(
            privateKey.keyMaterialForSigningOnly(),
            BLS_PRIVATE_KEY_SIZE,
            "BLS private key"
        );

    const blst_scalar scalar =
        scalarFromPrivateKey(privateBytes);

    if (publicKey.keyMaterial() != publicKeyFromScalar(scalar)) {
        throw std::invalid_argument("BLS public key does not match private key.");
    }

    const std::string canonicalMessage =
        bindMessage(message, domain);
    const std::string dst =
        blsDst(domain);

    blst_p2 hashPoint;
    blst_hash_to_g2(
        &hashPoint,
        reinterpret_cast<const unsigned char*>(canonicalMessage.data()),
        canonicalMessage.size(),
        reinterpret_cast<const unsigned char*>(dst.data()),
        dst.size(),
        nullptr,
        0
    );

    blst_p2 signaturePoint;
    blst_sign_pk_in_g1(&signaturePoint, &hashPoint, &scalar);

    std::array<unsigned char, BLS_SIGNATURE_SIZE> signatureBytes = {};
    blst_p2_compress(signatureBytes.data(), &signaturePoint);

    return Signature(
        CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        domain,
        algorithm(),
        publicKey,
        hexEncode(signatureBytes.data(), signatureBytes.size()),
        timestamp
    );
}

SignatureVerificationResult Bls12381SignatureProvider::verify(
    const std::string& message,
    const Signature& signature
) const {
    if (message.empty()) {
        return SignatureVerificationResult::invalid("Message is empty.");
    }

    if (!signature.isValid()) {
        return SignatureVerificationResult::invalid("Signature is structurally invalid.");
    }

    if (signature.algorithm() != algorithm() ||
        signature.publicKey().algorithm() != algorithm()) {
        return SignatureVerificationResult::invalid("Signature is not BLS12-381.");
    }

    try {
        const std::vector<unsigned char> publicBytes =
            requireHexBytes(
                signature.publicKey().keyMaterial(),
                BLS_PUBLIC_KEY_SIZE,
                "BLS public key"
            );

        const std::vector<unsigned char> signatureBytes =
            requireHexBytes(
                signature.signatureHex(),
                BLS_SIGNATURE_SIZE,
                "BLS signature"
            );

        blst_p1_affine publicKey;
        if (blst_p1_uncompress(&publicKey, publicBytes.data()) != BLST_SUCCESS ||
            !blst_p1_affine_in_g1(&publicKey) ||
            blst_p1_affine_is_inf(&publicKey)) {
            return SignatureVerificationResult::invalid("BLS public key is invalid.");
        }

        blst_p2_affine signaturePoint;
        if (blst_p2_uncompress(&signaturePoint, signatureBytes.data()) != BLST_SUCCESS ||
            !blst_p2_affine_in_g2(&signaturePoint) ||
            blst_p2_affine_is_inf(&signaturePoint)) {
            return SignatureVerificationResult::invalid("BLS signature point is invalid.");
        }

        const std::string canonicalMessage =
            bindMessage(message, signature.domain());
        const std::string dst =
            blsDst(signature.domain());

        const BLST_ERROR result =
            blst_core_verify_pk_in_g1(
                &publicKey,
                &signaturePoint,
                true,
                reinterpret_cast<const unsigned char*>(canonicalMessage.data()),
                canonicalMessage.size(),
                reinterpret_cast<const unsigned char*>(dst.data()),
                dst.size(),
                nullptr,
                0
            );

        if (result != BLST_SUCCESS) {
            return SignatureVerificationResult::invalid("BLS signature mismatch.");
        }
    } catch (const std::exception& error) {
        return SignatureVerificationResult::invalid(error.what());
    }

    return SignatureVerificationResult::valid();
}

KeyPair Bls12381SignatureProvider::generateKeyPair() {
    return keyPairFromIkm(randomIkm());
}

KeyPair Bls12381SignatureProvider::deriveKeyPairFromSeed(
    const std::string& seed
) {
    return keyPairFromIkm(seedToIkm(seed));
}

bool Bls12381SignatureProvider::isValidPublicKeyMaterial(
    const std::string& keyMaterial
) {
    if (!hasHexByteSize(keyMaterial, BLS_PUBLIC_KEY_SIZE)) {
        return false;
    }

    try {
        const std::vector<unsigned char> bytes =
            hexDecode(keyMaterial);
        blst_p1_affine publicKey;
        return blst_p1_uncompress(&publicKey, bytes.data()) == BLST_SUCCESS &&
               blst_p1_affine_in_g1(&publicKey) &&
               !blst_p1_affine_is_inf(&publicKey);
    } catch (...) {
        return false;
    }
}

bool Bls12381SignatureProvider::isValidPrivateKeyMaterial(
    const std::string& keyMaterial
) {
    if (!hasHexByteSize(keyMaterial, BLS_PRIVATE_KEY_SIZE)) {
        return false;
    }

    try {
        const std::vector<unsigned char> bytes =
            hexDecode(keyMaterial);
        (void)scalarFromPrivateKey(bytes);
        return true;
    } catch (...) {
        return false;
    }
}

Signature Bls12381SignatureProvider::aggregateSignatures(
    const std::vector<Signature>& signatures,
    std::int64_t createdAt
) {
    if (signatures.empty() || createdAt <= 0) {
        throw std::invalid_argument("BLS aggregate signature input is invalid.");
    }

    const Signature& first = signatures.front();

    blst_p2 aggregateSigPoint;
    blst_p1 aggregatePubKeyPoint;
    bool initialized = false;

    for (const Signature& signature : signatures) {
        if (!signature.isValid() ||
            signature.suite() != first.suite() ||
            signature.domain() != first.domain() ||
            signature.algorithm() != CryptoAlgorithm::BLS12_381) {
            throw std::invalid_argument("BLS aggregate signature contains incompatible signatures.");
        }

        // Aggregate the signature points (G2).
        const std::vector<unsigned char> signatureBytes =
            requireHexBytes(signature.signatureHex(), BLS_SIGNATURE_SIZE, "BLS signature");

        blst_p2_affine sigAffine;
        if (blst_p2_uncompress(&sigAffine, signatureBytes.data()) != BLST_SUCCESS ||
            !blst_p2_affine_in_g2(&sigAffine) ||
            blst_p2_affine_is_inf(&sigAffine)) {
            throw std::invalid_argument("BLS aggregate signature contains an invalid signature point.");
        }

        // Aggregate the public key points (G1). The aggregate public key is
        // required for correct pairing verification of the aggregate signature —
        // storing only the first signer's key would yield a verification mismatch
        // and enables identity spoofing in quorum accounting.
        const std::vector<unsigned char> pubKeyBytes =
            requireHexBytes(signature.publicKey().keyMaterial(), BLS_PUBLIC_KEY_SIZE, "BLS public key");

        blst_p1_affine pubKeyAffine;
        if (blst_p1_uncompress(&pubKeyAffine, pubKeyBytes.data()) != BLST_SUCCESS ||
            !blst_p1_affine_in_g1(&pubKeyAffine)) {
            throw std::invalid_argument("BLS aggregate signature contains an invalid public key point.");
        }

        if (!initialized) {
            blst_p2_from_affine(&aggregateSigPoint, &sigAffine);
            blst_p1_from_affine(&aggregatePubKeyPoint, &pubKeyAffine);
            initialized = true;
        } else {
            blst_p2_add_or_double_affine(&aggregateSigPoint, &aggregateSigPoint, &sigAffine);
            blst_p1_add_or_double_affine(&aggregatePubKeyPoint, &aggregatePubKeyPoint, &pubKeyAffine);
        }
    }

    std::array<unsigned char, BLS_SIGNATURE_SIZE> sigOutput = {};
    blst_p2_compress(sigOutput.data(), &aggregateSigPoint);

    std::array<unsigned char, BLS_PUBLIC_KEY_SIZE> pubKeyOutput = {};
    blst_p1_compress(pubKeyOutput.data(), &aggregatePubKeyPoint);

    const PublicKey aggregatePublicKey(
        CryptoAlgorithm::BLS12_381,
        hexEncode(pubKeyOutput.data(), pubKeyOutput.size())
    );

    return Signature(
        first.suite(),
        first.domain(),
        CryptoAlgorithm::BLS12_381,
        aggregatePublicKey,
        hexEncode(sigOutput.data(), sigOutput.size()),
        createdAt
    );
}

} // namespace nodo::crypto
