#include "storage/ValidatorPenaltyStore.hpp"

#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace nodo::storage {

ValidatorPenaltyStore::ValidatorPenaltyStore(std::filesystem::path penaltyDirectory)
    : m_penaltyDirectory(std::move(penaltyDirectory)) {}

const std::filesystem::path& ValidatorPenaltyStore::penaltyDirectory() const {
    return m_penaltyDirectory;
}

void ValidatorPenaltyStore::save(
    const consensus::ValidatorPenaltyDecision& decision
) const {
    if (!decision.isValid()) {
        throw std::invalid_argument("Cannot store invalid validator penalty decision.");
    }

    std::filesystem::create_directories(m_penaltyDirectory);
    AtomicFile::writeTextFile(
        pathForPenaltyId(decision.penaltyId()),
        decision.serialize()
    );
}

bool ValidatorPenaltyStore::containsPenalty(
    const std::string& penaltyId
) const {
    if (!isSafePenaltyId(penaltyId)) {
        return false;
    }
    return std::filesystem::exists(pathForPenaltyId(penaltyId));
}

consensus::ValidatorPenaltyDecision ValidatorPenaltyStore::load(
    const std::string& penaltyId
) const {
    if (!isSafePenaltyId(penaltyId)) {
        throw std::invalid_argument("Validator penalty id is unsafe.");
    }

    consensus::ValidatorPenaltyDecision decision =
        consensus::ValidatorPenaltyDecision::deserialize(
            AtomicFile::readTextFile(pathForPenaltyId(penaltyId))
        );

    if (decision.penaltyId() != penaltyId) {
        throw std::runtime_error(
            "Validator penalty file name does not match stored decision id."
        );
    }

    return decision;
}

std::vector<consensus::ValidatorPenaltyDecision> ValidatorPenaltyStore::loadAll() const {
    std::vector<consensus::ValidatorPenaltyDecision> decisions;
    if (!std::filesystem::exists(m_penaltyDirectory)) {
        return decisions;
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(m_penaltyDirectory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        if (entry.path().extension() == ".penalty") {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        decisions.push_back(load(file.stem().string()));
    }

    return decisions;
}

std::size_t ValidatorPenaltyStore::count() const {
    return loadAll().size();
}

std::filesystem::path ValidatorPenaltyStore::pathForPenaltyId(
    const std::string& penaltyId
) const {
    if (!isSafePenaltyId(penaltyId)) {
        throw std::invalid_argument("Validator penalty id is unsafe.");
    }
    return m_penaltyDirectory / (penaltyId + ".penalty");
}

bool ValidatorPenaltyStore::isSafePenaltyId(
    const std::string& penaltyId
) {
    if (penaltyId.empty() || penaltyId.size() > 160) {
        return false;
    }

    for (const char current : penaltyId) {
        const bool allowed =
            (current >= 'a' && current <= 'z') ||
            (current >= 'A' && current <= 'Z') ||
            (current >= '0' && current <= '9') ||
            current == '_' || current == '-' || current == '.';
        if (!allowed) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::storage
