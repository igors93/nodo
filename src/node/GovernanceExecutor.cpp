#include "node/GovernanceExecutor.hpp"

#include "core/ProtocolLimits.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

std::string governanceParameterTargetToString(GovernanceParameterTarget target) {
    switch (target) {
        case GovernanceParameterTarget::EPOCH_DURATION_SECONDS:          return "EPOCH_DURATION_SECONDS";
        case GovernanceParameterTarget::MINIMUM_VALIDATOR_COUNT:         return "MINIMUM_VALIDATOR_COUNT";
        case GovernanceParameterTarget::QUORUM_THRESHOLD_NUMERATOR:      return "QUORUM_THRESHOLD_NUMERATOR";
        case GovernanceParameterTarget::QUORUM_THRESHOLD_DENOMINATOR:    return "QUORUM_THRESHOLD_DENOMINATOR";
        case GovernanceParameterTarget::MAX_TRANSACTIONS_PER_BLOCK:      return "MAX_TRANSACTIONS_PER_BLOCK";
        case GovernanceParameterTarget::MINIMUM_FEE_RAW:                 return "MINIMUM_FEE_RAW";
        case GovernanceParameterTarget::TREASURY_ALLOCATION_BASIS_POINTS: return "TREASURY_ALLOCATION_BASIS_POINTS";
        case GovernanceParameterTarget::VALIDATOR_REWARD_BASIS_POINTS:   return "VALIDATOR_REWARD_BASIS_POINTS";
        case GovernanceParameterTarget::UNKNOWN:                         return "UNKNOWN";
    }
    return "UNKNOWN";
}

GovernanceParameterTarget governanceParameterTargetFromString(const std::string& s) {
    if (s == "EPOCH_DURATION_SECONDS")            return GovernanceParameterTarget::EPOCH_DURATION_SECONDS;
    if (s == "MINIMUM_VALIDATOR_COUNT")           return GovernanceParameterTarget::MINIMUM_VALIDATOR_COUNT;
    if (s == "QUORUM_THRESHOLD_NUMERATOR")        return GovernanceParameterTarget::QUORUM_THRESHOLD_NUMERATOR;
    if (s == "QUORUM_THRESHOLD_DENOMINATOR")      return GovernanceParameterTarget::QUORUM_THRESHOLD_DENOMINATOR;
    if (s == "MAX_TRANSACTIONS_PER_BLOCK")        return GovernanceParameterTarget::MAX_TRANSACTIONS_PER_BLOCK;
    if (s == "MINIMUM_FEE_RAW")                   return GovernanceParameterTarget::MINIMUM_FEE_RAW;
    if (s == "TREASURY_ALLOCATION_BASIS_POINTS")  return GovernanceParameterTarget::TREASURY_ALLOCATION_BASIS_POINTS;
    if (s == "VALIDATOR_REWARD_BASIS_POINTS")     return GovernanceParameterTarget::VALIDATOR_REWARD_BASIS_POINTS;
    return GovernanceParameterTarget::UNKNOWN;
}

