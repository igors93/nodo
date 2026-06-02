#include "node/GovernanceLifecycleStore.hpp"

#include "economics/GovernanceLifecycleVerifier.hpp"
#include "node/GovernanceLifecycleCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <stdexcept>
#include <utility>

namespace nodo::node {

std::string governanceLifecycleStoreStatusToString(
    GovernanceLifecycleStoreStatus status
) {
    switch (status) {
        case GovernanceLifecycleStoreStatus::STORED:
            return "STORED";
        case GovernanceLifecycleStoreStatus::LOADED:
            return "LOADED";
        case GovernanceLifecycleStoreStatus::NOT_FOUND:
            return "NOT_FOUND";
        case GovernanceLifecycleStoreStatus::INVALID_RECORD:
            return "INVALID_RECORD";
        case GovernanceLifecycleStoreStatus::IO_ERROR:
            return "IO_ERROR";
        default:
            return "UNKNOWN";
    }
}

GovernanceLifecycleStoreResult::GovernanceLifecycleStoreResult()
    : m_status(GovernanceLifecycleStoreStatus::IO_ERROR),
      m_reason("Uninitialized governance lifecycle store result."),
      m_lifecycle() {}

GovernanceLifecycleStoreResult GovernanceLifecycleStoreResult::stored(
    economics::GovernanceLifecycleRecord lifecycle
) {
    GovernanceLifecycleStoreResult result;
    result.m_status = GovernanceLifecycleStoreStatus::STORED;
    result.m_lifecycle = std::move(lifecycle);
    return result;
}

GovernanceLifecycleStoreResult GovernanceLifecycleStoreResult::loaded(
    economics::GovernanceLifecycleRecord lifecycle
) {
    GovernanceLifecycleStoreResult result;
    result.m_status = GovernanceLifecycleStoreStatus::LOADED;
    result.m_lifecycle = std::move(lifecycle);
    return result;
}

GovernanceLifecycleStoreResult GovernanceLifecycleStoreResult::rejected(
    GovernanceLifecycleStoreStatus status,
    std::string reason
) {
    GovernanceLifecycleStoreResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

bool GovernanceLifecycleStoreResult::success() const {
    return m_status == GovernanceLifecycleStoreStatus::STORED ||
           m_status == GovernanceLifecycleStoreStatus::LOADED;
}
GovernanceLifecycleStoreStatus GovernanceLifecycleStoreResult::status() const {
    return m_status;
}
const std::string& GovernanceLifecycleStoreResult::reason() const {
    return m_reason;
}
const economics::GovernanceLifecycleRecord&
GovernanceLifecycleStoreResult::lifecycle() const {
    return m_lifecycle;
}

GovernanceLifecycleStore::GovernanceLifecycleStore(
    std::filesystem::path lifecycleDirectory
)
    : m_lifecycleDirectory(std::move(lifecycleDirectory)) {}

GovernanceLifecycleStoreResult GovernanceLifecycleStore::save(
    const economics::GovernanceLifecycleRecord& lifecycle
) const {
    const economics::GovernanceLifecycleVerificationResult verification =
        economics::GovernanceLifecycleVerifier::verify(lifecycle);

    if (!verification.verified()) {
        return GovernanceLifecycleStoreResult::rejected(
            GovernanceLifecycleStoreStatus::INVALID_RECORD,
            "GovernanceLifecycleStore: lifecycle verification failed before save: " +
            verification.reason()
        );
    }

    try {
        std::filesystem::create_directories(m_lifecycleDirectory);
        storage::AtomicFile::writeTextFile(
            pathForLifecycleId(lifecycle.lifecycleId()),
            GovernanceLifecycleCodec::encode(lifecycle)
        );
        return GovernanceLifecycleStoreResult::stored(lifecycle);
    } catch (const std::exception& error) {
        return GovernanceLifecycleStoreResult::rejected(
            GovernanceLifecycleStoreStatus::IO_ERROR,
            error.what()
        );
    }
}

GovernanceLifecycleStoreResult GovernanceLifecycleStore::load(
    const std::string& lifecycleId
) const {
    if (!isSafeLifecycleId(lifecycleId)) {
        return GovernanceLifecycleStoreResult::rejected(
            GovernanceLifecycleStoreStatus::INVALID_RECORD,
            "GovernanceLifecycleStore: lifecycleId is unsafe."
        );
    }

    const std::filesystem::path path = pathForLifecycleId(lifecycleId);
    if (!std::filesystem::exists(path)) {
        return GovernanceLifecycleStoreResult::rejected(
            GovernanceLifecycleStoreStatus::NOT_FOUND,
            "GovernanceLifecycleStore: lifecycle not found: " + lifecycleId
        );
    }

    try {
        economics::GovernanceLifecycleRecord lifecycle =
            GovernanceLifecycleCodec::decode(storage::AtomicFile::readTextFile(path));

        const economics::GovernanceLifecycleVerificationResult verification =
            economics::GovernanceLifecycleVerifier::verify(lifecycle);

        if (!verification.verified()) {
            return GovernanceLifecycleStoreResult::rejected(
                GovernanceLifecycleStoreStatus::INVALID_RECORD,
                "GovernanceLifecycleStore: persisted lifecycle failed verification: " +
                verification.reason()
            );
        }

        return GovernanceLifecycleStoreResult::loaded(std::move(lifecycle));
    } catch (const std::exception& error) {
        return GovernanceLifecycleStoreResult::rejected(
            GovernanceLifecycleStoreStatus::IO_ERROR,
            error.what()
        );
    }
}

std::filesystem::path GovernanceLifecycleStore::pathForLifecycleId(
    const std::string& lifecycleId
) const {
    if (!isSafeLifecycleId(lifecycleId)) {
        throw std::invalid_argument("Governance lifecycle id is unsafe.");
    }

    return m_lifecycleDirectory / (lifecycleId + ".govlife");
}

bool GovernanceLifecycleStore::isSafeLifecycleId(
    const std::string& lifecycleId
) {
    if (lifecycleId.empty() || lifecycleId.size() > 200) {
        return false;
    }

    for (const char current : lifecycleId) {
        const bool allowed =
            (current >= 'a' && current <= 'z') ||
            (current >= 'A' && current <= 'Z') ||
            (current >= '0' && current <= '9') ||
            current == '_' ||
            current == '-' ||
            current == '.';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::node
