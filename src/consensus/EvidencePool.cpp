#include "consensus/EvidencePool.hpp"

#include <sstream>

namespace nodo::consensus {

EvidencePool::EvidencePool()
    : m_evidenceById() {}

SlashingEvidenceValidationResult EvidencePool::submitRecord(
    const SlashingEvidenceRecord& record
) {
    if (!record.isValid()) {
        return SlashingEvidenceValidationResult(
            SlashingEvidenceValidationStatus::REJECTED,
            "Slashing evidence record is invalid.",
            record
        );
    }

    if (contains(record.evidenceId())) {
        return SlashingEvidenceValidationResult(
            SlashingEvidenceValidationStatus::DUPLICATE,
            "Slashing evidence record is already present.",
            record
        );
    }

    m_evidenceById.emplace(record.evidenceId(), record);

    return SlashingEvidenceValidationResult(
        SlashingEvidenceValidationStatus::ACCEPTED,
        "Slashing evidence record accepted.",
        record
    );
}

SlashingEvidenceValidationResult EvidencePool::submitDoubleVoteEvidence(
    const DoubleVoteEvidence& evidence
) {
    SlashingEvidenceValidationResult validation =
        SlashingEvidenceVerifier::validateDoubleVoteStructure(evidence);

    if (!validation.accepted()) {
        return validation;
    }

    return submitRecord(validation.record());
}

bool EvidencePool::contains(const std::string& evidenceId) const {
    return m_evidenceById.find(evidenceId) != m_evidenceById.end();
}

const SlashingEvidenceRecord* EvidencePool::evidenceById(
    const std::string& evidenceId
) const {
    const auto found = m_evidenceById.find(evidenceId);
    return found == m_evidenceById.end() ? nullptr : &found->second;
}

std::vector<SlashingEvidenceRecord> EvidencePool::allEvidence() const {
    std::vector<SlashingEvidenceRecord> records;
    records.reserve(m_evidenceById.size());

    for (const auto& entry : m_evidenceById) {
        records.push_back(entry.second);
    }

    return records;
}

std::vector<SlashingEvidenceRecord> EvidencePool::evidenceForValidator(
    const std::string& validatorAddress
) const {
    std::vector<SlashingEvidenceRecord> records;

    for (const auto& entry : m_evidenceById) {
        if (entry.second.validatorAddress() == validatorAddress) {
            records.push_back(entry.second);
        }
    }

    return records;
}

std::size_t EvidencePool::size() const {
    return m_evidenceById.size();
}

std::size_t EvidencePool::countForValidator(
    const std::string& validatorAddress
) const {
    return evidenceForValidator(validatorAddress).size();
}

void EvidencePool::pruneOlderThan(std::int64_t cutoffTimestamp) {
    for (auto it = m_evidenceById.begin(); it != m_evidenceById.end(); ) {
        if (it->second.createdAt() < cutoffTimestamp) {
            it = m_evidenceById.erase(it);
        } else {
            ++it;
        }
    }
}

bool EvidencePool::isValid() const {
    for (const auto& entry : m_evidenceById) {
        if (entry.first != entry.second.evidenceId() ||
            !entry.second.isValid()) {
            return false;
        }
    }

    return true;
}

std::string EvidencePool::serialize() const {
    std::ostringstream output;
    output << "EvidencePool{size=" << m_evidenceById.size() << ";evidence=[";

    bool first = true;
    for (const auto& entry : m_evidenceById) {
        if (!first) {
            output << ",";
        }

        output << entry.second.serialize();
        first = false;
    }

    output << "]}";
    return output.str();
}

} // namespace nodo::consensus
