#ifndef NODO_NODE_SLASHING_EVIDENCE_MESSAGES_HPP
#define NODO_NODE_SLASHING_EVIDENCE_MESSAGES_HPP

#include "consensus/SlashingEvidence.hpp"

#include <cstdint>
#include <string>

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

    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& announcerNodeId() const;
    const consensus::DoubleVoteEvidence& evidence() const;
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
    consensus::DoubleVoteEvidence m_evidence;
    std::int64_t m_announcedAt;
};

class SlashingEvidenceRequest {
public:
    SlashingEvidenceRequest();

    SlashingEvidenceRequest(
        std::string requesterNodeId,
        std::string evidenceId,
        std::int64_t requestedAt
    );

    const std::string& requesterNodeId() const;
    const std::string& evidenceId() const;
    std::int64_t requestedAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_requesterNodeId;
    std::string m_evidenceId;
    std::int64_t m_requestedAt;
};

} // namespace nodo::node

#endif
