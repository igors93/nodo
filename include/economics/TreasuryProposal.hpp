#ifndef NODO_ECONOMICS_TREASURY_PROPOSAL_HPP
#define NODO_ECONOMICS_TREASURY_PROPOSAL_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

class TreasuryProposal {
public:
    TreasuryProposal();

    TreasuryProposal(
        std::string proposalId,
        std::string recipientAddress,
        utils::Amount amount,
        std::string purpose,
        std::uint64_t createdAtBlock,
        std::uint64_t requestedEpoch,
        std::string proposer
    );

    const std::string& proposalId() const;
    const std::string& recipientAddress() const;
    utils::Amount amount() const;
    const std::string& purpose() const;
    std::uint64_t createdAtBlock() const;
    std::uint64_t requestedEpoch() const;
    const std::string& proposer() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_proposalId;
    std::string m_recipientAddress;
    utils::Amount m_amount;
    std::string m_purpose;
    std::uint64_t m_createdAtBlock;
    std::uint64_t m_requestedEpoch;
    std::string m_proposer;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
