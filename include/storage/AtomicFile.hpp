#ifndef NODO_STORAGE_ATOMIC_FILE_HPP
#define NODO_STORAGE_ATOMIC_FILE_HPP

#include <filesystem>
#include <string>

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
};

} // namespace nodo::storage

#endif
