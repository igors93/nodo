#ifndef NODO_NODE_CHAIN_AUDIT_RESULT_HPP
#define NODO_NODE_CHAIN_AUDIT_RESULT_HPP

#include <cstdint>
#include <string>

namespace nodo::node {

enum class ChainAuditStatus {
    PASSED,
    FAILED
};

std::string chainAuditStatusToString(
    ChainAuditStatus status
);

class ChainAuditResult {
public:
    ChainAuditResult();

    static ChainAuditResult passed(
        std::string networkName,
        std::string cryptoProfile,
        std::uint64_t latestHeight,
        std::string latestHash,
        std::string latestStateRoot,
        std::size_t loadedBlockCount,
        std::size_t loadedMempoolTransactionCount,
        std::size_t validatorCount
    );

    static ChainAuditResult failed(
        std::string reason
    );

    ChainAuditStatus status() const;
    const std::string& reason() const;
    bool passed() const;

    const std::string& networkName() const;
    const std::string& cryptoProfile() const;
    std::uint64_t latestHeight() const;
    const std::string& latestHash() const;
    const std::string& latestStateRoot() const;
    std::size_t loadedBlockCount() const;
    std::size_t loadedMempoolTransactionCount() const;
    std::size_t validatorCount() const;

    std::string serialize() const;
    std::string toHumanReadableString() const;

private:
    ChainAuditStatus m_status;
    std::string m_reason;
    std::string m_networkName;
    std::string m_cryptoProfile;
    std::uint64_t m_latestHeight;
    std::string m_latestHash;
    std::string m_latestStateRoot;
    std::size_t m_loadedBlockCount;
    std::size_t m_loadedMempoolTransactionCount;
    std::size_t m_validatorCount;
};

} // namespace nodo::node

#endif
