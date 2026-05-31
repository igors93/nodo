#include "crypto/KeyStore.hpp"

#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::crypto {

namespace {

constexpr const char* KEY_FILE_VERSION =
    "NODO_KEY_FILE_V1";

constexpr const char* KEY_INDEX_VERSION =
    "NODO_KEY_INDEX_V1";

bool isSafeScalar(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.' ||
            character == ':' ||
            character == '#';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

} // namespace

StoredKeyMetadata::StoredKeyMetadata()
    : m_keyId(""),
      m_algorithm(CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE),
      m_provider(""),
      m_publicKey(),
      m_address(""),
      m_createdAt(0) {}

StoredKeyMetadata::StoredKeyMetadata(
    std::string keyId,
    CryptoAlgorithm algorithm,
    std::string provider,
    PublicKey publicKey,
    std::string address,
    std::int64_t createdAt
)
    : m_keyId(std::move(keyId)),
      m_algorithm(algorithm),
      m_provider(std::move(provider)),
      m_publicKey(std::move(publicKey)),
      m_address(std::move(address)),
      m_createdAt(createdAt) {}

const std::string& StoredKeyMetadata::keyId() const {
    return m_keyId;
}

CryptoAlgorithm StoredKeyMetadata::algorithm() const {
    return m_algorithm;
}

const std::string& StoredKeyMetadata::provider() const {
    return m_provider;
}

const PublicKey& StoredKeyMetadata::publicKey() const {
    return m_publicKey;
}

const std::string& StoredKeyMetadata::address() const {
    return m_address;
}

std::int64_t StoredKeyMetadata::createdAt() const {
    return m_createdAt;
}

bool StoredKeyMetadata::isValid() const {
    if (!KeyStore::isSafeKeyId(m_keyId) ||
        !isSafeScalar(m_provider) ||
        !m_publicKey.isValid() ||
        !isSafeScalar(m_publicKey.keyMaterial()) ||
        !isSafeScalar(m_address) ||
        m_createdAt <= 0) {
        return false;
    }

    if (m_publicKey.algorithm() != m_algorithm) {
        return false;
    }

    return NodeIdentity(
        m_keyId,
        m_publicKey,
        m_address
    ).isValid();
}

std::string StoredKeyMetadata::serializePublic() const {
    std::ostringstream oss;

    oss << "StoredKeyMetadata{"
        << "keyId=" << m_keyId
        << ";algorithm=" << cryptoAlgorithmToString(m_algorithm)
        << ";provider=" << m_provider
        << ";publicKeyFingerprint=" << m_publicKey.fingerprint()
        << ";address=" << m_address
        << ";createdAt=" << m_createdAt
        << "}";

    return oss.str();
}

std::string keyStoreStatusToString(
    KeyStoreStatus status
) {
    switch (status) {
        case KeyStoreStatus::OK:
            return "OK";
        case KeyStoreStatus::ALREADY_EXISTS:
            return "ALREADY_EXISTS";
        case KeyStoreStatus::NOT_FOUND:
            return "NOT_FOUND";
        case KeyStoreStatus::INVALID_INPUT:
            return "INVALID_INPUT";
        case KeyStoreStatus::IO_ERROR:
            return "IO_ERROR";
        default:
            return "IO_ERROR";
    }
}

KeyStoreCreateResult::KeyStoreCreateResult()
    : m_status(KeyStoreStatus::IO_ERROR),
      m_reason("Uninitialized key store create result."),
      m_metadata(),
      m_path() {}

KeyStoreCreateResult KeyStoreCreateResult::created(
    StoredKeyMetadata metadata,
    std::filesystem::path path
) {
    KeyStoreCreateResult result;
    result.m_status = KeyStoreStatus::OK;
    result.m_reason = "";
    result.m_metadata = std::move(metadata);
    result.m_path = std::move(path);
    return result;
}

KeyStoreCreateResult KeyStoreCreateResult::rejected(
    KeyStoreStatus status,
    std::string reason
) {
    KeyStoreCreateResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

KeyStoreStatus KeyStoreCreateResult::status() const {
    return m_status;
}

const std::string& KeyStoreCreateResult::reason() const {
    return m_reason;
}

const StoredKeyMetadata& KeyStoreCreateResult::metadata() const {
    return m_metadata;
}

const std::filesystem::path& KeyStoreCreateResult::path() const {
    return m_path;
}

bool KeyStoreCreateResult::success() const {
    return m_status == KeyStoreStatus::OK &&
           m_metadata.isValid();
}

KeyStoreLoadResult::KeyStoreLoadResult()
    : m_status(KeyStoreStatus::IO_ERROR),
      m_reason("Uninitialized key store load result."),
      m_keyId(""),
      m_keyPair(),
      m_metadata() {}

KeyStoreLoadResult KeyStoreLoadResult::loaded(
    std::string keyId,
    KeyPair keyPair,
    StoredKeyMetadata metadata
) {
    KeyStoreLoadResult result;
    result.m_status = KeyStoreStatus::OK;
    result.m_reason = "";
    result.m_keyId = std::move(keyId);
    result.m_keyPair = std::move(keyPair);
    result.m_metadata = std::move(metadata);
    return result;
}

KeyStoreLoadResult KeyStoreLoadResult::rejected(
    KeyStoreStatus status,
    std::string reason
) {
    KeyStoreLoadResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

KeyStoreStatus KeyStoreLoadResult::status() const {
    return m_status;
}

const std::string& KeyStoreLoadResult::reason() const {
    return m_reason;
}

const std::string& KeyStoreLoadResult::keyId() const {
    return m_keyId;
}

const KeyPair& KeyStoreLoadResult::keyPair() const {
    return m_keyPair;
}

const StoredKeyMetadata& KeyStoreLoadResult::metadata() const {
    return m_metadata;
}

bool KeyStoreLoadResult::loaded() const {
    return m_status == KeyStoreStatus::OK &&
           m_keyPair.isValid() &&
           m_metadata.isValid();
}

KeyStoreListResult::KeyStoreListResult()
    : m_status(KeyStoreStatus::IO_ERROR),
      m_reason("Uninitialized key store list result."),
      m_keys() {}

KeyStoreListResult KeyStoreListResult::loaded(
    std::vector<StoredKeyMetadata> keys
) {
    KeyStoreListResult result;
    result.m_status = KeyStoreStatus::OK;
    result.m_reason = "";
    result.m_keys = std::move(keys);
    return result;
}

KeyStoreListResult KeyStoreListResult::rejected(
    KeyStoreStatus status,
    std::string reason
) {
    KeyStoreListResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

KeyStoreStatus KeyStoreListResult::status() const {
    return m_status;
}

const std::string& KeyStoreListResult::reason() const {
    return m_reason;
}

const std::vector<StoredKeyMetadata>& KeyStoreListResult::keys() const {
    return m_keys;
}

bool KeyStoreListResult::loaded() const {
    return m_status == KeyStoreStatus::OK;
}

KeyStoreCreateResult KeyStore::createLocalKey(
    const std::filesystem::path& keysDirectory,
    const std::string& keyId,
    const std::string& seed,
    std::int64_t createdAt
) {
    if (!isSafeKeyId(keyId) ||
        seed.empty() ||
        createdAt <= 0) {
        return KeyStoreCreateResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Key creation input is invalid."
        );
    }

    try {
        std::filesystem::create_directories(keysDirectory);

        const std::filesystem::path path =
            keyFilePath(keysDirectory, keyId);

        if (std::filesystem::exists(path)) {
            return KeyStoreCreateResult::rejected(
                KeyStoreStatus::ALREADY_EXISTS,
                "Key already exists."
            );
        }

        const KeyPair keyPair =
            KeyPair::createDevelopmentKeyPair(seed);

        const StoredKeyMetadata metadata(
            keyId,
            keyPair.algorithm(),
            LOCAL_PROVIDER,
            keyPair.publicKey(),
            keyPair.address().value(),
            createdAt
        );

        if (!metadata.isValid()) {
            return KeyStoreCreateResult::rejected(
                KeyStoreStatus::INVALID_INPUT,
                "Generated key metadata is invalid."
            );
        }

        storage::AtomicFile::writeTextFile(
            path,
            keyFileContents(
                keyId,
                keyPair,
                createdAt
            )
        );

        writeIndex(keysDirectory);

        return KeyStoreCreateResult::created(
            metadata,
            path
        );
    } catch (const std::exception& error) {
        return KeyStoreCreateResult::rejected(
            KeyStoreStatus::IO_ERROR,
            error.what()
        );
    }
}

KeyStoreLoadResult KeyStore::loadKey(
    const std::filesystem::path& keysDirectory,
    const std::string& keyId
) {
    if (!isSafeKeyId(keyId)) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Key id is invalid."
        );
    }

    const std::filesystem::path path =
        keyFilePath(keysDirectory, keyId);

    if (!std::filesystem::exists(path)) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::NOT_FOUND,
            "Key was not found."
        );
    }

    try {
        return decodeKeyFile(
            keyId,
            storage::AtomicFile::readTextFile(path)
        );
    } catch (const std::exception& error) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::IO_ERROR,
            error.what()
        );
    }
}

