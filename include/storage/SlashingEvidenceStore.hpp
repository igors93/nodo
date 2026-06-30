#ifndef NODO_STORAGE_SLASHING_EVIDENCE_STORE_HPP
#define NODO_STORAGE_SLASHING_EVIDENCE_STORE_HPP

#include "consensus/EvidencePoolPersistence.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace nodo::storage {

class SlashingEvidenceStore final
    : public consensus::EvidencePoolPersistence {
public:
    explicit SlashingEvidenceStore(std::filesystem::path evidenceDirectory);

    const std::filesystem::path& evidenceDirectory() const;

    void persist(
        const consensus::DoubleVoteEvidence& evidence
    ) override;

    void persist(
        const consensus::ProposerEquivocationEvidence& evidence
    ) override;

    bool contains(const std::string& evidenceId) const;

    consensus::DoubleVoteEvidence load(
        const std::string& evidenceId
    ) const;

    consensus::ProposerEquivocationEvidence loadProposerEquivocation(
        const std::string& evidenceId
    ) const;

    std::vector<consensus::DoubleVoteEvidence> loadAll() const;

    std::vector<consensus::ProposerEquivocationEvidence> loadAllProposerEquivocation() const;

    bool erase(const std::string& evidenceId) override;

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
