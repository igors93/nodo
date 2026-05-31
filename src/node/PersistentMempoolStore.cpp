#include "node/PersistentMempoolStore.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SignatureBundle.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"
#include "utils/Amount.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

constexpr const char* MEMPOOL_TRANSACTION_VERSION =
    "NODO_MEMPOOL_TRANSACTION_V2";

serialization::KeyValueFileDocument parseTransactionDocument(
    const std::string& contents
) {
    serialization::KeyValueFileDocument document =
        serialization::KeyValueFileCodec::parse(
            contents,
            MEMPOOL_TRANSACTION_VERSION
        );

    document.requireOnlyFields(
        {
            "transactionId",
            "acceptedAt",
            "signatureSuite",
            "signatureDomain",
            "publicKeyAlgorithm",
            "publicKeyMaterial",
            "signatureHex",
            "signatureCreatedAt",
            "transaction"
        }
    );

    return document;
}

bool isSafeTransactionId(
    const std::string& transactionId
) {
    if (transactionId.empty()) {
        return false;
    }

    for (const char current : transactionId) {
        const bool hex =
            (current >= '0' && current <= '9') ||
            (current >= 'a' && current <= 'f') ||
            (current >= 'A' && current <= 'F');

        if (!hex) {
            return false;
        }
    }

    return true;
}

bool samePublicKey(
    const crypto::PublicKey& left,
    const crypto::PublicKey& right
) {
    return left.algorithm() == right.algorithm() &&
           left.keyMaterial() == right.keyMaterial();
}

bool allSignaturesUsePublicKey(
    const core::Transaction& transaction,
    const crypto::PublicKey& publicKey
) {
    if (!transaction.hasSignatureBundle()) {
        return false;
    }

    for (const crypto::Signature& signature : transaction.signatureBundle().signatures()) {
        if (!samePublicKey(
                signature.publicKey(),
                publicKey
            )) {
            return false;
        }
    }

    return true;
}

void validatePersistentTransactionSignature(
    const core::Transaction& transaction,
    const crypto::PublicKey& publicKey,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    const crypto::SignatureProvider& provider
) {
    if (!allSignaturesUsePublicKey(
            transaction,
            publicKey
        )) {
        throw std::invalid_argument("Persistent transaction signature does not use the declared public key.");
    }

    if (!transaction.signatureBundle().verifyForPolicy(
            transaction.signingPayload(),
            policy,
            context,
            provider
        )) {
        throw std::invalid_argument("Persistent transaction signature verification failed.");
    }
}

std::vector<std::filesystem::path> canonicalMempoolFiles(
    const std::filesystem::path& mempoolDirectory
) {
    std::vector<std::filesystem::path> files;

    for (const auto& entry : std::filesystem::directory_iterator(mempoolDirectory)) {
        if (!entry.is_regular_file() ||
            entry.path().extension() != ".nodo") {
            continue;
        }

        files.push_back(entry.path());
    }

    std::sort(
        files.begin(),
        files.end()
    );

    return files;
}

utils::Amount requiredTransactionBalance(
    const core::Transaction& transaction
) {
    try {
        return transaction.amount() + transaction.fee();
    } catch (const std::exception& error) {
        throw std::invalid_argument(
            std::string("Persistent transaction amount plus fee is invalid: ")
            + error.what()
        );
    }
}

void validateTransactionAgainstAccountState(
    const core::Transaction& transaction,
    const core::AccountStateView& accountStateView,
    std::int64_t minimumFeeRawUnits,
    std::set<std::string>& pendingSenders
) {
    if (minimumFeeRawUnits < 0) {
        throw std::invalid_argument("Persistent mempool minimum fee is negative.");
    }

    if (transaction.fee().rawUnits() < minimumFeeRawUnits) {
        throw std::invalid_argument("Persistent transaction fee is below the network minimum.");
    }

    if (!accountStateView.hasAccount(transaction.fromAddress())) {
        throw std::invalid_argument("Persistent transaction sender account is unknown.");
    }

    const core::AccountState sender =
        accountStateView.accountOrDefault(transaction.fromAddress());

    if (sender.nonce() == std::numeric_limits<std::uint64_t>::max()) {
        throw std::invalid_argument("Persistent transaction sender nonce cannot advance without overflow.");
    }

    const std::uint64_t expectedNonce =
        sender.nonce() + 1;

    if (transaction.nonce() <= sender.nonce()) {
        throw std::invalid_argument("Persistent transaction nonce is older than account state.");
    }

    if (transaction.nonce() != expectedNonce) {
        throw std::invalid_argument("Persistent transaction nonce is in the future and no per-account queue is available.");
    }

    if (sender.balance() < requiredTransactionBalance(transaction)) {
        throw std::invalid_argument("Persistent transaction sender balance is insufficient for amount plus fee.");
    }

    if (!pendingSenders.insert(transaction.fromAddress()).second) {
        throw std::invalid_argument("Persistent mempool already has a pending transaction from this sender.");
    }
}

} // namespace