KeyStoreListResult KeyStore::listKeys(
    const std::filesystem::path& keysDirectory
) {
    std::vector<StoredKeyMetadata> keys;

    if (!std::filesystem::exists(keysDirectory)) {
        return KeyStoreListResult::loaded(keys);
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(keysDirectory)) {
            if (!entry.is_regular_file() ||
                entry.path().filename() == "index.nodo" ||
                entry.path().extension() != ".nodo") {
                continue;
            }

            const std::string filename =
                entry.path().filename().string();

            if (filename.rfind("key_", 0) != 0) {
                continue;
            }

            const std::string keyId =
                filename.substr(
                    4,
                    filename.size() - 4 - std::string(".nodo").size()
                );

            const KeyStoreLoadResult loaded =
                loadKey(
                    keysDirectory,
                    keyId
                );

            if (!loaded.loaded()) {
                return KeyStoreListResult::rejected(
                    loaded.status(),
                    loaded.reason()
                );
            }

            keys.push_back(loaded.metadata());
        }

        std::sort(
            keys.begin(),
            keys.end(),
            [](const StoredKeyMetadata& left, const StoredKeyMetadata& right) {
                return left.keyId() < right.keyId();
            }
        );

        return KeyStoreListResult::loaded(keys);
    } catch (const std::exception& error) {
        return KeyStoreListResult::rejected(
            KeyStoreStatus::IO_ERROR,
            error.what()
        );
    }
}

