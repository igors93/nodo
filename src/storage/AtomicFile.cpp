#include "storage/AtomicFile.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace nodo::storage {

void AtomicFile::writeTextFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    if (path.empty()) {
        throw std::invalid_argument("Atomic write path cannot be empty.");
    }

    const std::filesystem::path parent =
        path.parent_path();

    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    const std::filesystem::path temporaryPath =
        path.parent_path() / (path.filename().string() + ".tmp");

    if (std::filesystem::exists(temporaryPath)) {
        throw std::runtime_error("Atomic write temporary file already exists: " + temporaryPath.string());
    }

    {
        std::ofstream output(
            temporaryPath,
            std::ios::out | std::ios::trunc
        );

        if (!output) {
            throw std::runtime_error("Unable to open temporary file for writing: " + temporaryPath.string());
        }

        output << contents;
        output.flush();

        if (!output) {
            throw std::runtime_error("Unable to write temporary file: " + temporaryPath.string());
        }
    }

    std::error_code renameError;
    std::filesystem::rename(
        temporaryPath,
        path,
        renameError
    );

    if (renameError) {
        const std::string firstRenameError =
            renameError.message();

        if (std::filesystem::exists(path)) {
            std::error_code removeError;
            std::filesystem::remove(
                path,
                removeError
            );

            if (!removeError) {
                renameError.clear();
                std::filesystem::rename(
                    temporaryPath,
                    path,
                    renameError
                );
            }
        }

        if (renameError) {
            std::error_code cleanupError;
            std::filesystem::remove(
                temporaryPath,
                cleanupError
            );

            throw std::runtime_error("Atomic file rename failed: " + firstRenameError);
        }
    }
}

std::string AtomicFile::readTextFile(
    const std::filesystem::path& path
) {
    std::ifstream input(
        path,
        std::ios::in | std::ios::binary
    );

    if (!input) {
        throw std::runtime_error("Unable to open file for reading: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (!input.good() && !input.eof()) {
        throw std::runtime_error("Unable to read file: " + path.string());
    }

    return buffer.str();
}

} // namespace nodo::storage
