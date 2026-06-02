#ifndef NODO_STORAGE_PROTOCOL_EVIDENCE_STORE_HPP
#define NODO_STORAGE_PROTOCOL_EVIDENCE_STORE_HPP

#include "economics/ProtocolEvidence.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace nodo::storage {

/*
 * ProtocolEvidenceStore persists canonical protocol evidence records to disk.
 *
 * Security principle:
 * Each evidence record is written atomically to an individual file. The store
 * rejects duplicate evidenceIds. Invalid evidence cannot be persisted.
 * The store never applies penalties; it only preserves auditable proof.
 *
 * DoS protection:
 * Callers must implement their own rate-limiting before calling save().
 * The store does not limit the total number of records per subject.
 */
class ProtocolEvidenceStore {
public:
    explicit ProtocolEvidenceStore(std::filesystem::path evidenceDirectory);

    const std::filesystem::path& evidenceDirectory() const;

    void save(const economics::ProtocolEvidence& evidence) const;

    bool contains(const std::string& evidenceId) const;

    economics::ProtocolEvidence load(const std::string& evidenceId) const;

    std::vector<economics::ProtocolEvidence> loadAll() const;

    std::vector<economics::ProtocolEvidence> loadBySubject(
        const std::string& subjectId
    ) const;

    std::size_t count() const;

private:
    std::filesystem::path m_evidenceDirectory;

    std::filesystem::path pathForEvidenceId(
        const std::string& evidenceId
    ) const;

    static bool isSafeEvidenceId(const std::string& evidenceId);
};

} // namespace nodo::storage

#endif
