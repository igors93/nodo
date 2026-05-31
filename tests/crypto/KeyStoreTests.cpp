#include "crypto/KeyStore.hpp"
#include "crypto/LocalSignatureProvider.hpp"
#include "crypto/Signer.hpp"

#include <filesystem>
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
        listed.keys().front().keyId() == "local-validator",
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

} // namespace

int main() {
    try {
        testCreateLoadAndListKey();
        testRejectsUnsafeKeyId();

        std::cout << "Nodo key store tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo key store tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
