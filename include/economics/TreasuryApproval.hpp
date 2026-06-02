#ifndef NODO_ECONOMICS_TREASURY_APPROVAL_HPP
#define NODO_ECONOMICS_TREASURY_APPROVAL_HPP

#include <cstdint>
#include <string>

namespace nodo::economics {

class TreasuryApproval {
public:
    TreasuryApproval();

    TreasuryApproval(
        std::string approvalId,
        std::string proposalId,
        std::uint64_t approvedAtBlock,
        std::string approver,
        std::string approvalProof
    );

    const std::string& approvalId() const;
    const std::string& proposalId() const;
    std::uint64_t approvedAtBlock() const;
    const std::string& approver() const;
    const std::string& approvalProof() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_approvalId;
    std::string m_proposalId;
    std::uint64_t m_approvedAtBlock;
    std::string m_approver;
    std::string m_approvalProof;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
