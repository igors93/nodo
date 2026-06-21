#ifndef NODO_CRYPTO_KEY_ENCRYPTION_SERVICE_HPP
#define NODO_CRYPTO_KEY_ENCRYPTION_SERVICE_HPP

#include <string>

namespace nodo::crypto {

/*
 * KeyEncryptionService implements symmetric encryption for key material at
 * rest using AES-256-GCM with a password-derived key.
 *
 * Key derivation: PBKDF2-HMAC-SHA256, 200 000 iterations, 32-byte output.
 * Encryption:     AES-256-GCM, 12-byte random nonce, 16-byte authentication tag.
 * Wire format:    BASE64( salt(32) || nonce(12) || tag(16) || ciphertext )
 *
 * Security principles:
 * - Salt and nonce are randomly generated per encryption call.
 * - The GCM authentication tag covers both ciphertext and the keyId AAD,
 *   so swapping an encrypted blob to a different keyId is detected.
 * - Private key material never appears in the returned envelope string.
 * - An incorrect password returns a rejection, not partial data.
 */
class KeyEncryptionService {
public:
    static constexpr int SALT_BYTES   = 32;
    static constexpr int NONCE_BYTES  = 12;
    static constexpr int TAG_BYTES    = 16;
    static constexpr int KEY_BYTES    = 32;
    static constexpr int PBKDF2_ITERS = 200000;

    /*
     * Encrypt plaintext private key material.
     *
     * @param keyId      Used as AAD to bind ciphertext to this specific key.
     * @param plaintext  Raw private key material (hex string from KeyPair).
     * @param password   User-supplied passphrase (UTF-8).
     * @returns          Hex-encoded encrypted envelope, or empty on error.
     */
    static std::string encrypt(
        const std::string& keyId,
        const std::string& plaintext,
        const std::string& password
    );

    /*
     * Decrypt an envelope produced by encrypt().
     *
     * @returns  Recovered plaintext, or empty string on auth failure.
     */
    static std::string decrypt(
        const std::string& keyId,
        const std::string& envelope,
        const std::string& password
    );

    /*
     * Returns true if the envelope has the correct structure and the
     * password authenticates correctly.  Does not return plaintext.
     */
    static bool verify(
        const std::string& keyId,
        const std::string& envelope,
        const std::string& password
    );

    static bool isEncryptedEnvelope(const std::string& value);
};

} // namespace nodo::crypto

#endif
