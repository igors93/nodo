#include "node/PersistentMempoolStore.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/SignatureBundle.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace nodo::node {

namespace {

std::map<std::string, std::string> parseKeyValueLines(
    const std::string& contents
) {
    std::map<std::string, std::string> fields;
    std::istringstream input(contents);
    std::string line;
    bool first = true;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        if (first &&
            line == "NODO_MEMPOOL_TRANSACTION_V1") {
            fields.emplace("version", line);
            first = false;
            continue;
        }

        first = false;

        const std::size_t separator =
            line.find('=');

        if (separator == std::string::npos ||
            separator == 0 ||
            separator + 1 >= line.size()) {
            throw std::invalid_argument("Malformed mempool transaction file line.");
        }

        fields.emplace(
            line.substr(0, separator),
            line.substr(separator + 1)
        );
    }

    if (fields.find("version") == fields.end()) {
        throw std::invalid_argument("Missing mempool transaction file version.");
    }

    return fields;
}

std::string requireField(
    const std::map<std::string, std::string>& fields,
    const std::string& key
) {
    const auto found =
        fields.find(key);

    if (found == fields.end()) {
        throw std::invalid_argument("Missing mempool transaction field: " + key);
    }

    return found->second;
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
    const crypto::PublicKey& developmentPublicKey,
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
        !developmentPublicKey.isValid() ||
        acceptedAt <= 0) {
        return PersistentMempoolWriteResult::rejected(
            PersistentMempoolWriteStatus::INVALID_TRANSACTION,
            "Persistent mempool rejected invalid transaction input."
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
            developmentPublicKey,
            acceptedAt
        );

    try {
        std::filesystem::create_directories(
            directoryConfig.mempoolDirectoryPath()
        );

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
        for (const auto& entry : std::filesystem::directory_iterator(directoryConfig.mempoolDirectoryPath())) {
            if (!entry.is_regular_file() ||
                entry.path().extension() != ".nodo") {
                continue;
            }

            try {
                const std::string contents =
                    readTextFile(entry.path());

                core::Transaction transaction =
                    decodeTransactionFile(contents);

                const std::int64_t acceptedAt =
                    decodeAcceptedAt(contents);

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
                    ++skippedCount;
                }
            } catch (const std::exception&) {
                ++skippedCount;
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
    const crypto::PublicKey& developmentPublicKey,
    std::int64_t acceptedAt
) {
    std::ostringstream oss;

    oss << "NODO_MEMPOOL_TRANSACTION_V1\n"
        << "transactionId=" << transaction.id() << "\n"
        << "acceptedAt=" << acceptedAt << "\n"
        << "publicKeyMaterial=" << developmentPublicKey.keyMaterial() << "\n"
        << "transaction=" << transaction.serialize() << "\n";

    return oss.str();
}

core::Transaction PersistentMempoolStore::decodeTransactionFile(
    const std::string& contents
) {
    const std::map<std::string, std::string> fields =
        parseKeyValueLines(contents);

    core::Transaction transaction =
        core::Transaction::deserializeForStateReplay(
            requireField(fields, "transaction")
        );

    const crypto::PublicKey publicKey(
        crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        requireField(fields, "publicKeyMaterial")
    );

    const crypto::PrivateKey privateKey(
        crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "persistent-mempool-development-private-key-" + publicKey.fingerprint()
    );

    transaction.attachSignatureBundle(
        crypto::SignatureBundle::createDevelopmentSignature(
            transaction.signingPayload(),
            publicKey,
            privateKey,
            transaction.timestamp()
        )
    );

    if (transaction.id() != requireField(fields, "transactionId")) {
        throw std::invalid_argument("Persistent transaction id does not match transaction payload.");
    }

    return transaction;
}

std::int64_t PersistentMempoolStore::decodeAcceptedAt(
    const std::string& contents
) {
    const std::map<std::string, std::string> fields =
        parseKeyValueLines(contents);

    return std::stoll(
        requireField(fields, "acceptedAt")
    );
}

void PersistentMempoolStore::writeTextFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    std::ofstream output(
        path,
        std::ios::out | std::ios::trunc
    );

    if (!output) {
        throw std::runtime_error("Unable to open persistent mempool file for writing: " + path.string());
    }

    output << contents;

    if (!output) {
        throw std::runtime_error("Unable to write persistent mempool file: " + path.string());
    }
}

std::string PersistentMempoolStore::readTextFile(
    const std::filesystem::path& path
) {
    std::ifstream input(path);

    if (!input) {
        throw std::runtime_error("Unable to open persistent mempool file for reading: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    return buffer.str();
}

} // namespace nodo::node
