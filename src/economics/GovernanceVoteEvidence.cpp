#include "economics/GovernanceVoteEvidence.hpp"

#include "economics/GovernanceVoteProof.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

GovernanceVoteEvidence::GovernanceVoteEvidence()
    : m_voteRecord(),
      m_valid(false),
      m_rejectionReason("GovernanceVoteEvidence: default-constructed.") {}

GovernanceVoteEvidence::GovernanceVoteEvidence(
    GovernanceVoteRecord voteRecord,
    std::string voteProof
)
    : m_voteRecord(std::move(voteRecord)),
      m_voteProof(std::move(voteProof)),
      m_valid(false),
      m_rejectionReason("")
{
    if (!m_voteRecord.isValid()) {
        m_rejectionReason =
            "GovernanceVoteEvidence: voteRecord is invalid: " +
            m_voteRecord.rejectionReason();
        return;
    }

    if (m_voteProof.empty()) {
        m_rejectionReason = "GovernanceVoteEvidence: voteProof must not be empty.";
        return;
    }

    const std::string expectedProof =
        GovernanceVoteProof::build(m_voteRecord);

    if (m_voteProof != expectedProof) {
        m_rejectionReason =
            "GovernanceVoteEvidence: voteProof does not match vote record.";
        return;
    }

    m_valid = true;
}

const GovernanceVoteRecord& GovernanceVoteEvidence::voteRecord() const {
    return m_voteRecord;
}

const std::string& GovernanceVoteEvidence::voteProof() const {
    return m_voteProof;
}

bool GovernanceVoteEvidence::isValid() const { return m_valid; }
const std::string& GovernanceVoteEvidence::rejectionReason() const {
    return m_rejectionReason;
}

std::string GovernanceVoteEvidence::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceVoteEvidence{"
        << "voteRecord=" << m_voteRecord.serialize()
        << ";voteProof=" << m_voteProof
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
