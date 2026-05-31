#ifndef NODO_STORAGE_ATOMIC_FILE_HPP
#define NODO_STORAGE_ATOMIC_FILE_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace nodo::storage {

class AtomicFile {
public:
    static void writeTextFile(
        const std::filesystem::path& path,
        const std::string& contents
    );

    static std::string readTextFile(
        const std::filesystem::path& path
    );

    static bool isTemporaryWriteFile(
        const std::filesystem::path& path
    );

    static std::vector<std::filesystem::path> listTemporaryWriteFiles(
        const std::filesystem::path& directory
    );

    static std::size_t removeTemporaryWriteFiles(
        const std::filesystem::path& directory
    );

private:
    static std::filesystem::path makeTemporaryPath(
        const std::filesystem::path& path
    );
};

} // namespace nodo::storage

#endif
