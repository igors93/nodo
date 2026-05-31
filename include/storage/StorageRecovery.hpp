#ifndef NODO_STORAGE_STORAGE_RECOVERY_HPP
#define NODO_STORAGE_STORAGE_RECOVERY_HPP

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace nodo::storage {

class StorageRecoveryResult {
public:
    StorageRecoveryResult();

    StorageRecoveryResult(
        std::size_t quarantinedFileCount,
        std::vector<std::string> quarantinedFiles
    );

    std::size_t quarantinedFileCount() const;
    const std::vector<std::string>& quarantinedFiles() const;
    bool recovered() const;

    std::string serialize() const;

private:
    std::size_t m_quarantinedFileCount;
    std::vector<std::string> m_quarantinedFiles;
};

class StorageRecovery {
public:
    static StorageRecoveryResult quarantineTemporaryWrites(
        const std::filesystem::path& rootDirectory
    );

    static std::filesystem::path quarantineDirectory(
        const std::filesystem::path& rootDirectory
    );
};

} // namespace nodo::storage

#endif