std::filesystem::path KeyStore::keyFilePath(
    const std::filesystem::path& keysDirectory,
    const std::string& keyId
) {
    if (!isSafeKeyId(keyId)) {
        throw std::invalid_argument("Unsafe key id for key file path.");
    }

    return keysDirectory / ("key_" + keyId + ".nodo");
}

std::filesystem::path KeyStore::indexFilePath(
    const std::filesystem::path& keysDirectory
) {
    return keysDirectory / "index.nodo";
}

bool KeyStore::isSafeKeyId(
    const std::string& keyId
) {
    if (keyId.empty() || keyId.size() > 80) {
        return false;
    }

    for (const char character : keyId) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::string KeyStore::keyFileContents(
    const std::string& keyId,
    const KeyPair& keyPair,
    std::int64_t createdAt
) {
    return serialization::KeyValueFileCodec::serialize(
        KEY_FILE_VERSION,
        {
            {"keyId", keyId},
            {"algorithm", cryptoAlgorithmToString(keyPair.algorithm())},
            {"provider", LOCAL_PROVIDER},
            {"publicKeyMaterial", keyPair.publicKey().keyMaterial()},
            {"privateKeyMaterial", keyPair.privateKeyForSigningOnly().keyMaterialForSigningOnly()},
            {"address", keyPair.address().value()},
            {"createdAt", std::to_string(createdAt)}
        }
    );
}

KeyStoreLoadResult KeyStore::decodeKeyFile(
    const std::string& keyId,
    const std::string& contents
) {
    const serialization::KeyValueFileDocument document =
        serialization::KeyValueFileCodec::parse(
            contents,
            KEY_FILE_VERSION
        );

    document.requireOnlyFields(
        {
            "keyId",
            "algorithm",
            "provider",
            "publicKeyMaterial",
            "privateKeyMaterial",
            "address",
            "createdAt"
        }
    );

    if (document.requireField("keyId") != keyId) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Key file id does not match requested key id."
        );
    }

    const CryptoAlgorithm algorithm =
        cryptoAlgorithmFromString(
            document.requireField("algorithm")
        );

    if (cryptoAlgorithmToString(algorithm) != document.requireField("algorithm")) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Unknown key algorithm."
        );
    }

    const KeyPair keyPair(
        PublicKey(
            algorithm,
            document.requireField("publicKeyMaterial")
        ),
        PrivateKey(
            algorithm,
            document.requireField("privateKeyMaterial")
        )
    );

    const StoredKeyMetadata metadata(
        keyId,
        algorithm,
        document.requireField("provider"),
        keyPair.publicKey(),
        document.requireField("address"),
        std::stoll(document.requireField("createdAt"))
    );

    if (!keyPair.isValid() ||
        !metadata.isValid()) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Parsed key file is invalid."
        );
    }

    if (metadata.address() != keyPair.address().value()) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Key file address does not match public key."
        );
    }

    return KeyStoreLoadResult::loaded(
        keyId,
        keyPair,
        metadata
    );
}

std::string KeyStore::indexFileContents(
    const std::vector<StoredKeyMetadata>& keys
) {
    std::vector<std::pair<std::string, std::string>> fields = {
        {"keyCount", std::to_string(keys.size())}
    };

    for (std::size_t index = 0; index < keys.size(); ++index) {
        fields.emplace_back(
            "key." + std::to_string(index),
            keys[index].keyId()
        );
    }

    return serialization::KeyValueFileCodec::serialize(
        KEY_INDEX_VERSION,
        fields
    );
}

void KeyStore::writeIndex(
    const std::filesystem::path& keysDirectory
) {
    const KeyStoreListResult listed =
        listKeys(keysDirectory);

    if (!listed.loaded()) {
        throw std::runtime_error("Cannot rebuild key index: " + listed.reason());
    }

    storage::AtomicFile::writeTextFile(
        indexFilePath(keysDirectory),
        indexFileContents(listed.keys())
    );
}

} // namespace nodo::crypto
