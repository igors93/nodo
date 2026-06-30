#ifndef NODO_NODE_SLASHING_EVIDENCE_MESSAGES_HPP
#define NODO_NODE_SLASHING_EVIDENCE_MESSAGES_HPP

#include "consensus/SlashingEvidence.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class SlashingEvidenceAnnouncement {
public:
    SlashingEvidenceAnnouncement();

    SlashingEvidenceAnnouncement(
        std::string networkId,
        std::string chainId,
        std::string announcerNodeId,
        consensus::DoubleVoteEvidence evidence,
        std::int64_t announcedAt
    );

    SlashingEvidenceAnnouncement(
        std::string networkId,
        std::string chainId,
        std::string announcerNodeId,
        consensus::ProposerEquivocationEvidence evidence,
        std::int64_t announcedAt
    );

    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& announcerNodeId() const;
    consensus::SlashingEvidenceType evidenceType() const;
    const consensus::DoubleVoteEvidence& evidence() const;
    const consensus::ProposerEquivocationEvidence& proposerEquivocationEvidence() const;
    consensus::SlashingEvidenceRecord record() const;
    std::int64_t announcedAt() const;

    bool isValid() const;
    std::string serialize() const;

    static SlashingEvidenceAnnouncement deserialize(
        const std::string& serialized
    );

private:
    std::string m_networkId;
    std::string m_chainId;
    std::string m_announcerNodeId;
    consensus::SlashingEvidenceType m_evidenceType;
    consensus::DoubleVoteEvidence m_evidence;
    consensus::ProposerEquivocationEvidence m_proposerEquivocationEvidence;
    std::int64_t m_announcedAt;
};

class SlashingEvidenceInventory {
public:
    SlashingEvidenceInventory();

    SlashingEvidenceInventory(
        std::string networkId,
        std::string chainId,
        std::string announcerNodeId,
        std::vector<std::string> evidenceIds,
        std::int64_t generatedAt
    );

    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& announcerNodeId() const;
    const std::vector<std::string>& evidenceIds() const;
    std::int64_t generatedAt() const;

    bool isValid() const;
    std::string serialize() const;

    static SlashingEvidenceInventory deserialize(
        const std::string& serialized
    );

private:
    std::string m_networkId;
    std::string m_chainId;
    std::string m_announcerNodeId;
    std::vector<std::string> m_evidenceIds;
    std::int64_t m_generatedAt;
};

class SlashingEvidenceRequest {
public:
    SlashingEvidenceRequest();

    SlashingEvidenceRequest(
        std::string networkId,
        std::string chainId,
        std::string requesterNodeId,
        std::string evidenceId,
        std::int64_t requestedAt
    );

    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& requesterNodeId() const;
    const std::string& evidenceId() const;
    std::int64_t requestedAt() const;

    bool isValid() const;
    std::string serialize() const;

    static SlashingEvidenceRequest deserialize(
        const std::string& serialized
    );

private:
    std::string m_networkId;
    std::string m_chainId;
    std::string m_requesterNodeId;
    std::string m_evidenceId;
    std::int64_t m_requestedAt;
};

class SlashingEvidenceResponse {
public:
    SlashingEvidenceResponse();

    SlashingEvidenceResponse(
        std::string networkId,
        std::string chainId,
        std::string responderNodeId,
        consensus::DoubleVoteEvidence evidence,
        std::int64_t respondedAt
    );

    SlashingEvidenceResponse(
        std::string networkId,
        std::string chainId,
        std::string responderNodeId,
        consensus::ProposerEquivocationEvidence evidence,
        std::int64_t respondedAt
    );

    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& responderNodeId() const;
    consensus::SlashingEvidenceType evidenceType() const;
    const consensus::DoubleVoteEvidence& evidence() const;
    const consensus::ProposerEquivocationEvidence& proposerEquivocationEvidence() const;
    std::int64_t respondedAt() const;

    bool isValid() const;
    std::string serialize() const;

    static SlashingEvidenceResponse deserialize(
        const std::string& serialized
    );

private:
    std::string m_networkId;
    std::string m_chainId;
    std::string m_responderNodeId;
    consensus::SlashingEvidenceType m_evidenceType;
    consensus::DoubleVoteEvidence m_evidence;
    consensus::ProposerEquivocationEvidence m_proposerEquivocationEvidence;
    std::int64_t m_respondedAt;
};

} // namespace nodo::node

#endif
