#include "storage/SlashingEvidenceStore.hpp"

#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <stdexcept>

namespace nodo::storage {

namespace {

constexpr const char* EVIDENCE_FILE_EXTENSION = ".evidence";

} // namespace

SlashingEvidenceStore::SlashingEvidenceStore(
    std::filesystem::path evidenceDirectory
) : m_evidenceDirectory(std::move(evidenceDirectory)) {}

const std::filesystem::path& SlashingEvidenceStore::evidenceDirectory() const {
    return m_evidenceDirectory;
}

void SlashingEvidenceStore::save(
    const consensus::SlashingEvidenceRecord& record
) const {
    if (!record.isValid()) {
        throw std::invalid_argument("Cannot persist invalid slashing evidence record.");
    }

    std::filesystem::create_directories(m_evidenceDirectory);

    AtomicFile::writeTextFile(
        pathForEvidenceId(record.evidenceId()),
        record.serialize()
    );
}

bool SlashingEvidenceStore::contains(
    const std::string& evidenceId
) const {
    if (!isSafeEvidenceId(evidenceId)) {
        return false;
    }

    return std::filesystem::exists(pathForEvidenceId(evidenceId));
}

consensus::SlashingEvidenceRecord SlashingEvidenceStore::load(
    const std::string& evidenceId
) const {
    if (!contains(evidenceId)) {
        throw std::invalid_argument("Slashing evidence record was not found.");
    }

    consensus::SlashingEvidenceRecord record =
        consensus::SlashingEvidenceRecord::deserialize(
            AtomicFile::readTextFile(pathForEvidenceId(evidenceId))
        );

    if (record.evidenceId() != evidenceId) {
        throw std::runtime_error(
            "Slashing evidence file name does not match stored evidence id."
        );
    }

    return record;
}

std::vector<consensus::SlashingEvidenceRecord> SlashingEvidenceStore::loadAll() const {
    std::vector<consensus::SlashingEvidenceRecord> records;

    if (!std::filesystem::exists(m_evidenceDirectory)) {
        return records;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_evidenceDirectory)) {
        if (!entry.is_regular_file() ||
            entry.path().extension() != EVIDENCE_FILE_EXTENSION) {
            continue;
        }

        records.push_back(load(entry.path().stem().string()));
    }

    std::sort(
        records.begin(),
        records.end(),
        [](const auto& left, const auto& right) {
            return left.evidenceId() < right.evidenceId();
        }
    );

    return records;
}

std::size_t SlashingEvidenceStore::count() const {
    return loadAll().size();
}

std::filesystem::path SlashingEvidenceStore::pathForEvidenceId(
    const std::string& evidenceId
) const {
    if (!isSafeEvidenceId(evidenceId)) {
        throw std::invalid_argument("Unsafe slashing evidence id rejected.");
    }

    return m_evidenceDirectory / (evidenceId + EVIDENCE_FILE_EXTENSION);
}

bool SlashingEvidenceStore::isSafeEvidenceId(
    const std::string& evidenceId
) {
    if (evidenceId.empty() || evidenceId.size() > 160) {
        return false;
    }

    for (const char current : evidenceId) {
        const bool allowed =
            (current >= 'a' && current <= 'z') ||
            (current >= 'A' && current <= 'Z') ||
            (current >= '0' && current <= '9') ||
            current == '_' ||
            current == '-';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::storage
