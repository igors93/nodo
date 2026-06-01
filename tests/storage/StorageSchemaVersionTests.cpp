#include "storage/StorageSchemaVersion.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::storage::StorageSchemaValidationStatus;
using nodo::storage::StorageSchemaVersion;
using nodo::storage::StorageSchemaVersionFile;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path tempPath(
    const std::string& suffix
) {
    return std::filesystem::temp_directory_path()
        / ("nodo-storage-schema-version-tests-" + suffix);
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

void writeFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    std::ofstream output(path, std::ios::trunc);
    output << contents;
}

void testWritesAndLoadsCurrentNodeDataDirectorySchema() {
    const std::filesystem::path path =
        tempPath("current");

    clean(path);

    StorageSchemaVersion::writeCurrentNodeDataDirectoryVersionFile(path);

    const auto validation =
        StorageSchemaVersionFile::validateNodeDataDirectoryRoot(path);

    requireCondition(
        validation.accepted(),
        "Current node data directory storage schema should validate."
    );

    requireCondition(
        validation.schema().has_value() &&
        validation.schema()->version() == StorageSchemaVersion::currentNodeDataDirectoryVersion(),
        "Accepted storage schema should expose the current version."
    );

    clean(path);
}

void testRejectsMissingSchemaVersionFile() {
    const std::filesystem::path path =
        tempPath("missing");

    clean(path);
    std::filesystem::create_directories(path);

    const auto validation =
        StorageSchemaVersionFile::validateNodeDataDirectoryRoot(path);

    requireCondition(
        !validation.accepted() &&
        validation.status() == StorageSchemaValidationStatus::MISSING_VERSION_FILE,
        "Missing storage schema version file should be rejected."
    );

    clean(path);
}

void testRejectsUnknownFutureSchemaVersion() {
    const std::filesystem::path path =
        tempPath("future");

    clean(path);
    std::filesystem::create_directories(path);

    writeFile(
        StorageSchemaVersionFile::pathForRoot(path),
        "NODO_STORAGE_SCHEMA_VERSION_V1\n"
        "schemaId=NODO_NODE_DATA_DIRECTORY\n"
        "version=999\n"
        "minimumCompatibleVersion=999\n"
    );

    const auto validation =
        StorageSchemaVersionFile::validateNodeDataDirectoryRoot(path);

    requireCondition(
        !validation.accepted() &&
        validation.status() == StorageSchemaValidationStatus::UNSUPPORTED_VERSION,
        "Unknown future storage schema version should be rejected."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testWritesAndLoadsCurrentNodeDataDirectorySchema();
        testRejectsMissingSchemaVersionFile();
        testRejectsUnknownFutureSchemaVersion();

        std::cout << "Nodo storage schema version tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo storage schema version tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
