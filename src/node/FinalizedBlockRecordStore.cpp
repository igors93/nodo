#include "node/FinalizedBlockRecordStore.hpp"

#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace nodo::node {

FinalizedBlockRecordStore::FinalizedBlockRecordStore(
    std::filesystem::path dataDirectory
) : m_dataDirectory(std::move(dataDirectory)) {}

const std::filesystem::path& FinalizedBlockRecordStore::dataDirectory() const {
    return m_dataDirectory;
}

std::filesystem::path FinalizedBlockRecordStore::recordFilePath(
    std::uint64_t height
) const {
    return m_dataDirectory / "sync" / "qc" / (std::to_string(height) + ".qc");
}

bool FinalizedBlockRecordStore::save(
    const consensus::FinalizedBlockRecord& record
) const {
    if (!record.isStructurallyValid()) {
        return false;
    }

    const std::string serialized = record.serialize();
    const std::filesystem::path path = recordFilePath(record.blockIndex());

    // Guard against divergent finality decisions for the same height.
    if (std::filesystem::exists(path)) {
        try {
            const std::string existing = storage::AtomicFile::readTextFile(path);
            if (existing == serialized) {
                return true; // already stored — idempotent
            }
            return false; // divergent record — protocol violation
        } catch (...) {
            // Unreadable existing file — overwrite below.
        }
    }

    try {
        std::filesystem::create_directories(path.parent_path());
        storage::AtomicFile::writeTextFile(path, serialized);
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<consensus::FinalizedBlockRecord> FinalizedBlockRecordStore::load(
    std::uint64_t height
) const {
    const std::filesystem::path path = recordFilePath(height);
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    try {
        const std::string contents = storage::AtomicFile::readTextFile(path);
        const consensus::FinalizedBlockRecord record =
            consensus::FinalizedBlockRecord::deserialize(contents);
        if (!record.isStructurallyValid()) {
            return std::nullopt;
        }
        return record;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<consensus::FinalizedBlockRecord> FinalizedBlockRecordStore::loadAll() const {
    const std::filesystem::path dir = m_dataDirectory / "sync" / "qc";

    std::vector<consensus::FinalizedBlockRecord> records;

    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || ec) {
        return records;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        const std::string filename = entry.path().filename().string();

        // Expect "<height>.qc"
        if (filename.size() < 4 ||
            filename.substr(filename.size() - 3) != ".qc") {
            continue;
        }

        const std::string heightStr = filename.substr(0, filename.size() - 3);
        bool isNumeric = !heightStr.empty();
        for (const char c : heightStr) {
            if (c < '0' || c > '9') {
                isNumeric = false;
                break;
            }
        }
        if (!isNumeric) {
            continue;
        }

        std::uint64_t height = 0;
        try {
            height = std::stoull(heightStr);
        } catch (...) {
            continue;
        }

        const auto opt = load(height);
        if (opt.has_value()) {
            records.push_back(opt.value());
        }
    }

    std::sort(
        records.begin(),
        records.end(),
        [](const consensus::FinalizedBlockRecord& a,
           const consensus::FinalizedBlockRecord& b) {
            return a.blockIndex() < b.blockIndex();
        }
    );

    return records;
}

} // namespace nodo::node
