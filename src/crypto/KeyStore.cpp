#include "crypto/KeyStore.hpp"

#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyEncryptionService.hpp"
#include "crypto/AddressDerivation.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>


#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

namespace nodo::crypto {

namespace {

constexpr const char* KEY_FILE_VERSION_V3 =
    "NODO_KEY_FILE_V3";

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

serialization::KeyValueFileDocument parseKeyFileDocument(
    const std::string& contents
) {
    serialization::KeyValueFileDocument document =
        serialization::KeyValueFileCodec::parse(
            contents,
            KEY_FILE_VERSION_V3
        );

    document.requireOnlyFields(
        {
            "keyId",
            "algorithm",
            "suite",
            "keyType",
            "provider",
            "networkProfile",
            "publicKeyMaterial",
            "privateKeyMaterial",
            "address",
            "createdAt"
        }
    );

    return document;
}

} // namespace

std::string keyStoreKeyTypeToString(
    KeyStoreKeyType keyType
) {
    switch (keyType) {
        case KeyStoreKeyType::USER:
            return "user";
        case KeyStoreKeyType::VALIDATOR:
            return "validator";
        default:
            return "user";
    }
}

KeyStoreKeyType keyStoreKeyTypeFromString(
    const std::string& value
) {
    if (value == "validator") {
        return KeyStoreKeyType::VALIDATOR;
    }

    return KeyStoreKeyType::USER;
}

StoredKeyMetadata::StoredKeyMetadata()
    : m_keyId(""),
      m_algorithm(CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE),
      m_suite(CryptoSuiteId::UNKNOWN),
      m_keyType(KeyStoreKeyType::USER),
      m_provider(""),
      m_publicKey(),
      m_address(""),
      m_createdAt(0),
      m_networkProfile(""),
      m_encryptionLevel(KeyEncryptionLevel::PLAINTEXT) {}

StoredKeyMetadata::StoredKeyMetadata(
    std::string keyId,
    CryptoAlgorithm algorithm,
    CryptoSuiteId suite,
    KeyStoreKeyType keyType,
    std::string provider,
    PublicKey publicKey,
    std::string address,
    std::int64_t createdAt,
    std::string networkProfile,
    KeyEncryptionLevel encryptionLevel
)
    : m_keyId(std::move(keyId)),
      m_algorithm(algorithm),
      m_suite(suite),
      m_keyType(keyType),
      m_provider(std::move(provider)),
      m_publicKey(std::move(publicKey)),
      m_address(std::move(address)),
      m_createdAt(createdAt),
      m_networkProfile(std::move(networkProfile)),
      m_encryptionLevel(encryptionLevel) {}

const std::string& StoredKeyMetadata::keyId() const {
    return m_keyId;
}

CryptoAlgorithm StoredKeyMetadata::algorithm() const {
    return m_algorithm;
}

CryptoSuiteId StoredKeyMetadata::suite() const {
    return m_suite;
}

KeyStoreKeyType StoredKeyMetadata::keyType() const {
    return m_keyType;
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

const std::string& StoredKeyMetadata::networkProfile() const {
    return m_networkProfile;
}

KeyEncryptionLevel StoredKeyMetadata::encryptionLevel() const {
    return m_encryptionLevel;
}

bool StoredKeyMetadata::isLocalnetOnly() const {
    return m_networkProfile == KeyStore::LOCAL_NETWORK_PROFILE;
}

bool StoredKeyMetadata::isValid() const {
    if (!KeyStore::isSafeKeyId(m_keyId)) { return false; }
    if (!isSafeScalar(m_provider)) { return false; }
    if (!isSafeScalar(m_networkProfile)) { return false; }
    if (!isSupportedCryptoSuite(m_suite)) { return false; }
    if (!m_publicKey.isValid()) { return false; }
    if (!isSafeScalar(m_publicKey.keyMaterial())) { return false; }
    if (!isSafeScalar(m_address)) { return false; }
    if (m_createdAt <= 0) { return false; }

    if (m_publicKey.algorithm() != m_algorithm) {
        return false;
    }

    if (m_keyType == KeyStoreKeyType::USER &&
        (m_algorithm != CryptoAlgorithm::CLASSIC_ED25519 ||
         m_provider != KeyStore::USER_PROVIDER)) {
        return false;
    }

    if (m_keyType == KeyStoreKeyType::VALIDATOR &&
        (m_algorithm != CryptoAlgorithm::BLS12_381 ||
         m_provider != KeyStore::VALIDATOR_PROVIDER)) {
        return false;
    }

    if (isDevelopmentOnlyAlgorithm(m_algorithm)) {
        return false;
    }

    if (!isLocalnetOnly()) {
        if (m_encryptionLevel == KeyEncryptionLevel::PLAINTEXT) {
            return false;
        }
        if (!KeyEncryptionPolicy::isAcceptable(m_encryptionLevel, m_networkProfile)) {
            return false;
        }
    }

    return LocalNodeKeyIdentity(
        m_keyId,
        m_publicKey,
        m_address
    ).isValid();
}

std::string StoredKeyMetadata::serializePublic() const {
    std::ostringstream oss;

    oss << "StoredKeyMetadata{"
        << "keyId=" << m_keyId
        << ";keyType=" << keyStoreKeyTypeToString(m_keyType)
        << ";suite=" << cryptoSuiteIdToString(m_suite)
        << ";algorithm=" << cryptoAlgorithmToString(m_algorithm)
        << ";provider=" << m_provider
        << ";networkProfile=" << m_networkProfile
        << ";encryptionLevel=" << keyEncryptionLevelToString(m_encryptionLevel)
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
    std::int64_t createdAt,
    const std::string& password,
    const std::string& networkProfile
) {
    return createLocalKey(
        keysDirectory,
        keyId,
        KeyStoreKeyType::USER,
        seed,
        createdAt,
        password,
        networkProfile
    );
}

KeyStoreCreateResult KeyStore::createLocalKey(
    const std::filesystem::path& keysDirectory,
    const std::string& keyId,
    KeyStoreKeyType keyType,
    const std::string& seed,
    std::int64_t createdAt,
    const std::string& password,
    const std::string& networkProfile
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
            keyType == KeyStoreKeyType::VALIDATOR
                ? KeyPair::createDeterministicBls12381KeyPair(seed)
                : KeyPair::createDeterministicEd25519KeyPair(seed);

        const std::string provider =
            keyType == KeyStoreKeyType::VALIDATOR
                ? VALIDATOR_PROVIDER
                : USER_PROVIDER;

        const KeyEncryptionLevel level = password.empty()
            ? KeyEncryptionLevel::PLAINTEXT
            : KeyEncryptionLevel::TESTNET_SAFE;

        const StoredKeyMetadata metadata(
            keyId,
            keyPair.algorithm(),
            CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
            keyType,
            provider,
            keyPair.publicKey(),
            keyPair.address().value(),
            createdAt,
            networkProfile,
            level
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
                keyType,
                keyPair,
                createdAt,
                password,
                networkProfile
            )
        );

#if defined(__unix__) || defined(__APPLE__)
        ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif

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
    const std::string& keyId,
    const std::string& password,
    bool metadataOnly
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
        // Plaintext keys require permissions validation.
        // We only enforce this check if metadataOnly is false, or if it is localnet.
#if defined(__unix__) || defined(__APPLE__)
        {
            struct ::stat info{};
            if (::stat(path.c_str(), &info) == 0) {
                const mode_t publicBits = info.st_mode & (S_IRWXG | S_IRWXO);
                if (publicBits != 0) {
                    return KeyStoreLoadResult::rejected(
                        KeyStoreStatus::INVALID_INPUT,
                        "Key file has unsafe permissions (group/other access detected). "
                        "This key store is localnet-only — it is not production custody. "
                        "Fix permissions with: chmod 600 " + path.string()
                    );
                }
            }
        }
#endif

        const KeyStoreLoadResult loaded =
            decodeKeyFile(
                keyId,
                storage::AtomicFile::readTextFile(path),
                password,
                metadataOnly
            );

        if (!loaded.loaded()) {
            return KeyStoreLoadResult::rejected(
                loaded.status(),
                "Invalid key file "
                + path.string()
                + ": "
                + loaded.reason()
            );
        }

        return loaded;
    } catch (const std::exception& error) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::IO_ERROR,
            "Invalid key file "
            + path.string()
            + ": "
            + error.what()
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
                    keyId,
                    "",
                    true
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
    KeyStoreKeyType keyType,
    const KeyPair& keyPair,
    std::int64_t createdAt,
    const std::string& password,
    const std::string& networkProfile
) {
    const std::string provider =
        keyType == KeyStoreKeyType::VALIDATOR
            ? VALIDATOR_PROVIDER
            : USER_PROVIDER;

    std::string privateKeyMat = keyPair.privateKeyForSigningOnly().keyMaterialForSigningOnly();
    if (!password.empty()) {
        privateKeyMat = KeyEncryptionService::encrypt(keyId, privateKeyMat, password);
    }

    return serialization::KeyValueFileCodec::serialize(
        KEY_FILE_VERSION_V3,
        {
            {"keyId", keyId},
            {"algorithm", cryptoAlgorithmToString(keyPair.algorithm())},
            {"suite", cryptoSuiteIdToString(CryptoSuiteId::NODO_CRYPTO_SUITE_V1)},
            {"keyType", keyStoreKeyTypeToString(keyType)},
            {"provider", provider},
            {"networkProfile", networkProfile},
            {"publicKeyMaterial", keyPair.publicKey().keyMaterial()},
            {"privateKeyMaterial", privateKeyMat},
            {"address", keyPair.address().value()},
            {"createdAt", std::to_string(createdAt)}
        }
    );
}