std::string persistentMempoolWriteStatusToString(
    PersistentMempoolWriteStatus status
) {
    switch (status) {
        case PersistentMempoolWriteStatus::STORED:
            return "STORED";
        case PersistentMempoolWriteStatus::ALREADY_STORED:
            return "ALREADY_STORED";
        case PersistentMempoolWriteStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case PersistentMempoolWriteStatus::INVALID_TRANSACTION:
            return "INVALID_TRANSACTION";
        case PersistentMempoolWriteStatus::IO_ERROR:
            return "IO_ERROR";
        default:
            return "IO_ERROR";
    }
}

PersistentMempoolWriteResult::PersistentMempoolWriteResult()
    : m_status(PersistentMempoolWriteStatus::IO_ERROR),
      m_reason("Uninitialized persistent mempool write result."),
      m_transactionId(""),
      m_path() {}

PersistentMempoolWriteResult PersistentMempoolWriteResult::stored(
    std::string transactionId,
    std::filesystem::path path
) {
    PersistentMempoolWriteResult result;
    result.m_status = PersistentMempoolWriteStatus::STORED;
    result.m_reason = "";
    result.m_transactionId = std::move(transactionId);
    result.m_path = std::move(path);
    return result;
}

PersistentMempoolWriteResult PersistentMempoolWriteResult::alreadyStored(
    std::string transactionId,
    std::filesystem::path path
) {
    PersistentMempoolWriteResult result;
    result.m_status = PersistentMempoolWriteStatus::ALREADY_STORED;
    result.m_reason = "Transaction already exists in persistent mempool.";
    result.m_transactionId = std::move(transactionId);
    result.m_path = std::move(path);
    return result;
}

