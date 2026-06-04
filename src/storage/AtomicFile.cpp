#include "storage/AtomicFile.hpp"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <system_error>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace nodo::storage {

namespace {

constexpr const char* TEMPORARY_WRITE_MARKER =
    ".tmp.";

std::string monotonicSuffix() {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch();

    return std::to_string(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()
    );
}

bool hasTemporaryWriteMarker(
    const std::filesystem::path& path
) {
    const std::string filename =
        path.filename().string();

    return filename.find(TEMPORARY_WRITE_MARKER) != std::string::npos;
}

} // namespace

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
        makeTemporaryPath(path);

    if (std::filesystem::exists(temporaryPath)) {
        throw std::runtime_error(
            "Atomic write temporary file already exists: "
            + temporaryPath.string()
        );
    }

    try {
        {
            std::ofstream output(
                temporaryPath,
                std::ios::out | std::ios::binary | std::ios::trunc
            );

            if (!output) {
                throw std::runtime_error(
                    "Unable to open temporary file for writing: "
                    + temporaryPath.string()
                );
            }

            output << contents;
            output.flush();

            if (!output) {
                throw std::runtime_error(
                    "Unable to write temporary file: "
                    + temporaryPath.string()
                );
            }

#if defined(__unix__) || defined(__APPLE__)
            const int fd = ::open(
                temporaryPath.c_str(),
                O_RDONLY
            );

            if (fd >= 0) {
                ::fsync(fd);
                ::close(fd);
            }
#endif
        }

        std::error_code renameError;

        std::filesystem::rename(
            temporaryPath,
            path,
            renameError
        );

        if (renameError) {
            throw std::runtime_error(
                "Atomic file replacement failed: "
                + renameError.message()
            );
        }

#if defined(__unix__) || defined(__APPLE__)
        {
            const std::filesystem::path dirPath =
                path.parent_path().empty()
                    ? std::filesystem::path(".")
                    : path.parent_path();

            const int dirFd = ::open(
                dirPath.c_str(),
                O_RDONLY
            );

            if (dirFd >= 0) {
                ::fsync(dirFd);
                ::close(dirFd);
            }
        }
#endif
    } catch (...) {
        std::error_code cleanupError;
        std::filesystem::remove(
            temporaryPath,
            cleanupError
        );

        throw;
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

    input.seekg(0, std::ios::end);
    const std::streampos end =
        input.tellg();

    if (end < 0) {
        throw std::runtime_error("Unable to determine file size: " + path.string());
    }

    std::string contents;
    contents.resize(static_cast<std::size_t>(end));

    input.seekg(0, std::ios::beg);

    if (!contents.empty()) {
        input.read(
            contents.data(),
            static_cast<std::streamsize>(contents.size())
        );

        if (input.gcount() != static_cast<std::streamsize>(contents.size())) {
            throw std::runtime_error("Unable to read complete file: " + path.string());
        }
    }

    return contents;
}

bool AtomicFile::isTemporaryWriteFile(
    const std::filesystem::path& path
) {
    return hasTemporaryWriteMarker(path);
}

std::vector<std::filesystem::path> AtomicFile::listTemporaryWriteFiles(
    const std::filesystem::path& directory
) {
    std::vector<std::filesystem::path> temporaryFiles;

    if (directory.empty() ||
        !std::filesystem::exists(directory)) {
        return temporaryFiles;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        if (isTemporaryWriteFile(entry.path())) {
            temporaryFiles.push_back(entry.path());
        }
    }

    return temporaryFiles;
}

std::size_t AtomicFile::removeTemporaryWriteFiles(
    const std::filesystem::path& directory
) {
    std::size_t removed = 0;

    for (const std::filesystem::path& path : listTemporaryWriteFiles(directory)) {
        std::error_code removeError;
        if (std::filesystem::remove(path, removeError) &&
            !removeError) {
            ++removed;
        }
    }

    return removed;
}

std::filesystem::path AtomicFile::makeTemporaryPath(
    const std::filesystem::path& path
) {
    const std::filesystem::path parent =
        path.parent_path();

    const std::string filename =
        path.filename().string()
        + TEMPORARY_WRITE_MARKER
        + monotonicSuffix();

    if (parent.empty()) {
        return std::filesystem::path(filename);
    }

    return parent / filename;
}

} // namespace nodo::storage
