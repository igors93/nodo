#include "storage/ProtocolEvidenceStore.hpp"

#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <stdexcept>

namespace nodo::storage {

namespace {

constexpr const char* kProtocolEvidenceExtension = ".pevidence";

bool isProtocolEvidenceFile(const std::filesystem::directory_entry& entry) {
    return entry.is_regular_file() &&
           entry.path().extension() == kProtocolEvidenceExtension;
}

} // namespace

ProtocolEvidenceStore::ProtocolEvidenceStore(
    std::filesystem::path evidenceDirectory
) : m_evidenceDirectory(std::move(evidenceDirectory)) {}

const std::filesystem::path& ProtocolEvidenceStore::evidenceDirectory() const {
    return m_evidenceDirectory;
}

void ProtocolEvidenceStore::save(
    const economics::ProtocolEvidence& evidence
) const {
    if (!evidence.isValid()) {
        throw std::invalid_argument(
            "ProtocolEvidenceStore::save: cannot persist invalid evidence: " +
            evidence.rejectionReason()
        );
    }

    if (!isSafeEvidenceId(evidence.evidenceId())) {
        throw std::invalid_argument(
            "ProtocolEvidenceStore::save: unsafe evidenceId: " +
            evidence.evidenceId()
        );
    }

    const std::filesystem::path path = pathForEvidenceId(evidence.evidenceId());

    if (std::filesystem::exists(path)) {
        throw std::runtime_error(
            "ProtocolEvidenceStore::save: duplicate evidenceId: " +
            evidence.evidenceId()
        );
    }

    std::filesystem::create_directories(m_evidenceDirectory);
    AtomicFile::writeTextFile(path, evidence.serialize());
}

bool ProtocolEvidenceStore::contains(const std::string& evidenceId) const {
    if (!isSafeEvidenceId(evidenceId)) {
        return false;
    }
    return std::filesystem::exists(pathForEvidenceId(evidenceId));
}

economics::ProtocolEvidence ProtocolEvidenceStore::load(
    const std::string& evidenceId
) const {
    if (!isSafeEvidenceId(evidenceId)) {
        throw std::invalid_argument(
            "ProtocolEvidenceStore::load: unsafe evidenceId: " + evidenceId
        );
    }

    const std::filesystem::path path = pathForEvidenceId(evidenceId);

    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(
            "ProtocolEvidenceStore::load: evidenceId not found: " + evidenceId
        );
    }

    const std::string content = AtomicFile::readTextFile(path);
    const economics::ProtocolEvidence evidence =
        economics::ProtocolEvidence::deserialize(content);

    if (!evidence.isValid()) {
        throw std::runtime_error(
            "ProtocolEvidenceStore::load: decoded evidence is invalid: " +
            evidence.rejectionReason()
        );
    }

    return evidence;
}

std::vector<economics::ProtocolEvidence> ProtocolEvidenceStore::loadAll() const {
    std::vector<economics::ProtocolEvidence> records;

    if (!std::filesystem::exists(m_evidenceDirectory)) {
        return records;
    }

    for (const auto& entry :
         std::filesystem::directory_iterator(m_evidenceDirectory)) {
        if (isProtocolEvidenceFile(entry)) {
            records.push_back(load(entry.path().stem().string()));
        }
    }

    return records;
}

std::vector<economics::ProtocolEvidence> ProtocolEvidenceStore::loadBySubject(
    const std::string& subjectId
) const {
    std::vector<economics::ProtocolEvidence> all = loadAll();
    std::vector<economics::ProtocolEvidence> result;
    for (auto& ev : all) {
        if (ev.subjectId() == subjectId) {
            result.push_back(std::move(ev));
        }
    }
    return result;
}

std::size_t ProtocolEvidenceStore::count() const {
    if (!std::filesystem::exists(m_evidenceDirectory)) {
        return 0;
    }
    std::size_t n = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator(m_evidenceDirectory)) {
        if (isProtocolEvidenceFile(entry)) {
            ++n;
        }
    }
    return n;
}

std::filesystem::path ProtocolEvidenceStore::pathForEvidenceId(
    const std::string& evidenceId
) const {
    return m_evidenceDirectory / (evidenceId + kProtocolEvidenceExtension);
}

bool ProtocolEvidenceStore::isSafeEvidenceId(const std::string& evidenceId) {
    if (evidenceId.empty()) {
        return false;
    }
    for (const char c : evidenceId) {
        const bool safe =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_';
        if (!safe) {
            return false;
        }
    }
    return true;
}

} // namespace nodo::storage
