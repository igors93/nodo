#include "config/NetworkParameters.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/LocalSignatureProvider.hpp"
#include "crypto/Signer.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "utils/Amount.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

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
        / "nodo-transaction-admission-validator-tests";
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

config::NetworkParameters localnetWithMinimumFee(
    std::uint64_t minimumFeeRawUnits
) {
    return config::NetworkParameters(
        "nodo-localnet-admission-test",
        "nodo-localnet",
        "nodo/0.1",
        60,
        1,
        2,
        3,
        1000,
        128,
        10000,
        minimumFeeRawUnits,
        60,
        1,
        "LOCAL_DETERMINISTIC_PROVIDER_V1",
        "NODO_STORAGE_V2"
    );
}

crypto::KeyStoreLoadResult createAndLoadKey(
    const std::filesystem::path& path,
    const std::string& keyId,
    const std::string& seed
) {
    const crypto::KeyStoreCreateResult created =
        crypto::KeyStore::createLocalKey(
            path,
            keyId,
            seed,
            kTimestamp
        );

    if (!created.success()) {
        throw std::runtime_error("Key creation failed in admission test: " + created.reason());
    }

    const crypto::KeyStoreLoadResult loaded =
        crypto::KeyStore::loadKey(
            path,
            keyId
        );

    if (!loaded.loaded()) {
        throw std::runtime_error("Key load failed in admission test: " + loaded.reason());
    }

    return loaded;
}

core::Transaction buildTransaction(
    const crypto::KeyPair& keyPair,
    const crypto::LocalSignatureProvider& provider,
    std::int64_t feeRawUnits
) {
    const crypto::Signer signer(
        keyPair,
        provider
    );

    return core::TransactionBuilder::buildSignedTransfer(
        core::TransactionBuildRequest(
            "nodo-localnet-recipient",
            utils::Amount::fromRawUnits(1000),
            utils::Amount::fromRawUnits(feeRawUnits),
            1,
            kTimestamp + 10
        ),
        signer
    );
}

void testAcceptsValidLocalSubmission() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const crypto::KeyStoreLoadResult key =
        createAndLoadKey(
            path,
            "local-validator",
            "admission-test-seed"
        );

    const crypto::LocalSignatureProvider provider;
    const core::Transaction transaction =
        buildTransaction(
            key.keyPair(),
            provider,
            100
        );

    const node::TransactionAdmissionResult result =
        node::TransactionAdmissionValidator::validateLocalSubmission(
            transaction,
            key.metadata(),
            localnetWithMinimumFee(100),
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );

    requireCondition(
        result.accepted(),
        "Valid local transaction should pass admission validation."
    );

    clean(path);
}

void testRejectsTransactionBelowMinimumFee() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const crypto::KeyStoreLoadResult key =
        createAndLoadKey(
            path,
            "local-validator",
            "admission-test-seed"
        );

    const crypto::LocalSignatureProvider provider;
    const core::Transaction transaction =
        buildTransaction(
            key.keyPair(),
            provider,
            99
        );

    const node::TransactionAdmissionResult result =
        node::TransactionAdmissionValidator::validateLocalSubmission(
            transaction,
            key.metadata(),
            localnetWithMinimumFee(100),
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );

    requireCondition(
        !result.accepted() &&
        result.status() == node::TransactionAdmissionStatus::BELOW_MINIMUM_FEE,
        "Transaction below network minimum fee should be rejected."
    );

    clean(path);
}

void testRejectsTransactionSignedByDifferentKey() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const crypto::KeyStoreLoadResult signerKey =
        createAndLoadKey(
            path,
            "local-validator",
            "admission-test-seed"
        );

    const crypto::KeyStoreLoadResult differentKey =
        createAndLoadKey(
            path,
            "other-validator",
            "admission-other-seed"
        );

    const crypto::LocalSignatureProvider provider;
    const core::Transaction transaction =
        buildTransaction(
            signerKey.keyPair(),
            provider,
            100
        );

    const node::TransactionAdmissionResult result =
        node::TransactionAdmissionValidator::validateLocalSubmission(
            transaction,
            differentKey.metadata(),
            localnetWithMinimumFee(100),
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );

    requireCondition(
        !result.accepted() &&
        result.status() == node::TransactionAdmissionStatus::INVALID_KEY,
        "Transaction signed by one key must not be admitted with another key metadata."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testAcceptsValidLocalSubmission();
        testRejectsTransactionBelowMinimumFee();
        testRejectsTransactionSignedByDifferentKey();

        std::cout << "Nodo transaction admission validator tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo transaction admission validator tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
