#include "storage/SlashingEvidenceStore.hpp"

#include "core/ProtocolLimits.hpp"
#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <stdexcept>

namespace nodo::storage {

namespace {

constexpr const char* EVIDENCE_FILE_EXTENSION = ".evidence";

bool isEvidenceFile(
    const std::filesystem::directory_entry& entry
) {
    std::error_code statusError;
    const std::filesystem::file_status status =
        entry.symlink_status(statusError);
    return !statusError && std::filesystem::is_regular_file(status) &&
           entry.path().extension() == EVIDENCE_FILE_EXTENSION;
}

std::size_t evidenceFileCount(
    const std::filesystem::path& evidenceDirectory
) {
    if (!std::filesystem::exists(evidenceDirectory)) {
        return 0;
    }

    std::size_t count = 0;

    for (const auto& entry : std::filesystem::directory_iterator(evidenceDirectory)) {
        if (isEvidenceFile(entry)) {
            ++count;
        }
    }

    return count;
}

bool isDoubleVoteEncoding(const std::string& serialized) {
    return serialized.rfind("DoubleVoteEvidence{", 0) == 0;
}

bool isProposerEquivocationEncoding(const std::string& serialized) {
    return serialized.rfind("ProposerEquivocationEvidence{", 0) == 0;
}

} // namespace

SlashingEvidenceStore::SlashingEvidenceStore(
    std::filesystem::path evidenceDirectory
) : m_evidenceDirectory(std::move(evidenceDirectory)) {}

const std::filesystem::path& SlashingEvidenceStore::evidenceDirectory() const {
    return m_evidenceDirectory;
}

void SlashingEvidenceStore::persist(
    const consensus::DoubleVoteEvidence& evidence
) {
    const consensus::SlashingEvidenceValidationResult validation =
        consensus::SlashingEvidenceVerifier::validateDoubleVoteStructure(
            evidence
        );
    if (!validation.accepted()) {
        throw std::invalid_argument(
            "Cannot persist invalid double-vote evidence."
        );
    }

    const std::string serialized = evidence.serialize();
    if (serialized.size() >
        core::ProtocolLimits::MAX_SLASHING_EVIDENCE_GOSSIP_BYTES) {
        throw std::length_error(
            "Double-vote evidence exceeds its persistence limit."
        );
    }

    const std::string evidenceId = evidence.evidenceId();
    if (contains(evidenceId)) {
        const consensus::DoubleVoteEvidence existing = load(evidenceId);
        if (existing.serialize() != serialized) {
            throw std::runtime_error(
                "Stored slashing evidence conflicts with the same evidence id."
            );
        }
        return;
    }

    std::filesystem::create_directories(m_evidenceDirectory);

    AtomicFile::writeTextFile(
        pathForEvidenceId(evidenceId),
        serialized
    );
}



void SlashingEvidenceStore::persist(
    const consensus::ProposerEquivocationEvidence& evidence
) {
    const consensus::SlashingEvidenceValidationResult validation =
        consensus::SlashingEvidenceVerifier::validateProposerEquivocationStructure(
            evidence
        );
    if (!validation.accepted()) {
        throw std::invalid_argument(
            "Cannot persist invalid proposer-equivocation evidence."
        );
    }

    const std::string serialized = evidence.serialize();
    if (serialized.size() >
        core::ProtocolLimits::MAX_SLASHING_EVIDENCE_GOSSIP_BYTES) {
        throw std::length_error(
            "Proposer-equivocation evidence exceeds its persistence limit."
        );
    }

    const std::string evidenceId = evidence.evidenceId();
    if (contains(evidenceId)) {
        const std::string existing = AtomicFile::readTextFile(pathForEvidenceId(evidenceId));
        if (existing != serialized) {
            throw std::runtime_error(
                "Stored slashing evidence conflicts with the same evidence id."
            );
        }
        return;
    }

    std::filesystem::create_directories(m_evidenceDirectory);

    AtomicFile::writeTextFile(
        pathForEvidenceId(evidenceId),
        serialized
    );
}

bool SlashingEvidenceStore::contains(
    const std::string& evidenceId
) const {
    if (!isSafeEvidenceId(evidenceId)) {
        return false;
    }

    std::error_code statusError;
    const std::filesystem::file_status status =
        std::filesystem::symlink_status(
            pathForEvidenceId(evidenceId), statusError
        );
    return !statusError && std::filesystem::is_regular_file(status);
}

consensus::DoubleVoteEvidence SlashingEvidenceStore::load(
    const std::string& evidenceId
) const {
    if (!contains(evidenceId)) {
        throw std::invalid_argument("Slashing evidence record was not found.");
    }

    const std::filesystem::path path = pathForEvidenceId(evidenceId);
    std::error_code sizeError;
    const std::uintmax_t fileSize = std::filesystem::file_size(
        path, sizeError
    );
    if (sizeError || fileSize == 0 || fileSize >
        core::ProtocolLimits::MAX_SLASHING_EVIDENCE_GOSSIP_BYTES) {
        throw std::runtime_error(
            "Stored slashing evidence has an invalid size."
        );
    }

    const std::string serialized = AtomicFile::readTextFile(path);

    consensus::DoubleVoteEvidence evidence =
        consensus::DoubleVoteEvidence::deserialize(serialized);

    if (evidence.evidenceId() != evidenceId) {
        throw std::runtime_error(
            "Slashing evidence file name does not match stored evidence id."
        );
    }

    return evidence;
}

consensus::ProposerEquivocationEvidence SlashingEvidenceStore::loadProposerEquivocation(
    const std::string& evidenceId
) const {
    if (!contains(evidenceId)) {
        throw std::invalid_argument("Slashing evidence record was not found.");
    }

    const std::filesystem::path path = pathForEvidenceId(evidenceId);
    std::error_code sizeError;
    const std::uintmax_t fileSize = std::filesystem::file_size(
        path, sizeError
    );
    if (sizeError || fileSize == 0 || fileSize >
        core::ProtocolLimits::MAX_SLASHING_EVIDENCE_GOSSIP_BYTES) {
        throw std::runtime_error(
            "Stored slashing evidence has an invalid size."
        );
    }

    const std::string serialized = AtomicFile::readTextFile(path);

    consensus::ProposerEquivocationEvidence evidence =
        consensus::ProposerEquivocationEvidence::deserialize(serialized);

    if (evidence.evidenceId() != evidenceId) {
        throw std::runtime_error(
            "Slashing evidence file name does not match stored evidence id."
        );
    }

    return evidence;
}

std::vector<consensus::DoubleVoteEvidence> SlashingEvidenceStore::loadAll() const {
    std::vector<consensus::DoubleVoteEvidence> evidence;

    if (!std::filesystem::exists(m_evidenceDirectory)) {
        return evidence;
    }

    const std::size_t fileCount = evidenceFileCount(m_evidenceDirectory);
    if (fileCount > core::ProtocolLimits::MAX_PENDING_SLASHING_EVIDENCE) {
        throw std::length_error(
            "Stored slashing evidence pool exceeds its protocol limit."
        );
    }
    evidence.reserve(fileCount);

    for (const auto& entry : std::filesystem::directory_iterator(m_evidenceDirectory)) {
        if (!isEvidenceFile(entry)) {
            continue;
        }

        const std::string serialized = AtomicFile::readTextFile(entry.path());
        if (isDoubleVoteEncoding(serialized)) {
            evidence.push_back(load(entry.path().stem().string()));
        } else if (!isProposerEquivocationEncoding(serialized)) {
            throw std::runtime_error("Stored slashing evidence has an unknown encoding.");
        }
    }

    std::sort(
        evidence.begin(),
        evidence.end(),
        [](const auto& left, const auto& right) {
            return left.evidenceId() < right.evidenceId();
        }
    );

    return evidence;
}

std::vector<consensus::ProposerEquivocationEvidence>
SlashingEvidenceStore::loadAllProposerEquivocation() const {
    std::vector<consensus::ProposerEquivocationEvidence> evidence;

    if (!std::filesystem::exists(m_evidenceDirectory)) {
        return evidence;
    }

    const std::size_t fileCount = evidenceFileCount(m_evidenceDirectory);
    if (fileCount > core::ProtocolLimits::MAX_PENDING_SLASHING_EVIDENCE) {
        throw std::length_error(
            "Stored slashing evidence pool exceeds its protocol limit."
        );
    }
    evidence.reserve(fileCount);

    for (const auto& entry : std::filesystem::directory_iterator(m_evidenceDirectory)) {
        if (!isEvidenceFile(entry)) {
            continue;
        }
        const std::string serialized = AtomicFile::readTextFile(entry.path());
        if (isProposerEquivocationEncoding(serialized)) {
            evidence.push_back(loadProposerEquivocation(entry.path().stem().string()));
        } else if (!isDoubleVoteEncoding(serialized)) {
            throw std::runtime_error("Stored slashing evidence has an unknown encoding.");
        }
    }

    std::sort(
        evidence.begin(),
        evidence.end(),
        [](const auto& left, const auto& right) {
            return left.evidenceId() < right.evidenceId();
        }
    );

    return evidence;
}

bool SlashingEvidenceStore::erase(
    const std::string& evidenceId
) {
    if (!isSafeEvidenceId(evidenceId)) {
        return false;
    }

    std::error_code existsError;
    const std::filesystem::path path = pathForEvidenceId(evidenceId);
    const bool exists = std::filesystem::exists(path, existsError);
    if (existsError) {
        return false;
    }
    if (!exists) {
        return true;
    }

    std::error_code removeError;
    const bool removed = std::filesystem::remove(path, removeError);
    return removed && !removeError;
}

std::size_t SlashingEvidenceStore::count() const {
    return evidenceFileCount(m_evidenceDirectory);
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