KeyStoreLoadResult KeyStore::decodeKeyFile(
    const std::string& keyId,
    const std::string& contents,
    const std::string& password,
    bool metadataOnly
) {
    const serialization::KeyValueFileDocument document =
        parseKeyFileDocument(contents);

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

    const CryptoSuiteId suite =
        cryptoSuiteIdFromString(
            document.requireField("suite")
        );

    if (!isSupportedCryptoSuite(suite)) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Unknown key crypto suite."
        );
    }

    const KeyStoreKeyType keyType =
        keyStoreKeyTypeFromString(
            document.requireField("keyType")
        );

    if (keyStoreKeyTypeToString(keyType) != document.requireField("keyType")) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Unknown key type."
        );
    }

    const std::string rawPrivateKeyMaterial = document.requireField("privateKeyMaterial");
    const bool isEncrypted = KeyEncryptionService::isEncryptedEnvelope(rawPrivateKeyMaterial);
    const KeyEncryptionLevel level = isEncrypted
        ? KeyEncryptionLevel::TESTNET_SAFE
        : KeyEncryptionLevel::PLAINTEXT;

    std::string decryptedPrivateKeyMaterial;
    if (isEncrypted) {
        if (metadataOnly) {
            // Keep it empty/dummy since we don't need the private key
            decryptedPrivateKeyMaterial = "";
        } else {
            if (password.empty()) {
                return KeyStoreLoadResult::rejected(
                    KeyStoreStatus::INVALID_INPUT,
                    "Password required to load encrypted key."
                );
            }
            decryptedPrivateKeyMaterial = KeyEncryptionService::decrypt(keyId, rawPrivateKeyMaterial, password);
            if (decryptedPrivateKeyMaterial.empty()) {
                return KeyStoreLoadResult::rejected(
                    KeyStoreStatus::INVALID_INPUT,
                    "Decryption failed (possibly wrong password)."
                );
            }
        }
    } else {
        decryptedPrivateKeyMaterial = rawPrivateKeyMaterial;
    }

    const KeyPair keyPair(
        PublicKey(
            algorithm,
            document.requireField("publicKeyMaterial")
        ),
        PrivateKey(
            algorithm,
            decryptedPrivateKeyMaterial
        )
    );

    const StoredKeyMetadata metadata(
        keyId,
        algorithm,
        suite,
        keyType,
        document.requireField("provider"),
        keyPair.publicKey(),
        document.requireField("address"),
        std::stoll(document.requireField("createdAt")),
        document.requireField("networkProfile"),
        level
    );

    if ((!metadataOnly && !keyPair.isValid()) || !metadata.isValid()) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Parsed key file is invalid."
        );
    }

    if (metadata.address() != AddressDerivation::deriveFromPublicKey(keyPair.publicKey()).value()) {
        return KeyStoreLoadResult::rejected(
            KeyStoreStatus::INVALID_INPUT,
            "Key file address does not match public key."
        );
    }

    if (level == KeyEncryptionLevel::PLAINTEXT) {
        if (contents != keyFileContents(
                keyId,
                metadata.keyType(),
                keyPair,
                metadata.createdAt(),
                "",
                metadata.networkProfile()
            )) {
            return KeyStoreLoadResult::rejected(
                KeyStoreStatus::INVALID_INPUT,
                "Key file is not canonical."
            );
        }
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
