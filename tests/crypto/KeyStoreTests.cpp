#include "crypto/KeyStore.hpp"
#include "crypto/LocalSignatureProvider.hpp"
#include "crypto/Signer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::KeyStore;
using nodo::crypto::LocalSignatureProvider;
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
            "local-validator",
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
            "local-validator",
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
            "local-validator"
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

    const LocalSignatureProvider provider;
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
        listed.keys().front().keyId() == "local-validator" &&
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
            "local-validator",
            "key-store-test-seed",
            kTimestamp
        );

    requireCondition(
        created.success(),
        "Key creation should succeed before malformed file test."
    );

    {
        std::ofstream output(KeyStore::keyFilePath(path, "local-validator"), std::ios::trunc);
        output << "NODO_KEY_FILE_V2\n"
               << "keyId=local-validator\n"
               << "algorithm=DEVELOPMENT_FAKE_SIGNATURE\n"
               << "provider=LOCAL_DETERMINISTIC_PROVIDER_V1\n"
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
            "local-validator"
        );

    requireCondition(
        !loaded.loaded() &&
        loaded.reason().find("key_local-validator.nodo") != std::string::npos &&
        loaded.reason().find("unknownField") != std::string::npos,
        "Malformed key file should be rejected with file path and reason."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testCreateLoadAndListKey();
        testRejectsUnsafeKeyId();
        testRejectsMalformedKeyFile();

        std::cout << "Nodo key store tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo key store tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
