#ifndef NODO_NODE_VALIDATOR_PENALTY_MESSAGES_HPP
#define NODO_NODE_VALIDATOR_PENALTY_MESSAGES_HPP

#include "consensus/ValidatorPenaltyApplication.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

class ValidatorPenaltyAnnouncement {
public:
    ValidatorPenaltyAnnouncement();

    ValidatorPenaltyAnnouncement(
        std::string networkId,
        std::string chainId,
        std::string announcerNodeId,
        consensus::ValidatorPenaltyDecision decision,
        std::int64_t announcedAt
    );

    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& announcerNodeId() const;
    const consensus::ValidatorPenaltyDecision& decision() const;
    std::int64_t announcedAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_networkId;
    std::string m_chainId;
    std::string m_announcerNodeId;
    consensus::ValidatorPenaltyDecision m_decision;
    std::int64_t m_announcedAt;
};

class ValidatorPenaltyRequest {
public:
    ValidatorPenaltyRequest();

    ValidatorPenaltyRequest(
        std::string requesterNodeId,
        std::string penaltyId,
        std::int64_t requestedAt
    );

    const std::string& requesterNodeId() const;
    const std::string& penaltyId() const;
    std::int64_t requestedAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_requesterNodeId;
    std::string m_penaltyId;
    std::int64_t m_requestedAt;
};

} // namespace nodo::node

#endif
