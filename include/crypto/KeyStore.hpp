#ifndef NODO_CRYPTO_KEY_STORE_HPP
#define NODO_CRYPTO_KEY_STORE_HPP

#include "crypto/KeyPair.hpp"
#include "crypto/LocalIdentity.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nodo::crypto {

class StoredKeyMetadata {
public:
    StoredKeyMetadata();

    StoredKeyMetadata(
        std::string keyId,
        CryptoAlgorithm algorithm,
        std::string provider,
        PublicKey publicKey,
        std::string address,
        std::int64_t createdAt,
        std::string networkProfile = "localnet"
    );

    const std::string& keyId() const;
    CryptoAlgorithm algorithm() const;
    const std::string& provider() const;
    const PublicKey& publicKey() const;
    const std::string& address() const;
    std::int64_t createdAt() const;
    const std::string& networkProfile() const;

    bool isLocalnetOnly() const;
    bool isValid() const;
    std::string serializePublic() const;

private:
    std::string m_keyId;
    CryptoAlgorithm m_algorithm;
    std::string m_provider;
    PublicKey m_publicKey;
    std::string m_address;
    std::int64_t m_createdAt;
    std::string m_networkProfile;
};

enum class KeyStoreStatus {
    OK,
    ALREADY_EXISTS,
    NOT_FOUND,
    INVALID_INPUT,
    IO_ERROR
};

std::string keyStoreStatusToString(
    KeyStoreStatus status
);

class KeyStoreCreateResult {
public:
    KeyStoreCreateResult();

    static KeyStoreCreateResult created(
        StoredKeyMetadata metadata,
        std::filesystem::path path
    );

    static KeyStoreCreateResult rejected(
        KeyStoreStatus status,
        std::string reason
    );

    KeyStoreStatus status() const;
    const std::string& reason() const;
    const StoredKeyMetadata& metadata() const;
    const std::filesystem::path& path() const;
    bool success() const;

private:
    KeyStoreStatus m_status;
    std::string m_reason;
    StoredKeyMetadata m_metadata;
    std::filesystem::path m_path;
};

class KeyStoreLoadResult {
public:
    KeyStoreLoadResult();

    static KeyStoreLoadResult loaded(
        std::string keyId,
        KeyPair keyPair,
        StoredKeyMetadata metadata
    );

    static KeyStoreLoadResult rejected(
        KeyStoreStatus status,
        std::string reason
    );

    KeyStoreStatus status() const;
    const std::string& reason() const;
    const std::string& keyId() const;
    const KeyPair& keyPair() const;
    const StoredKeyMetadata& metadata() const;
    bool loaded() const;

private:
    KeyStoreStatus m_status;
    std::string m_reason;
    std::string m_keyId;
    KeyPair m_keyPair;
    StoredKeyMetadata m_metadata;
};

class KeyStoreListResult {
public:
    KeyStoreListResult();

    static KeyStoreListResult loaded(
        std::vector<StoredKeyMetadata> keys
    );

    static KeyStoreListResult rejected(
        KeyStoreStatus status,
        std::string reason
    );

    KeyStoreStatus status() const;
    const std::string& reason() const;
    const std::vector<StoredKeyMetadata>& keys() const;
    bool loaded() const;

private:
    KeyStoreStatus m_status;
    std::string m_reason;
    std::vector<StoredKeyMetadata> m_keys;
};

class KeyStore {
public:
    static constexpr const char* LOCAL_PROVIDER =
        "LOCAL_DETERMINISTIC_PROVIDER_V1";

    static constexpr const char* LOCAL_NETWORK_PROFILE =
        "localnet";

    static KeyStoreCreateResult createLocalKey(
        const std::filesystem::path& keysDirectory,
        const std::string& keyId,
        const std::string& seed,
        std::int64_t createdAt
    );

    static KeyStoreLoadResult loadKey(
        const std::filesystem::path& keysDirectory,
        const std::string& keyId
    );

    static KeyStoreListResult listKeys(
        const std::filesystem::path& keysDirectory
    );

    static std::filesystem::path keyFilePath(
        const std::filesystem::path& keysDirectory,
        const std::string& keyId
    );

    static std::filesystem::path indexFilePath(
        const std::filesystem::path& keysDirectory
    );

    static bool isSafeKeyId(
        const std::string& keyId
    );

private:
    static std::string keyFileContents(
        const std::string& keyId,
        const KeyPair& keyPair,
        std::int64_t createdAt
    );

    static KeyStoreLoadResult decodeKeyFile(
        const std::string& keyId,
        const std::string& contents
    );

    static std::string indexFileContents(
        const std::vector<StoredKeyMetadata>& keys
    );

    static void writeIndex(
        const std::filesystem::path& keysDirectory
    );
};

} // namespace nodo::crypto

#endif
