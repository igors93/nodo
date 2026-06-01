#ifndef NODO_STORAGE_SLASHING_EVIDENCE_STORE_HPP
#define NODO_STORAGE_SLASHING_EVIDENCE_STORE_HPP

#include "consensus/SlashingEvidence.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace nodo::storage {

class SlashingEvidenceStore {
public:
    explicit SlashingEvidenceStore(std::filesystem::path evidenceDirectory);

    const std::filesystem::path& evidenceDirectory() const;

    void save(const consensus::SlashingEvidenceRecord& record) const;

    bool contains(const std::string& evidenceId) const;

    consensus::SlashingEvidenceRecord load(
        const std::string& evidenceId
    ) const;

    std::vector<consensus::SlashingEvidenceRecord> loadAll() const;

    std::size_t count() const;

private:
    std::filesystem::path m_evidenceDirectory;

    std::filesystem::path pathForEvidenceId(
        const std::string& evidenceId
    ) const;

    static bool isSafeEvidenceId(
        const std::string& evidenceId
    );
};

} // namespace nodo::storage

#endif
