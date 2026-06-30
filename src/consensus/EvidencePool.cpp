#include "consensus/EvidencePool.hpp"

#include "core/ProtocolLimits.hpp"

#include <algorithm>
#include <exception>
#include <sstream>
#include <stdexcept>

namespace nodo::consensus {

EvidencePool::EvidencePool()
    : m_evidenceById(),
      m_doubleVoteEvidenceById(),
      m_proposerEquivocationEvidenceById(),
      m_persistence(nullptr) {}

SlashingEvidenceValidationResult EvidencePool::submitDoubleVoteEvidence(
    const DoubleVoteEvidence& evidence
) {
    SlashingEvidenceValidationResult validation =
        SlashingEvidenceVerifier::validateDoubleVoteStructure(evidence);

    if (!validation.accepted()) {
        return validation;
    }

    const SlashingEvidenceRecord& record = validation.record();
    if (contains(record.evidenceId())) {
        return SlashingEvidenceValidationResult(
            SlashingEvidenceValidationStatus::DUPLICATE,
            "Slashing evidence record is already present.",
            record
        );
    }

    if (m_evidenceById.size() >=
        core::ProtocolLimits::MAX_PENDING_SLASHING_EVIDENCE) {
        return SlashingEvidenceValidationResult(
            SlashingEvidenceValidationStatus::REJECTED,
            "Pending slashing evidence pool reached its protocol limit.",
            record
        );
    }

    if (m_persistence != nullptr) {
        try {
            m_persistence->persist(evidence);
        } catch (const std::exception&) {
            return SlashingEvidenceValidationResult(
                SlashingEvidenceValidationStatus::REJECTED,
                "Slashing evidence could not be persisted atomically.",
                record
            );
        }
    }

    m_evidenceById.emplace(record.evidenceId(), record);
    m_doubleVoteEvidenceById.emplace(record.evidenceId(), evidence);
    return SlashingEvidenceValidationResult(
        SlashingEvidenceValidationStatus::ACCEPTED,
        "Slashing evidence record accepted.",
        record
    );
}


SlashingEvidenceValidationResult EvidencePool::submitProposerEquivocationEvidence(
    const ProposerEquivocationEvidence& evidence
) {
    SlashingEvidenceValidationResult validation =
        SlashingEvidenceVerifier::validateProposerEquivocationStructure(evidence);

    if (!validation.accepted()) {
        return validation;
    }

    const SlashingEvidenceRecord& record = validation.record();
    if (contains(record.evidenceId())) {
        return SlashingEvidenceValidationResult(
            SlashingEvidenceValidationStatus::DUPLICATE,
            "Slashing evidence record is already present.",
            record
        );
    }

    if (m_evidenceById.size() >=
        core::ProtocolLimits::MAX_PENDING_SLASHING_EVIDENCE) {
        return SlashingEvidenceValidationResult(
            SlashingEvidenceValidationStatus::REJECTED,
            "Pending slashing evidence pool reached its protocol limit.",
            record
        );
    }

    if (m_persistence != nullptr) {
        try {
            m_persistence->persist(evidence);
        } catch (const std::exception&) {
            return SlashingEvidenceValidationResult(
                SlashingEvidenceValidationStatus::REJECTED,
                "Slashing evidence could not be persisted atomically.",
                record
            );
        }
    }

    m_evidenceById.emplace(record.evidenceId(), record);
    m_proposerEquivocationEvidenceById.emplace(record.evidenceId(), evidence);
    return SlashingEvidenceValidationResult(
        SlashingEvidenceValidationStatus::ACCEPTED,
        "Slashing evidence record accepted.",
        record
    );
}

void EvidencePool::setPersistence(EvidencePoolPersistence* persistence) {
    if (!m_evidenceById.empty() && persistence != m_persistence) {
        throw std::logic_error(
            "Evidence persistence cannot change after pool admission."
        );
    }
    if (m_persistence != nullptr &&
        persistence != nullptr &&
        m_persistence != persistence) {
        throw std::logic_error(
            "Evidence persistence cannot be replaced while bound."
        );
    }
    m_persistence = persistence;
}

bool EvidencePool::hasPersistence() const {
    return m_persistence != nullptr;
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

const ProposerEquivocationEvidence* EvidencePool::proposerEquivocationEvidenceById(
    const std::string& evidenceId
) const {
    const auto found = m_proposerEquivocationEvidenceById.find(evidenceId);
    return found == m_proposerEquivocationEvidenceById.end() ? nullptr : &found->second;
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

std::vector<ProposerEquivocationEvidence> EvidencePool::allProposerEquivocationEvidence() const {
    std::vector<ProposerEquivocationEvidence> evidence;
    evidence.reserve(m_proposerEquivocationEvidenceById.size());
    for (const auto& [id, value] : m_proposerEquivocationEvidenceById) {
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
    m_proposerEquivocationEvidenceById.erase(evidenceId);
    if (!removed || m_persistence == nullptr) {
        return removed;
    }
    return m_persistence->erase(evidenceId);
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
            if (m_persistence != nullptr &&
                !m_persistence->erase(it->first)) {
                ++it;
                continue;
            }
            m_doubleVoteEvidenceById.erase(it->first);
            m_proposerEquivocationEvidenceById.erase(it->first);
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

    for (const auto& [evidenceId, evidence] : m_proposerEquivocationEvidenceById) {
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