std::string governanceExecutionStatusToString(GovernanceExecutionStatus status) {
    switch (status) {
        case GovernanceExecutionStatus::APPLIED:                 return "APPLIED";
        case GovernanceExecutionStatus::PENDING:                 return "PENDING";
        case GovernanceExecutionStatus::REJECTED_UNKNOWN_TARGET: return "REJECTED_UNKNOWN_TARGET";
        case GovernanceExecutionStatus::REJECTED_INVALID_VALUE:  return "REJECTED_INVALID_VALUE";
        case GovernanceExecutionStatus::REJECTED_NOT_YET_EFFECTIVE: return "REJECTED_NOT_YET_EFFECTIVE";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// GovernanceParameterChange
// ---------------------------------------------------------------------------

GovernanceParameterChange::GovernanceParameterChange()
    : m_proposalId("")
    , m_target(GovernanceParameterTarget::UNKNOWN)
    , m_previousValue("")
    , m_newValue("")
    , m_effectiveAtHeight(0)
    , m_appliedAt(0)
{}

GovernanceParameterChange::GovernanceParameterChange(
    std::string proposalId,
    GovernanceParameterTarget target,
    std::string previousValue,
    std::string newValue,
    std::uint64_t effectiveAtHeight,
    std::int64_t appliedAt
)
    : m_proposalId(std::move(proposalId))
    , m_target(target)
    , m_previousValue(std::move(previousValue))
    , m_newValue(std::move(newValue))
    , m_effectiveAtHeight(effectiveAtHeight)
    , m_appliedAt(appliedAt)
{}

GovernanceParameterChange GovernanceParameterChange::pending(
    std::string proposalId,
    GovernanceParameterTarget target,
    std::string newValue,
    std::uint64_t effectiveAtHeight
) {
    return GovernanceParameterChange(
        std::move(proposalId),
        target,
        "",  // previousValue unknown until applied
        std::move(newValue),
        effectiveAtHeight,
        0    // 0 = not yet applied
    );
}

const std::string& GovernanceParameterChange::proposalId() const { return m_proposalId; }
GovernanceParameterTarget GovernanceParameterChange::target() const { return m_target; }
const std::string& GovernanceParameterChange::previousValue() const { return m_previousValue; }
const std::string& GovernanceParameterChange::newValue() const { return m_newValue; }
std::uint64_t GovernanceParameterChange::effectiveAtHeight() const { return m_effectiveAtHeight; }
std::int64_t GovernanceParameterChange::appliedAt() const { return m_appliedAt; }

bool GovernanceParameterChange::isApplied() const {
    return m_appliedAt > 0;
}

bool GovernanceParameterChange::isValid() const {
    return !m_proposalId.empty() &&
           m_target != GovernanceParameterTarget::UNKNOWN &&
           !m_newValue.empty() &&
           m_effectiveAtHeight > 0;
}

std::string GovernanceParameterChange::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceParameterChange{"
        << "proposalId=" << m_proposalId
        << ";target=" << governanceParameterTargetToString(m_target)
        << ";previousValue=" << m_previousValue
        << ";newValue=" << m_newValue
        << ";effectiveAtHeight=" << m_effectiveAtHeight
        << ";appliedAt=" << m_appliedAt
        << "}";
    return oss.str();
}

GovernanceParameterChange GovernanceParameterChange::deserialize(const std::string& s) {
    // Helper: extract value for key= in key=value; format
    auto extractField = [&](const std::string& key) -> std::string {
        const std::string search = key + "=";
        const std::size_t pos = s.find(search);
        if (pos == std::string::npos) {
            return "";
        }
        const std::size_t valueStart = pos + search.size();
        const std::size_t valueEnd = s.find(';', valueStart);
        if (valueEnd == std::string::npos) {
            std::size_t end = s.find('}', valueStart);
            if (end == std::string::npos) {
                end = s.size();
            }
            return s.substr(valueStart, end - valueStart);
        }
        return s.substr(valueStart, valueEnd - valueStart);
    };

    const std::string proposalId       = extractField("proposalId");
    const std::string targetStr        = extractField("target");
    const std::string previousValue    = extractField("previousValue");
    const std::string newValue         = extractField("newValue");
    const std::string effectiveAtStr   = extractField("effectiveAtHeight");
    const std::string appliedAtStr     = extractField("appliedAt");

    const std::uint64_t effectiveAt = effectiveAtStr.empty() ? 0 : std::stoull(effectiveAtStr);
    const std::int64_t  appliedAt   = appliedAtStr.empty()   ? 0 : std::stoll(appliedAtStr);

    return GovernanceParameterChange(
        proposalId,
        governanceParameterTargetFromString(targetStr),
        previousValue,
        newValue,
        effectiveAt,
        appliedAt
    );
}

// ---------------------------------------------------------------------------
// GovernanceExecutionResult
// ---------------------------------------------------------------------------

GovernanceExecutionResult::GovernanceExecutionResult(
    GovernanceExecutionStatus status,
    std::string detail,
    GovernanceParameterChange change
)
    : m_status(status)
    , m_detail(std::move(detail))
    , m_change(std::move(change))
{}

GovernanceExecutionResult GovernanceExecutionResult::applied(
    GovernanceParameterChange change
) {
    return GovernanceExecutionResult(
        GovernanceExecutionStatus::APPLIED,
        "Applied: " + governanceParameterTargetToString(change.target()) +
            " = " + change.newValue(),
        std::move(change)
    );
}

GovernanceExecutionResult GovernanceExecutionResult::pending(
    std::string proposalId,
    std::uint64_t effectiveAtHeight,
    std::uint64_t currentHeight
) {
    std::ostringstream detail;
    detail << "Proposal " << proposalId
           << " effective at height " << effectiveAtHeight
           << " (current: " << currentHeight << ")";

    return GovernanceExecutionResult(
        GovernanceExecutionStatus::PENDING,
        detail.str(),
        GovernanceParameterChange{}
    );
}

GovernanceExecutionResult GovernanceExecutionResult::rejected(
    GovernanceExecutionStatus reason,
    std::string detail
) {
    return GovernanceExecutionResult(reason, std::move(detail), GovernanceParameterChange{});
}

GovernanceExecutionStatus GovernanceExecutionResult::status() const { return m_status; }
const std::string& GovernanceExecutionResult::detail() const { return m_detail; }
bool GovernanceExecutionResult::isApplied() const { return m_status == GovernanceExecutionStatus::APPLIED; }
bool GovernanceExecutionResult::isPending() const { return m_status == GovernanceExecutionStatus::PENDING; }
const GovernanceParameterChange& GovernanceExecutionResult::change() const { return m_change; }

std::string GovernanceExecutionResult::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceExecutionResult{"
        << "status=" << governanceExecutionStatusToString(m_status)
        << ";detail=" << m_detail
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// GovernanceExecutor — private helpers
// ---------------------------------------------------------------------------

namespace {

bool parseUint64Strict(
    const std::string& value,
    std::uint64_t& parsedValue
) {
    if (value.empty()) {
        return false;
    }
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
    }

    try {
        std::size_t parsedCharacters = 0;
        const unsigned long long parsed =
            std::stoull(value, &parsedCharacters);
        if (parsedCharacters != value.size() ||
            parsed > std::numeric_limits<std::uint64_t>::max() ||
            std::to_string(parsed) != value) {
            return false;
        }
        parsedValue = static_cast<std::uint64_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Extract value from the canonical transaction-safe "key=value,key=value" form.
// Semicolons remain accepted for durable data created by earlier revisions.
std::string extractPayloadField(const std::string& payload, const std::string& key) {
    const std::string search = key + "=";
    const std::size_t pos = payload.find(search);
    if (pos == std::string::npos) {
        return "";
    }
    const std::size_t valueStart = pos + search.size();
    const std::size_t semicolonEnd = payload.find(';', valueStart);
    const std::size_t commaEnd = payload.find(',', valueStart);
    const std::size_t valueEnd =
        semicolonEnd == std::string::npos ? commaEnd :
        commaEnd == std::string::npos ? semicolonEnd :
        std::min(semicolonEnd, commaEnd);
    if (valueEnd == std::string::npos) {
        return payload.substr(valueStart);
    }
    return payload.substr(valueStart, valueEnd - valueStart);
}

} // namespace

GovernanceParameterTarget GovernanceExecutor::parseTarget(const std::string& payload) {
    const std::string targetStr = extractPayloadField(payload, "target");
    return governanceParameterTargetFromString(targetStr);
}

std::string GovernanceExecutor::parseNewValue(const std::string& payload) {
    return extractPayloadField(payload, "value");
}

std::uint64_t GovernanceExecutor::parseEffectiveHeight(const std::string& payload) {
    const std::string heightStr = extractPayloadField(payload, "effectiveHeight");
    if (heightStr.empty()) {
        return 0;
    }
    std::uint64_t effectiveHeight = 0;
    return parseUint64Strict(heightStr, effectiveHeight)
        ? effectiveHeight
        : 0;
}

bool GovernanceExecutor::validateValue(
    GovernanceParameterTarget target,
    const std::string& value
) const {
    if (value.empty()) {
        return false;
    }

    std::uint64_t numericValue = 0;
    if (!parseUint64Strict(value, numericValue)) {
        return false;
    }

    switch (target) {
        case GovernanceParameterTarget::EPOCH_DURATION_SECONDS:
            return numericValue >= 60;    // at least 1 minute

        case GovernanceParameterTarget::MINIMUM_VALIDATOR_COUNT:
            return numericValue >= 1;

        case GovernanceParameterTarget::QUORUM_THRESHOLD_NUMERATOR:
            return numericValue >= 1;

        case GovernanceParameterTarget::QUORUM_THRESHOLD_DENOMINATOR:
            return numericValue >= 2;

        case GovernanceParameterTarget::MAX_TRANSACTIONS_PER_BLOCK:
            return numericValue >= 1 &&
                   numericValue <= core::ProtocolLimits::MAX_BLOCK_RECORDS;

        case GovernanceParameterTarget::MINIMUM_FEE_RAW:
            return numericValue <= static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()
            );

        case GovernanceParameterTarget::TREASURY_ALLOCATION_BASIS_POINTS:
            return numericValue <= 10000;  // max 100%

        case GovernanceParameterTarget::VALIDATOR_REWARD_BASIS_POINTS:
            return numericValue <= 10000;  // max 100%

        case GovernanceParameterTarget::UNKNOWN:
            return false;
    }

    return false;
}

// ---------------------------------------------------------------------------
// GovernanceExecutor — public interface
// ---------------------------------------------------------------------------

GovernanceExecutor::GovernanceExecutor() = default;

GovernanceExecutionResult GovernanceExecutor::executeProposal(
    const std::string& proposalId,
    const std::string& proposalPayload,
    std::uint64_t currentHeight,
    std::int64_t  now
) {
    // Prevent double-execution
    if (hasBeenExecuted(proposalId)) {
        return GovernanceExecutionResult::rejected(
            GovernanceExecutionStatus::REJECTED_INVALID_VALUE,
            "Proposal " + proposalId + " has already been executed."
        );
    }

    const GovernanceParameterTarget target = parseTarget(proposalPayload);

    if (target == GovernanceParameterTarget::UNKNOWN) {
        return GovernanceExecutionResult::rejected(
            GovernanceExecutionStatus::REJECTED_UNKNOWN_TARGET,
            "Unknown governance parameter target in proposal " + proposalId
        );
    }

    const std::string newValue = parseNewValue(proposalPayload);
    const std::uint64_t effectiveAtHeight = parseEffectiveHeight(proposalPayload);

    if (effectiveAtHeight == 0 || !validateValue(target, newValue)) {
        return GovernanceExecutionResult::rejected(
            GovernanceExecutionStatus::REJECTED_INVALID_VALUE,
            "Invalid value '" + newValue + "' for target " +
            governanceParameterTargetToString(target)
        );
    }

    if (currentHeight < effectiveAtHeight) {
        // Not yet effective — record as pending
        GovernanceParameterChange pendingChange = GovernanceParameterChange::pending(
            proposalId,
            target,
            newValue,
            effectiveAtHeight
        );

        // Avoid duplicate pending entries
        bool alreadyPending = false;
        for (const auto& pc : m_pendingChanges) {
            if (pc.proposalId() == proposalId) {
                alreadyPending = true;
                break;
            }
        }
        if (!alreadyPending) {
            m_pendingChanges.push_back(pendingChange);
        }

        return GovernanceExecutionResult::pending(proposalId, effectiveAtHeight, currentHeight);
    }

    // Height is sufficient — apply the change
    const std::string previousValue = currentValueForTarget(target);

    GovernanceParameterChange appliedChange(
        proposalId,
        target,
        previousValue,
        newValue,
        effectiveAtHeight,
        now
    );

    m_appliedChanges.push_back(appliedChange);

    // Remove from pending if it was there
    m_pendingChanges.erase(
        std::remove_if(
            m_pendingChanges.begin(),
            m_pendingChanges.end(),
            [&proposalId](const GovernanceParameterChange& pc) {
                return pc.proposalId() == proposalId;
            }
        ),
        m_pendingChanges.end()
    );

    return GovernanceExecutionResult::applied(appliedChange);
}

std::size_t GovernanceExecutor::advanceToHeight(
    std::uint64_t currentHeight,
    std::int64_t now
) {
    if (currentHeight == 0 || now <= 0) {
        throw std::invalid_argument("Governance activation boundary is invalid.");
    }

    std::size_t activated = 0;
    std::vector<GovernanceParameterChange> remaining;
    remaining.reserve(m_pendingChanges.size());

    for (const GovernanceParameterChange& pending : m_pendingChanges) {
        if (pending.effectiveAtHeight() > currentHeight) {
            remaining.push_back(pending);
            continue;
        }

        m_appliedChanges.emplace_back(
            pending.proposalId(),
            pending.target(),
            currentValueForTarget(pending.target()),
            pending.newValue(),
            pending.effectiveAtHeight(),
            now
        );
        ++activated;
    }

    m_pendingChanges = std::move(remaining);
    return activated;
}

const std::vector<GovernanceParameterChange>& GovernanceExecutor::appliedChanges() const {
    return m_appliedChanges;
}

std::vector<GovernanceParameterChange> GovernanceExecutor::pendingChanges() const {
    return m_pendingChanges;
}

bool GovernanceExecutor::hasBeenExecuted(const std::string& proposalId) const {
    for (const auto& change : m_appliedChanges) {
        if (change.proposalId() == proposalId) {
            return true;
        }
    }
    return false;
}

std::string GovernanceExecutor::currentValueForTarget(
    GovernanceParameterTarget target
) const {
    // Scan applied changes in reverse to get the most recent value
    for (auto it = m_appliedChanges.rbegin(); it != m_appliedChanges.rend(); ++it) {
        if (it->target() == target) {
            return it->newValue();
        }
    }
    return "";
}

std::string GovernanceExecutor::serialize() const {
    std::ostringstream oss;
    oss << "GovernanceExecutor{"
        << "appliedCount=" << m_appliedChanges.size()
        << ";pendingCount=" << m_pendingChanges.size()
        << "}";
    return oss.str();
}

} // namespace nodo::node
