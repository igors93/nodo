#include "economics/TreasuryApprovalProof.hpp"

namespace nodo::economics {

std::string TreasuryApprovalProof::build(
    const std::string &governanceProposalId,
    const std::string &treasuryProposalId, const std::string &decisionId,
    const std::string &policyVersion, std::uint64_t decidedAtBlock) {
  return "treasury-approval:" + governanceProposalId + ":" +
         treasuryProposalId + ":" + decisionId + ":" + policyVersion + ":" +
         std::to_string(decidedAtBlock);
}

} // namespace nodo::economics
