#include "consensus/EvidencePool.hpp"

#include <algorithm>
#include <sstream>

namespace nodo::consensus {

EvidencePool::EvidencePool()
    : m_evidenceById(),
      m_doubleVoteEvidenceById() {}

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

    const SlashingEvidenceValidationResult stored =
        submitRecord(validation.record());
    if (stored.accepted()) {
        m_doubleVoteEvidenceById.emplace(
            validation.record().evidenceId(), evidence
        );
    }
    return stored;
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

const DoubleVoteEvidence* EvidencePool::doubleVoteEvidenceById(
    const std::string& evidenceId
) const {
    const auto found = m_doubleVoteEvidenceById.find(evidenceId);
    return found == m_doubleVoteEvidenceById.end() ? nullptr : &found->second;
}

std::vector<SlashingEvidenceRecord> EvidencePool::allEvidence() const {
    std::vector<SlashingEvidenceRecord> records;
    records.reserve(m_evidenceById.size());

    for (const auto& entry : m_evidenceById) {
        records.push_back(entry.second);
    }

    return records;
}

std::vector<DoubleVoteEvidence> EvidencePool::allDoubleVoteEvidence() const {
    std::vector<DoubleVoteEvidence> evidence;
    evidence.reserve(m_doubleVoteEvidenceById.size());
    for (const auto& [id, value] : m_doubleVoteEvidenceById) {
        (void)id;
        evidence.push_back(value);
    }
    return evidence;
}

std::vector<DoubleVoteEvidence> EvidencePool::doubleVoteEvidenceBeforeHeight(
    std::uint64_t blockHeight
) const {
    std::vector<DoubleVoteEvidence> evidence;
    for (const auto& [id, value] : m_doubleVoteEvidenceById) {
        (void)id;
        if (value.firstVote().blockIndex() < blockHeight) {
            evidence.push_back(value);
        }
    }
    return evidence;
}

bool EvidencePool::removeEvidence(const std::string& evidenceId) {
    const bool removed = m_evidenceById.erase(evidenceId) > 0;
    m_doubleVoteEvidenceById.erase(evidenceId);
    return removed;
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

void EvidencePool::pruneOlderThan(std::int64_t cutoffTimestamp, std::int64_t now) {
    // Never remove evidence newer than (now - kMinRetentionSeconds) regardless
    // of the requested cutoff, to prevent slashing evidence from being pruned
    // before the unbonding/finality window has elapsed.
    const std::int64_t safeCutoff =
        std::min(cutoffTimestamp, now - kMinRetentionSeconds);

    for (auto it = m_evidenceById.begin(); it != m_evidenceById.end(); ) {
        if (it->second.createdAt() < safeCutoff) {
            m_doubleVoteEvidenceById.erase(it->first);
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

    for (const auto& [evidenceId, evidence] : m_doubleVoteEvidenceById) {
        const auto record = m_evidenceById.find(evidenceId);
        if (record == m_evidenceById.end() ||
            evidence.evidenceId() != evidenceId ||
            evidence.toRecord().serialize() != record->second.serialize()) {
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