PersistentMempoolWriteResult PersistentMempoolWriteResult::rejected(
    PersistentMempoolWriteStatus status,
    std::string reason
) {
    PersistentMempoolWriteResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

PersistentMempoolWriteStatus PersistentMempoolWriteResult::status() const {
    return m_status;
}

const std::string& PersistentMempoolWriteResult::reason() const {
    return m_reason;
}

const std::string& PersistentMempoolWriteResult::transactionId() const {
    return m_transactionId;
}

const std::filesystem::path& PersistentMempoolWriteResult::path() const {
    return m_path;
}

bool PersistentMempoolWriteResult::stored() const {
    return m_status == PersistentMempoolWriteStatus::STORED;
}

bool PersistentMempoolWriteResult::alreadyStored() const {
    return m_status == PersistentMempoolWriteStatus::ALREADY_STORED;
}

bool PersistentMempoolWriteResult::success() const {
    return stored() || alreadyStored();
}

std::string PersistentMempoolWriteResult::serialize() const {
    std::ostringstream oss;

    oss << "PersistentMempoolWriteResult{"
        << "status=" << persistentMempoolWriteStatusToString(m_status)
        << ";reason=" << m_reason
        << ";transactionId=" << m_transactionId
        << ";path=" << m_path.string()
        << "}";

    return oss.str();
}

PersistentMempoolLoadResult::PersistentMempoolLoadResult()
    : m_status(PersistentMempoolLoadStatus::IO_ERROR),
      m_reason("Uninitialized persistent mempool load result."),
      m_loadedTransactionCount(0),
      m_skippedTransactionCount(0) {}

PersistentMempoolLoadResult PersistentMempoolLoadResult::loaded(
    std::size_t loadedTransactionCount,
    std::size_t skippedTransactionCount
) {
    PersistentMempoolLoadResult result;
    result.m_status = PersistentMempoolLoadStatus::LOADED;
    result.m_reason = "";
    result.m_loadedTransactionCount = loadedTransactionCount;
    result.m_skippedTransactionCount = skippedTransactionCount;
    return result;
}

PersistentMempoolLoadResult PersistentMempoolLoadResult::rejected(
    PersistentMempoolLoadStatus status,
    std::string reason
) {
    PersistentMempoolLoadResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

PersistentMempoolLoadStatus PersistentMempoolLoadResult::status() const {
    return m_status;
}

const std::string& PersistentMempoolLoadResult::reason() const {
    return m_reason;
}

std::size_t PersistentMempoolLoadResult::loadedTransactionCount() const {
    return m_loadedTransactionCount;
}

std::size_t PersistentMempoolLoadResult::skippedTransactionCount() const {
    return m_skippedTransactionCount;
}

bool PersistentMempoolLoadResult::loaded() const {
    return m_status == PersistentMempoolLoadStatus::LOADED;
}

std::string PersistentMempoolLoadResult::serialize() const {
    std::ostringstream oss;

    oss << "PersistentMempoolLoadResult{"
        << "loadedTransactionCount=" << m_loadedTransactionCount
        << ";skippedTransactionCount=" << m_skippedTransactionCount
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

PersistentMempoolWriteResult PersistentMempoolStore::persistTransaction(
    const NodeDataDirectoryConfig& directoryConfig,
    const core::Transaction& transaction,
    const crypto::PublicKey& publicKey,
    std::int64_t acceptedAt
) {
    if (!directoryConfig.isValid()) {
        return PersistentMempoolWriteResult::rejected(
            PersistentMempoolWriteStatus::INVALID_CONFIG,
            "Node data directory config is invalid."
        );
    }

    if (!NodeDataDirectory::isInitialized(directoryConfig)) {
        return PersistentMempoolWriteResult::rejected(
            PersistentMempoolWriteStatus::INVALID_CONFIG,
            "Node data directory is not initialized."
        );
    }

    if (!transaction.isStructurallyValid(
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION
        ) ||
        !publicKey.isValid() ||
        acceptedAt <= 0) {
        return PersistentMempoolWriteResult::rejected(
            PersistentMempoolWriteStatus::INVALID_TRANSACTION,
            "Persistent mempool rejected invalid transaction input."
        );
    }

    const crypto::Ed25519SignatureProvider provider;

    try {
        validatePersistentTransactionSignature(
            transaction,
            publicKey,
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            provider
        );
    } catch (const std::exception& error) {
        return PersistentMempoolWriteResult::rejected(
            PersistentMempoolWriteStatus::INVALID_TRANSACTION,
            error.what()
        );
    }

    const std::filesystem::path path =
        transactionFilePath(
            directoryConfig,
            transaction.id()
        );

    const std::string contents =
        transactionFileContents(
            transaction,
            publicKey,
            acceptedAt
        );

    try {
        std::filesystem::create_directories(
            directoryConfig.mempoolDirectoryPath()
        );

        for (const std::filesystem::path& existingPath :
             canonicalMempoolFiles(directoryConfig.mempoolDirectoryPath())) {
            if (existingPath == path) {
                continue;
            }

            std::optional<core::Transaction> existingTransaction;

            try {
                existingTransaction =
                    decodeTransactionFile(
                        readTextFile(existingPath)
                    );
            } catch (const std::exception& error) {
                return PersistentMempoolWriteResult::rejected(
                    PersistentMempoolWriteStatus::INVALID_TRANSACTION,
                    "Invalid persistent mempool file "
                    + existingPath.string()
                    + ": "
                    + error.what()
                );
            }

            if (existingTransaction->fromAddress() == transaction.fromAddress() &&
                existingTransaction->nonce() == transaction.nonce()) {
                return PersistentMempoolWriteResult::rejected(
                    PersistentMempoolWriteStatus::INVALID_TRANSACTION,
                    "Another persistent transaction with the same sender nonce already exists."
                );
            }
        }

        if (std::filesystem::exists(path)) {
            if (readTextFile(path) == contents) {
                return PersistentMempoolWriteResult::alreadyStored(
                    transaction.id(),
                    path
                );
            }

            return PersistentMempoolWriteResult::rejected(
                PersistentMempoolWriteStatus::INVALID_TRANSACTION,
                "Different persistent transaction content exists for same id."
            );
        }

        writeTextFile(
            path,
            contents
        );

        return PersistentMempoolWriteResult::stored(
            transaction.id(),
            path
        );
    } catch (const std::exception& error) {
        return PersistentMempoolWriteResult::rejected(
            PersistentMempoolWriteStatus::IO_ERROR,
            error.what()
        );
    }
}

PersistentMempoolLoadResult PersistentMempoolStore::loadIntoMempool(
    const NodeDataDirectoryConfig& directoryConfig,
    mempool::Mempool& mempool,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context
) {
    if (!directoryConfig.isValid()) {
        return PersistentMempoolLoadResult::rejected(
            PersistentMempoolLoadStatus::INVALID_CONFIG,
            "Node data directory config is invalid."
        );
    }

    if (!std::filesystem::exists(directoryConfig.mempoolDirectoryPath())) {
        return PersistentMempoolLoadResult::loaded(0, 0);
    }

    std::size_t loadedCount = 0;
    std::size_t skippedCount = 0;

    try {
        const crypto::Ed25519SignatureProvider provider;

        for (const std::filesystem::path& path :
             canonicalMempoolFiles(directoryConfig.mempoolDirectoryPath())) {
            const std::string contents =
                readTextFile(path);

            try {
                core::Transaction transaction =
                    decodeTransactionFile(contents);

                const std::int64_t acceptedAt =
                    decodeAcceptedAt(contents);

                validatePersistentTransactionSignature(
                    transaction,
                    transaction.signatureBundle().signatures().front().publicKey(),
                    policy,
                    context,
                    provider
                );

                const auto admission =
                    mempool.admitTransaction(
                        transaction,
                        policy,
                        context,
                        acceptedAt
                    );

                if (admission.success()) {
                    ++loadedCount;
                } else {
                    return PersistentMempoolLoadResult::rejected(
                        PersistentMempoolLoadStatus::IO_ERROR,
                        "Invalid persistent mempool file "
                        + path.string()
                        + ": "
                        + admission.reason()
                    );
                }
            } catch (const std::exception& error) {
                return PersistentMempoolLoadResult::rejected(
                    PersistentMempoolLoadStatus::IO_ERROR,
                    "Invalid persistent mempool file "
                    + path.string()
                    + ": "
                    + error.what()
                );
            }
        }
    } catch (const std::exception& error) {
        return PersistentMempoolLoadResult::rejected(
            PersistentMempoolLoadStatus::IO_ERROR,
            error.what()
        );
    }

    return PersistentMempoolLoadResult::loaded(
        loadedCount,
        skippedCount
    );
}

PersistentMempoolLoadResult PersistentMempoolStore::loadIntoMempool(
    const NodeDataDirectoryConfig& directoryConfig,
    mempool::Mempool& mempool,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    const core::AccountStateView& accountStateView,
    std::int64_t minimumFeeRawUnits,
    const crypto::SignatureProvider& provider
) {
    if (!directoryConfig.isValid() ||
        !accountStateView.isValid() ||
        minimumFeeRawUnits < 0) {
        return PersistentMempoolLoadResult::rejected(
            PersistentMempoolLoadStatus::INVALID_CONFIG,
            "Persistent mempool load config is invalid."
        );
    }

    if (!std::filesystem::exists(directoryConfig.mempoolDirectoryPath())) {
        return PersistentMempoolLoadResult::loaded(0, 0);
    }

    std::size_t loadedCount = 0;
    std::size_t skippedCount = 0;
    std::set<std::string> pendingSenders;

    try {
        for (const std::filesystem::path& path :
             canonicalMempoolFiles(directoryConfig.mempoolDirectoryPath())) {
            const std::string contents =
                readTextFile(path);

            try {
                core::Transaction transaction =
                    decodeTransactionFile(contents);

                const std::int64_t acceptedAt =
                    decodeAcceptedAt(contents);

                validatePersistentTransactionSignature(
                    transaction,
                    transaction.signatureBundle().signatures().front().publicKey(),
                    policy,
                    context,
                    provider
                );

                validateTransactionAgainstAccountState(
                    transaction,
                    accountStateView,
                    minimumFeeRawUnits,
                    pendingSenders
                );

                const auto admission =
                    mempool.admitTransaction(
                        transaction,
                        policy,
                        context,
                        acceptedAt
                    );

                if (admission.success()) {
                    ++loadedCount;
                } else {
                    return PersistentMempoolLoadResult::rejected(
                        PersistentMempoolLoadStatus::IO_ERROR,
                        "Invalid persistent mempool file "
                        + path.string()
                        + ": "
                        + admission.reason()
                    );
                }
            } catch (const std::exception& error) {
                return PersistentMempoolLoadResult::rejected(
                    PersistentMempoolLoadStatus::IO_ERROR,
                    "Invalid persistent mempool file "
                    + path.string()
                    + ": "
                    + error.what()
                );
            }
        }
    } catch (const std::exception& error) {
        return PersistentMempoolLoadResult::rejected(
            PersistentMempoolLoadStatus::IO_ERROR,
            error.what()
        );
    }

    return PersistentMempoolLoadResult::loaded(
        loadedCount,
        skippedCount
    );
}

std::size_t PersistentMempoolStore::removeTransactions(
    const NodeDataDirectoryConfig& directoryConfig,
    const std::vector<std::string>& transactionIds
) {
    if (!directoryConfig.isValid()) {
        return 0;
    }

    std::size_t removed = 0;

    for (const std::string& transactionId : transactionIds) {
        const std::filesystem::path path =
            transactionFilePath(
                directoryConfig,
                transactionId
            );

        std::error_code error;
        if (std::filesystem::remove(path, error)) {
            ++removed;
        }
    }

    return removed;
}

std::filesystem::path PersistentMempoolStore::transactionFilePath(
    const NodeDataDirectoryConfig& directoryConfig,
    const std::string& transactionId
) {
    if (!isSafeTransactionId(transactionId)) {
        throw std::invalid_argument("Unsafe transaction id for mempool path.");
    }

    return directoryConfig.mempoolDirectoryPath()
        / ("tx_" + transactionId + ".nodo");
}

std::string PersistentMempoolStore::transactionFileContents(
    const core::Transaction& transaction,
    const crypto::PublicKey& publicKey,
    std::int64_t acceptedAt
) {
    if (!transaction.hasSignatureBundle() ||
        transaction.signatureBundle().signatures().empty()) {
        throw std::invalid_argument("Persistent mempool requires a signed transaction.");
    }

    const crypto::Signature& signature =
        transaction.signatureBundle().signatures().front();

    return serialization::KeyValueFileCodec::serialize(
        MEMPOOL_TRANSACTION_VERSION,
        {
            {"transactionId", transaction.id()},
            {"acceptedAt", std::to_string(acceptedAt)},
            {"signatureSuite", crypto::cryptoSuiteIdToString(signature.suite())},
            {"signatureDomain", crypto::signingDomainToString(signature.domain())},
            {"publicKeyAlgorithm", crypto::cryptoAlgorithmToString(publicKey.algorithm())},
            {"publicKeyMaterial", publicKey.keyMaterial()},
            {"signatureHex", signature.signatureHex()},
            {"signatureCreatedAt", std::to_string(signature.createdAt())},
            {"transaction", transaction.serialize()}
        }
    );
}

core::Transaction PersistentMempoolStore::decodeTransactionFile(
    const std::string& contents
) {
    const serialization::KeyValueFileDocument fields =
        parseTransactionDocument(contents);

    core::Transaction transaction =
        core::Transaction::deserializeForStateReplay(
            fields.requireField("transaction")
        );

    const crypto::CryptoAlgorithm algorithm =
        crypto::cryptoAlgorithmFromString(
            fields.requireField("publicKeyAlgorithm")
        );

    if (crypto::cryptoAlgorithmToString(algorithm) !=
        fields.requireField("publicKeyAlgorithm")) {
        throw std::invalid_argument("Persistent transaction public key algorithm is unknown.");
    }

    const crypto::PublicKey publicKey(
        algorithm,
        fields.requireField("publicKeyMaterial")
    );

    crypto::SignatureBundle signatureBundle;

    const crypto::CryptoSuiteId suite =
        crypto::cryptoSuiteIdFromString(
            fields.requireField("signatureSuite")
        );

    if (!crypto::isSupportedCryptoSuite(suite)) {
        throw std::invalid_argument("Persistent transaction signature suite is unknown.");
    }

    const crypto::SigningDomain domain =
        crypto::signingDomainFromString(
            fields.requireField("signatureDomain")
        );

    if (domain == crypto::SigningDomain::UNKNOWN) {
        throw std::invalid_argument("Persistent transaction signature domain is unknown.");
    }

    signatureBundle.addSignature(
        crypto::Signature(
            suite,
            domain,
            algorithm,
            publicKey,
            fields.requireField("signatureHex"),
            std::stoll(fields.requireField("signatureCreatedAt"))
        )
    );

    transaction.attachSignatureBundle(signatureBundle);

    if (transaction.id() != fields.requireField("transactionId")) {
        throw std::invalid_argument("Persistent transaction id does not match transaction payload.");
    }

    const std::int64_t acceptedAt =
        std::stoll(fields.requireField("acceptedAt"));

    if (transactionFileContents(
            transaction,
            publicKey,
            acceptedAt
        ) != contents) {
        throw std::invalid_argument("Persistent mempool file is not canonical.");
    }

    return transaction;
}

std::int64_t PersistentMempoolStore::decodeAcceptedAt(
    const std::string& contents
) {
    const serialization::KeyValueFileDocument fields =
        parseTransactionDocument(contents);

    return std::stoll(
        fields.requireField("acceptedAt")
    );
}

void PersistentMempoolStore::writeTextFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    storage::AtomicFile::writeTextFile(
        path,
        contents
    );
}

std::string PersistentMempoolStore::readTextFile(
    const std::filesystem::path& path
) {
    return storage::AtomicFile::readTextFile(path);
}

} // namespace nodo::node
