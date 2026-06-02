#ifndef NODO_NODE_GOVERNANCE_LIFECYCLE_STORE_HPP
#define NODO_NODE_GOVERNANCE_LIFECYCLE_STORE_HPP

#include "economics/GovernanceLifecycleRecord.hpp"

#include <filesystem>
#include <string>

namespace nodo::node {

enum class GovernanceLifecycleStoreStatus {
    STORED,
    LOADED,
    NOT_FOUND,
    INVALID_RECORD,
    IO_ERROR
};

std::string governanceLifecycleStoreStatusToString(
    GovernanceLifecycleStoreStatus status
);

class GovernanceLifecycleStoreResult {
public:
    GovernanceLifecycleStoreResult();

    static GovernanceLifecycleStoreResult stored(
        economics::GovernanceLifecycleRecord lifecycle
    );

    static GovernanceLifecycleStoreResult loaded(
        economics::GovernanceLifecycleRecord lifecycle
    );

    static GovernanceLifecycleStoreResult rejected(
        GovernanceLifecycleStoreStatus status,
        std::string reason
    );

    bool success() const;
    GovernanceLifecycleStoreStatus status() const;
    const std::string& reason() const;
    const economics::GovernanceLifecycleRecord& lifecycle() const;

private:
    GovernanceLifecycleStoreStatus m_status;
    std::string m_reason;
    economics::GovernanceLifecycleRecord m_lifecycle;
};

class GovernanceLifecycleStore {
public:
    explicit GovernanceLifecycleStore(std::filesystem::path lifecycleDirectory);

    GovernanceLifecycleStoreResult save(
        const economics::GovernanceLifecycleRecord& lifecycle
    ) const;

    GovernanceLifecycleStoreResult load(
        const std::string& lifecycleId
    ) const;

    std::filesystem::path pathForLifecycleId(
        const std::string& lifecycleId
    ) const;

private:
    std::filesystem::path m_lifecycleDirectory;

    static bool isSafeLifecycleId(const std::string& lifecycleId);
};

} // namespace nodo::node

#endif
