#ifndef NODO_STORAGE_STORAGE_MIGRATION_HPP
#define NODO_STORAGE_STORAGE_MIGRATION_HPP

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace nodo::storage {

/*
 * StorageMigrationStep describes one version-to-version migration.
 *
 * A step is applied when the on-disk schema version equals fromVersion and
 * the binary expects toVersion (or higher).  Steps must be idempotent: if the
 * migration is interrupted and restarted, applying it again must produce the
 * same result.
 *
 * The migration function receives the node data directory root path and returns
 * true on success, false on failure.  It should never delete data without first
 * creating a backup, and must write all output files atomically.
 */
struct StorageMigrationStep {
    std::uint64_t fromVersion;
    std::uint64_t toVersion;
    std::string   description;

    std::function<bool(const std::filesystem::path& dataRoot)> apply;
};

enum class StorageMigrationStatus {
    OK,
    ALREADY_CURRENT,
    UNSUPPORTED_DOWNGRADE,
    MIGRATION_FAILED,
    INVALID_INPUT
};

std::string storageMigrationStatusToString(StorageMigrationStatus status);

class StorageMigrationResult {
public:
    StorageMigrationResult();

    static StorageMigrationResult ok(std::uint64_t finalVersion);
    static StorageMigrationResult alreadyCurrent(std::uint64_t version);
    static StorageMigrationResult rejected(
        StorageMigrationStatus status,
        std::string reason
    );

    StorageMigrationStatus status() const;
    const std::string&     reason() const;
    std::uint64_t          finalVersion() const;

    bool success() const;
    std::string serialize() const;

private:
    StorageMigrationStatus m_status;
    std::string            m_reason;
    std::uint64_t          m_finalVersion;
};

/*
 * StorageMigrationRunner applies a sequence of registered steps to bring a
 * node data directory from its current schema version to the target version.
 *
 * Usage:
 *   StorageMigrationRunner runner;
 *   runner.registerStep({ 1, 2, "Add peers directory", migratev1tov2 });
 *   auto result = runner.migrate(dataRoot, currentVersion, targetVersion);
 *
 * The runner applies steps in ascending fromVersion order.  If any step fails,
 * migration halts and the partially-migrated directory should be considered
 * invalid (the node should exit and let the operator restore from backup).
 */
class StorageMigrationRunner {
public:
    StorageMigrationRunner() = default;

    void registerStep(StorageMigrationStep step);

    StorageMigrationResult migrate(
        const std::filesystem::path& dataRoot,
        std::uint64_t                currentVersion,
        std::uint64_t                targetVersion
    ) const;

    /*
     * Returns a pre-configured runner with all built-in migration steps.
     */
    static StorageMigrationRunner defaultRunner();

    const std::vector<StorageMigrationStep>& steps() const;

private:
    std::vector<StorageMigrationStep> m_steps;
};

} // namespace nodo::storage

#endif
