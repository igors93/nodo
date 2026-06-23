#include "config/NetworkParameters.hpp"
#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/SignatureProvider.hpp"
#include "crypto/Signer.hpp"
#include "mempool/Mempool.hpp"
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
        "localnet",
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
        "NODO_CRYPTO_SUITE_V1",
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
            crypto::KeyStoreKeyType::USER,
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
    const crypto::SignatureProvider& provider,
    std::int64_t feeRawUnits,
    std::uint64_t nonce = 1,
    std::int64_t timestamp = kTimestamp + 10
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
            nonce,
            timestamp
        ),
        signer
    );
}

core::AccountStateView accountStateFor(
    const std::string& address,
    std::int64_t balanceRawUnits,
    std::uint64_t nonce
) {
    core::AccountStateView view;

    requireCondition(
        view.putAccount(
            core::AccountState(
                address,
                utils::Amount::fromRawUnits(balanceRawUnits),
                nonce
            )
        ),
        "Account state test fixture should accept valid account."
    );

    return view;
}

void testAcceptsValidLocalSubmission() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const crypto::KeyStoreLoadResult key =
        createAndLoadKey(
            path,
            "local-user",
            "admission-test-seed"
        );

    const crypto::Ed25519SignatureProvider provider;
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
            "local-user",
            "admission-test-seed"
        );

    const crypto::Ed25519SignatureProvider provider;
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
            "local-user",
            "admission-test-seed"
        );

    const crypto::KeyStoreLoadResult differentKey =
        createAndLoadKey(
            path,
            "other-user",
            "admission-other-seed"
        );

    const crypto::Ed25519SignatureProvider provider;
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

void testAcceptsRuntimeSubmissionWithSufficientBalance() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const crypto::KeyStoreLoadResult key =
        createAndLoadKey(
            path,
            "local-user",
            "admission-test-seed"
        );

    const crypto::Ed25519SignatureProvider provider;
    const core::Transaction transaction =
        buildTransaction(
            key.keyPair(),
            provider,
            100
        );

    const node::TransactionAdmissionResult result =
        node::TransactionAdmissionValidator::validateRuntimeSubmission(
            transaction,
            key.metadata(),
            localnetWithMinimumFee(100),
            accountStateFor(
                key.metadata().address(),
                1100,
                0
            ),
            mempool::Mempool(),
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );

    requireCondition(
        result.accepted(),
        "Runtime admission should accept transaction when balance covers amount plus fee."
    );

    clean(path);
}

void testRejectsRuntimeSubmissionWithInsufficientBalance() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const crypto::KeyStoreLoadResult key =
        createAndLoadKey(
            path,
            "local-user",
            "admission-test-seed"
        );

    const crypto::Ed25519SignatureProvider provider;
    const core::Transaction transaction =
        buildTransaction(
            key.keyPair(),
            provider,
            100
        );

    const node::TransactionAdmissionResult result =
        node::TransactionAdmissionValidator::validateRuntimeSubmission(
            transaction,
            key.metadata(),
            localnetWithMinimumFee(100),
            accountStateFor(
                key.metadata().address(),
                1099,
                0
            ),
            mempool::Mempool(),
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );

    requireCondition(
        !result.accepted() &&
        result.status() == node::TransactionAdmissionStatus::INSUFFICIENT_BALANCE,
        "Runtime admission should reject transaction when balance cannot cover amount plus fee."
    );

    clean(path);
}

