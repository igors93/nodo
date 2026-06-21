#include "storage/StorageMigration.hpp"

#include "storage/AtomicFile.hpp"
#include "storage/StorageSchemaVersion.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace nodo::storage {

namespace {

// v1 → v2: ensure the peers/ sub-directory exists (added in schema v2).
bool migrateV1toV2(const std::filesystem::path& dataRoot) {
    const std::filesystem::path peersDir = dataRoot / "peers";
    if (!std::filesystem::exists(peersDir)) {
        std::error_code ec;
        std::filesystem::create_directories(peersDir, ec);
        if (ec) return false;
    }

    // Rewrite the schema version file to v2.
    const StorageSchemaVersion v2(
        StorageSchemaVersion::nodeDataDirectorySchemaId(),
        2,
        1
    );
    storage::AtomicFile::writeTextFile(
        StorageSchemaVersionFile::pathForRoot(dataRoot),
        v2.toFileContents()
    );

    return true;
}

} // namespace

std::string storageMigrationStatusToString(StorageMigrationStatus status) {
    switch (status) {
        case StorageMigrationStatus::OK:                 return "OK";
        case StorageMigrationStatus::ALREADY_CURRENT:    return "ALREADY_CURRENT";
        case StorageMigrationStatus::UNSUPPORTED_DOWNGRADE: return "UNSUPPORTED_DOWNGRADE";
        case StorageMigrationStatus::MIGRATION_FAILED:   return "MIGRATION_FAILED";
        case StorageMigrationStatus::INVALID_INPUT:      return "INVALID_INPUT";
        default:                                         return "INVALID_INPUT";
    }
}

StorageMigrationResult::StorageMigrationResult()
    : m_status(StorageMigrationStatus::INVALID_INPUT)
    , m_reason("Uninitialized.")
    , m_finalVersion(0)
{}

StorageMigrationResult StorageMigrationResult::ok(std::uint64_t finalVersion) {
    StorageMigrationResult r;
    r.m_status = StorageMigrationStatus::OK;
    r.m_reason = "";
    r.m_finalVersion = finalVersion;
    return r;
}

StorageMigrationResult StorageMigrationResult::alreadyCurrent(std::uint64_t version) {
    StorageMigrationResult r;
    r.m_status = StorageMigrationStatus::ALREADY_CURRENT;
    r.m_reason = "";
    r.m_finalVersion = version;
    return r;
}

StorageMigrationResult StorageMigrationResult::rejected(
    StorageMigrationStatus status,
    std::string reason
) {
    StorageMigrationResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    r.m_finalVersion = 0;
    return r;
}

StorageMigrationStatus StorageMigrationResult::status() const { return m_status; }
const std::string&     StorageMigrationResult::reason()  const { return m_reason; }
std::uint64_t          StorageMigrationResult::finalVersion() const { return m_finalVersion; }
bool                   StorageMigrationResult::success()  const {
    return m_status == StorageMigrationStatus::OK ||
           m_status == StorageMigrationStatus::ALREADY_CURRENT;
}

std::string StorageMigrationResult::serialize() const {
    std::ostringstream oss;
    oss << "StorageMigrationResult{"
        << "status=" << storageMigrationStatusToString(m_status)
        << ";reason=" << m_reason
        << ";finalVersion=" << m_finalVersion
        << "}";
    return oss.str();
}

void StorageMigrationRunner::registerStep(StorageMigrationStep step) {
    m_steps.push_back(std::move(step));
}

const std::vector<StorageMigrationStep>& StorageMigrationRunner::steps() const {
    return m_steps;
}

StorageMigrationResult StorageMigrationRunner::migrate(
    const std::filesystem::path& dataRoot,
    std::uint64_t                currentVersion,
    std::uint64_t                targetVersion
) const {
    if (currentVersion == targetVersion) {
        return StorageMigrationResult::alreadyCurrent(currentVersion);
    }
    if (currentVersion > targetVersion) {
        return StorageMigrationResult::rejected(
            StorageMigrationStatus::UNSUPPORTED_DOWNGRADE,
            "Downgrade from v" + std::to_string(currentVersion)
            + " to v" + std::to_string(targetVersion)
            + " is not supported. Restore from a compatible backup."
        );
    }
    if (!std::filesystem::exists(dataRoot)) {
        return StorageMigrationResult::rejected(
            StorageMigrationStatus::INVALID_INPUT,
            "Data root does not exist: " + dataRoot.string()
        );
    }

    // Collect steps that advance from currentVersion toward targetVersion,
    // sorted by fromVersion so they run in sequence.
    std::vector<const StorageMigrationStep*> applicable;
    for (const auto& step : m_steps) {
        if (step.fromVersion >= currentVersion &&
            step.toVersion   <= targetVersion) {
            applicable.push_back(&step);
        }
    }
    std::sort(
        applicable.begin(), applicable.end(),
        [](const StorageMigrationStep* a, const StorageMigrationStep* b) {
            return a->fromVersion < b->fromVersion;
        }
    );

    std::uint64_t version = currentVersion;
    for (const auto* step : applicable) {
        if (step->fromVersion != version) {
            return StorageMigrationResult::rejected(
                StorageMigrationStatus::MIGRATION_FAILED,
                "Missing migration step from v" + std::to_string(version)
                + " to v" + std::to_string(step->fromVersion)
            );
        }
        if (!step->apply(dataRoot)) {
            return StorageMigrationResult::rejected(
                StorageMigrationStatus::MIGRATION_FAILED,
                "Migration step v" + std::to_string(step->fromVersion)
                + "->v" + std::to_string(step->toVersion)
                + " (" + step->description + ") failed."
            );
        }
        version = step->toVersion;
    }

    if (version != targetVersion) {
        return StorageMigrationResult::rejected(
            StorageMigrationStatus::MIGRATION_FAILED,
            "No complete migration path from v" + std::to_string(currentVersion)
            + " to v" + std::to_string(targetVersion)
        );
    }

    return StorageMigrationResult::ok(version);
}

StorageMigrationRunner StorageMigrationRunner::defaultRunner() {
    StorageMigrationRunner runner;
    runner.registerStep({
        1, 2,
        "Add peers sub-directory",
        migrateV1toV2
    });
    return runner;
}

} // namespace nodo::storage
