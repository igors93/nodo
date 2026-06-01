#ifndef NODO_STORAGE_STORAGE_SCHEMA_VERSION_HPP
#define NODO_STORAGE_STORAGE_SCHEMA_VERSION_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace nodo::storage {

enum class StorageSchemaValidationStatus {
    ACCEPTED,
    MISSING_VERSION_FILE,
    UNSUPPORTED_SCHEMA,
    UNSUPPORTED_VERSION,
    DOWNGRADE_REJECTED,
    NON_CANONICAL,
    IO_ERROR
};

std::string storageSchemaValidationStatusToString(
    StorageSchemaValidationStatus status
);

class StorageSchemaVersion {
public:
    StorageSchemaVersion();

    StorageSchemaVersion(
        std::string schemaId,
        std::uint64_t version,
        std::uint64_t minimumCompatibleVersion
    );

    const std::string& schemaId() const;
    std::uint64_t version() const;
    std::uint64_t minimumCompatibleVersion() const;

    bool isStructurallyValid() const;
    bool isSupportedNodeDataDirectoryVersion() const;

    std::string serialize() const;
    std::string toFileContents() const;

    static std::string schemaFileName();
    static std::string nodeDataDirectorySchemaId();
    static std::uint64_t currentNodeDataDirectoryVersion();
    static std::uint64_t minimumSupportedNodeDataDirectoryVersion();

    static StorageSchemaVersion currentNodeDataDirectorySchema();

    static StorageSchemaVersion fromFileContents(
        const std::string& contents
    );

    static void writeCurrentNodeDataDirectoryVersionFile(
        const std::filesystem::path& rootPath
    );

private:
    std::string m_schemaId;
    std::uint64_t m_version;
    std::uint64_t m_minimumCompatibleVersion;
};

class StorageSchemaValidationResult {
public:
    StorageSchemaValidationResult();

    static StorageSchemaValidationResult accepted(
        StorageSchemaVersion schema
    );

    static StorageSchemaValidationResult rejected(
        StorageSchemaValidationStatus status,
        std::string reason
    );

    StorageSchemaValidationStatus status() const;
    const std::string& reason() const;
    const std::optional<StorageSchemaVersion>& schema() const;

    bool accepted() const;
    std::string serialize() const;

private:
    StorageSchemaValidationStatus m_status;
    std::string m_reason;
    std::optional<StorageSchemaVersion> m_schema;
};

class StorageSchemaVersionFile {
public:
    static std::filesystem::path pathForRoot(
        const std::filesystem::path& rootPath
    );

    static StorageSchemaValidationResult validateNodeDataDirectoryRoot(
        const std::filesystem::path& rootPath
    );
};

} // namespace nodo::storage

#endif