void testAcceptsRuntimeQueuedNonceWhenReservedBalanceIsSufficient() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const crypto::KeyStoreLoadResult key =
        createAndLoadKey(
            path,
            "local-user",
            "admission-test-seed"
        );

    const crypto::Ed25519SignatureProvider provider;
    const core::Transaction pending =
        buildTransaction(
            key.keyPair(),
            provider,
            100,
            1,
            kTimestamp + 20
        );
    const core::Transaction queued =
        buildTransaction(
            key.keyPair(),
            provider,
            100,
            2,
            kTimestamp + 21
        );

    mempool::Mempool mempool;
    requireCondition(
        mempool.admitTransaction(
            pending,
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            kTimestamp + 22
        ).accepted(),
        "Pending nonce fixture should enter mempool."
    );

    const node::TransactionAdmissionResult result =
        node::TransactionAdmissionValidator::validateRuntimeSubmission(
            queued,
            key.metadata(),
            localnetWithMinimumFee(100),
            accountStateFor(
                key.metadata().address(),
                2200,
                0
            ),
            mempool,
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );

    requireCondition(
        result.accepted(),
        "Runtime admission should accept queued nonce when balance covers the queue."
    );

    clean(path);
}

void testAcceptsRuntimeReplacementWhenHigherFeeIsAllowed() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const crypto::KeyStoreLoadResult key =
        createAndLoadKey(
            path,
            "local-user",
            "admission-test-seed"
        );

    const crypto::Ed25519SignatureProvider provider;
    const core::Transaction pending =
        buildTransaction(
            key.keyPair(),
            provider,
            100,
            1,
            kTimestamp + 20
        );
    const core::Transaction replacement =
        buildTransaction(
            key.keyPair(),
            provider,
            200,
            1,
            kTimestamp + 21
        );

    mempool::Mempool mempool;
    requireCondition(
        mempool.admitTransaction(
            pending,
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            kTimestamp + 22
        ).accepted(),
        "Pending replacement fixture should enter mempool."
    );

    const node::TransactionAdmissionResult result =
        node::TransactionAdmissionValidator::validateRuntimeSubmission(
            replacement,
            key.metadata(),
            localnetWithMinimumFee(100),
            accountStateFor(
                key.metadata().address(),
                1200,
                0
            ),
            mempool,
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );

    requireCondition(
        result.accepted(),
        "Runtime admission should allow higher-fee replacement when mempool policy allows it."
    );

    clean(path);
}

void testRejectsRuntimeReplacementWhenPolicyDisallowsIt() {
    const std::filesystem::path path =
        tempPath();

    clean(path);

    const crypto::KeyStoreLoadResult key =
        createAndLoadKey(
            path,
            "local-user",
            "admission-test-seed"
        );

    const crypto::Ed25519SignatureProvider provider;
    const core::Transaction pending =
        buildTransaction(
            key.keyPair(),
            provider,
            100,
            1,
            kTimestamp + 20
        );
    const core::Transaction replacement =
        buildTransaction(
            key.keyPair(),
            provider,
            200,
            1,
            kTimestamp + 21
        );

    mempool::Mempool mempool(
        mempool::MempoolConfig(
            5000,
            0,
            false,
            60 * 60
        )
    );
    requireCondition(
        mempool.admitTransaction(
            pending,
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            kTimestamp + 22
        ).accepted(),
        "Pending no-replacement fixture should enter mempool."
    );

    const node::TransactionAdmissionResult result =
        node::TransactionAdmissionValidator::validateRuntimeSubmission(
            replacement,
            key.metadata(),
            localnetWithMinimumFee(100),
            accountStateFor(
                key.metadata().address(),
                1200,
                0
            ),
            mempool,
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );

    requireCondition(
        !result.accepted() &&
        result.status() == node::TransactionAdmissionStatus::CONFLICTING_NONCE,
        "Runtime admission should reject same-nonce replacement when mempool policy disallows it."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testAcceptsValidLocalSubmission();
        testRejectsTransactionBelowMinimumFee();
        testRejectsTransactionSignedByDifferentKey();
        testAcceptsRuntimeSubmissionWithSufficientBalance();
        testRejectsRuntimeSubmissionWithInsufficientBalance();
        testAcceptsRuntimeQueuedNonceWhenReservedBalanceIsSufficient();
        testAcceptsRuntimeReplacementWhenHigherFeeIsAllowed();
        testRejectsRuntimeReplacementWhenPolicyDisallowsIt();

        std::cout << "Nodo transaction admission validator tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo transaction admission validator tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
