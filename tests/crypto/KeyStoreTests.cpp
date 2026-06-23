#include "crypto/KeyStore.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/Signer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::KeyStore;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyStoreKeyType;
using nodo::crypto::Signer;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path tempPath() {
    return std::filesystem::temp_directory_path()
        / "nodo-key-store-tests";
}

void clean(
    const std::filesystem::path& path
) {
    std::error_code error;
    std::filesystem::remove_all(
        path,
        error
    );
}

void testCreateLoadAndListKey() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const auto created =
        KeyStore::createLocalKey(
            path,
            "local-user",
            KeyStoreKeyType::USER,
            "key-store-test-seed",
            kTimestamp
        );

    requireCondition(
        created.success(),
        "Key creation should succeed."
    );

    requireCondition(
        created.metadata().networkProfile() == KeyStore::LOCAL_NETWORK_PROFILE &&
        created.metadata().isLocalnetOnly(),
        "Created key should be marked as localnet-only."
    );

    const std::string publicMetadata =
        created.metadata().serializePublic();

    requireCondition(
        publicMetadata.find("privateKeyMaterial") == std::string::npos,
        "Public key metadata must not expose private key material."
    );

    const auto duplicate =
        KeyStore::createLocalKey(
            path,
            "local-user",
            KeyStoreKeyType::USER,
            "key-store-test-seed",
            kTimestamp + 1
        );

    requireCondition(
        !duplicate.success(),
        "Key creation should not overwrite an existing key."
    );

    const auto loaded =
        KeyStore::loadKey(
            path,
            "local-user"
        );

    requireCondition(
        loaded.loaded() &&
        loaded.metadata().address() == created.metadata().address(),
        "Stored key should load with matching public metadata."
    );

    requireCondition(
        loaded.metadata().networkProfile() == KeyStore::LOCAL_NETWORK_PROFILE &&
        loaded.metadata().isLocalnetOnly(),
        "Loaded key should stay marked as localnet-only."
    );

    const Ed25519SignatureProvider provider;
    const Signer signer(
        loaded.keyPair(),
        provider
    );

    requireCondition(
        signer.address() == created.metadata().address(),
        "Signer should expose the stored key address."
    );

    const auto listed =
        KeyStore::listKeys(path);

    requireCondition(
        listed.loaded() &&
        listed.keys().size() == 1U &&
        listed.keys().front().keyId() == "local-user" &&
        listed.keys().front().keyType() == KeyStoreKeyType::USER &&
        listed.keys().front().networkProfile() == KeyStore::LOCAL_NETWORK_PROFILE,
        "Key listing should be deterministic and public-only."
    );

    clean(path);
}

void testRejectsUnsafeKeyId() {
    const auto created =
        KeyStore::createLocalKey(
            tempPath(),
            "../escape",
            "seed",
            kTimestamp
        );

    requireCondition(
        !created.success(),
        "Key store should reject unsafe key ids."
    );
}

void testRejectsMalformedKeyFile() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const auto created =
        KeyStore::createLocalKey(
            path,
            "local-user",
            KeyStoreKeyType::USER,
            "key-store-test-seed",
            kTimestamp
        );

    requireCondition(
        created.success(),
        "Key creation should succeed before malformed file test."
    );

    {
        std::ofstream output(KeyStore::keyFilePath(path, "local-user"), std::ios::trunc);
        output << "NODO_KEY_FILE_V3\n"
               << "keyId=local-user\n"
               << "algorithm=CLASSIC_ED25519\n"
               << "suite=NODO_CRYPTO_SUITE_V1\n"
               << "keyType=USER\n"
               << "provider=OPENSSL_ED25519_PROVIDER_V1\n"
               << "networkProfile=localnet\n"
               << "publicKeyMaterial=broken\n"
               << "privateKeyMaterial=broken\n"
               << "address=broken\n"
               << "createdAt=" << kTimestamp << "\n"
               << "unknownField=must-fail\n";
    }

    const auto loaded =
        KeyStore::loadKey(
            path,
            "local-user"
        );

    requireCondition(
        !loaded.loaded() &&
        loaded.reason().find("key_local-user.nodo") != std::string::npos &&
        loaded.reason().find("unknownField") != std::string::npos,
        "Malformed key file should be rejected with file path and reason."
    );

    clean(path);
}

void testEncryptedKey() {
    const std::filesystem::path path = tempPath();
    clean(path);

    const std::string keyId = "enc-user";
    const std::string seed = "key-store-test-seed";
    const std::string password = "strong-password-123";
    const std::string network = "testnet-candidate";

    // 1. Create encrypted key
    const auto created =
        KeyStore::createLocalKey(
            path,
            keyId,
            KeyStoreKeyType::USER,
            seed,
            kTimestamp,
            password,
            network
        );

    requireCondition(
        created.success(),
        "Encrypted key creation should succeed. Reason: " + created.reason()
    );

    requireCondition(
        created.metadata().networkProfile() == network &&
        !created.metadata().isLocalnetOnly() &&
        created.metadata().encryptionLevel() == nodo::crypto::KeyEncryptionLevel::TESTNET_SAFE,
        "Created encrypted key should match the network profile and encryption level."
    );

    // 2. Try loading without password
    const auto loadedNoPassword =
        KeyStore::loadKey(
            path,
            keyId
        );

    requireCondition(
        !loadedNoPassword.loaded(),
        "Loading encrypted key without password should fail."
    );

    // 3. Try loading with wrong password
    const auto loadedWrongPassword =
        KeyStore::loadKey(
            path,
            keyId,
            "wrong-password"
        );

    requireCondition(
        !loadedWrongPassword.loaded(),
        "Loading encrypted key with wrong password should fail."
    );

    // 4. Load metadata only
    const auto loadedMetadataOnly =
        KeyStore::loadKey(
            path,
            keyId,
            "",
            true
        );

    requireCondition(
        loadedMetadataOnly.loaded() &&
        loadedMetadataOnly.metadata().keyId() == keyId &&
        loadedMetadataOnly.metadata().networkProfile() == network,
        "Loading metadata only should succeed without password. Reason: " + loadedMetadataOnly.reason()
    );

    // 5. Load with correct password
    const auto loadedCorrectPassword =
        KeyStore::loadKey(
            path,
            keyId,
            password
        );

    requireCondition(
        loadedCorrectPassword.loaded() &&
        loadedCorrectPassword.metadata().address() == created.metadata().address(),
        "Loading encrypted key with correct password should succeed."
    );

    // 6. Test that listed keys includes our encrypted key and can load metadata
    const auto listed = KeyStore::listKeys(path);
    requireCondition(
        listed.loaded() &&
        listed.keys().size() == 1U &&
        listed.keys().front().keyId() == keyId &&
        listed.keys().front().networkProfile() == network &&
        listed.keys().front().encryptionLevel() == nodo::crypto::KeyEncryptionLevel::TESTNET_SAFE,
        "Listing should succeed and list encrypted keys without password."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testCreateLoadAndListKey();
        testRejectsUnsafeKeyId();
        testRejectsMalformedKeyFile();
        testEncryptedKey();

        std::cout << "Nodo key store tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo key store tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
