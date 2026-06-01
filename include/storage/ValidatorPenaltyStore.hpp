#ifndef NODO_STORAGE_VALIDATOR_PENALTY_STORE_HPP
#define NODO_STORAGE_VALIDATOR_PENALTY_STORE_HPP

#include "consensus/ValidatorPenaltyApplication.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace nodo::storage {

class ValidatorPenaltyStore {
public:
    explicit ValidatorPenaltyStore(std::filesystem::path penaltyDirectory);

    const std::filesystem::path& penaltyDirectory() const;

    void save(
        const consensus::ValidatorPenaltyDecision& decision
    ) const;

    bool containsPenalty(
        const std::string& penaltyId
    ) const;

    consensus::ValidatorPenaltyDecision load(
        const std::string& penaltyId
    ) const;

    std::vector<consensus::ValidatorPenaltyDecision> loadAll() const;

    std::size_t count() const;

private:
    std::filesystem::path m_penaltyDirectory;

    std::filesystem::path pathForPenaltyId(
        const std::string& penaltyId
    ) const;

    static bool isSafePenaltyId(
        const std::string& penaltyId
    );
};

} // namespace nodo::storage

#endif
