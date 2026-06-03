#include "node/ChainAuditResult.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

std::string chainAuditStatusToString(
    ChainAuditStatus status
) {
    switch (status) {
        case ChainAuditStatus::PASSED:
            return "PASSED";
        case ChainAuditStatus::FAILED:
            return "FAILED";
        default:
            return "FAILED";
    }
}

ChainAuditResult::ChainAuditResult()
    : m_status(ChainAuditStatus::FAILED),
      m_reason("Uninitialized chain audit result."),
      m_networkName(""),
      m_cryptoProfile(""),
      m_latestHeight(0),
      m_latestHash(""),
      m_latestStateRoot(""),
      m_loadedBlockCount(0),
      m_loadedMempoolTransactionCount(0),
      m_validatorCount(0) {}

ChainAuditResult ChainAuditResult::passed(
    std::string networkName,
    std::string cryptoProfile,
    std::uint64_t latestHeight,
    std::string latestHash,
    std::string latestStateRoot,
    std::size_t loadedBlockCount,
    std::size_t loadedMempoolTransactionCount,
    std::size_t validatorCount
) {
    ChainAuditResult result;
    result.m_status = ChainAuditStatus::PASSED;
    result.m_reason = "";
    result.m_networkName = std::move(networkName);
    result.m_cryptoProfile = std::move(cryptoProfile);
    result.m_latestHeight = latestHeight;
    result.m_latestHash = std::move(latestHash);
    result.m_latestStateRoot = std::move(latestStateRoot);
    result.m_loadedBlockCount = loadedBlockCount;
    result.m_loadedMempoolTransactionCount = loadedMempoolTransactionCount;
    result.m_validatorCount = validatorCount;
    return result;
}

ChainAuditResult ChainAuditResult::failed(
    std::string reason
) {
    ChainAuditResult result;
    result.m_status = ChainAuditStatus::FAILED;
    result.m_reason = std::move(reason);
    return result;
}

ChainAuditStatus ChainAuditResult::status() const {
    return m_status;
}

const std::string& ChainAuditResult::reason() const {
    return m_reason;
}

bool ChainAuditResult::passed() const {
    return m_status == ChainAuditStatus::PASSED &&
           !m_networkName.empty() &&
           !m_cryptoProfile.empty() &&
           !m_latestHash.empty() &&
           !m_latestStateRoot.empty();
}

const std::string& ChainAuditResult::networkName() const {
    return m_networkName;
}

const std::string& ChainAuditResult::cryptoProfile() const {
    return m_cryptoProfile;
}

std::uint64_t ChainAuditResult::latestHeight() const {
    return m_latestHeight;
}

const std::string& ChainAuditResult::latestHash() const {
    return m_latestHash;
}

const std::string& ChainAuditResult::latestStateRoot() const {
    return m_latestStateRoot;
}

std::size_t ChainAuditResult::loadedBlockCount() const {
    return m_loadedBlockCount;
}

std::size_t ChainAuditResult::loadedMempoolTransactionCount() const {
    return m_loadedMempoolTransactionCount;
}

std::size_t ChainAuditResult::validatorCount() const {
    return m_validatorCount;
}

const std::optional<AuditAssignment>& ChainAuditResult::latestAuditAssignment() const {
    return m_latestAuditAssignment;
}

void ChainAuditResult::setAuditAssignment(AuditAssignment assignment) {
    m_latestAuditAssignment = std::move(assignment);
}

std::string ChainAuditResult::serialize() const {
    std::ostringstream oss;

    oss << "ChainAuditResult{"
        << "status=" << chainAuditStatusToString(m_status)
        << ";reason=" << m_reason
        << ";networkName=" << m_networkName
        << ";cryptoProfile=" << m_cryptoProfile
        << ";latestHeight=" << m_latestHeight
        << ";latestHash=" << m_latestHash
        << ";latestStateRoot=" << m_latestStateRoot
        << ";loadedBlockCount=" << m_loadedBlockCount
        << ";loadedMempoolTransactionCount=" << m_loadedMempoolTransactionCount
        << ";validatorCount=" << m_validatorCount
        << "}";

    return oss.str();
}

std::string ChainAuditResult::toHumanReadableString() const {
    std::ostringstream output;

    if (!passed()) {
        output << "Nodo chain audit failed.\n"
               << "Reason: " << m_reason << "\n";
        return output.str();
    }

    output << "Nodo chain audit passed.\n"
           << "Network: " << m_networkName << "\n"
           << "Crypto profile: " << m_cryptoProfile << "\n"
           << "Latest height: " << m_latestHeight << "\n"
           << "Latest hash: " << m_latestHash << "\n"
           << "Latest state root: " << m_latestStateRoot << "\n"
           << "Loaded finalized blocks: " << m_loadedBlockCount << "\n"
           << "Loaded mempool transactions: " << m_loadedMempoolTransactionCount << "\n"
           << "Validators: " << m_validatorCount << "\n";

    return output.str();
}

} // namespace nodo::node
