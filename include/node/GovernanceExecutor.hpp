#ifndef NODO_NODE_GOVERNANCE_EXECUTOR_HPP
#define NODO_NODE_GOVERNANCE_EXECUTOR_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace nodo::node {

/*
 * GovernanceParameterTarget identifies which protocol parameter a governance
 * proposal is requesting to change.
 */
enum class GovernanceParameterTarget {
    EPOCH_DURATION_SECONDS,
    MINIMUM_VALIDATOR_COUNT,
    QUORUM_THRESHOLD_NUMERATOR,
    QUORUM_THRESHOLD_DENOMINATOR,
    MAX_TRANSACTIONS_PER_BLOCK,
    MINIMUM_FEE_RAW,
    TREASURY_ALLOCATION_BASIS_POINTS,
    VALIDATOR_REWARD_BASIS_POINTS,
    UNKNOWN
};

std::string governanceParameterTargetToString(GovernanceParameterTarget target);
GovernanceParameterTarget governanceParameterTargetFromString(const std::string& s);

/*
 * GovernanceParameterChange is an immutable audit record of one applied or
 * pending parameter change. Every applied change stores both the previous and
 * new values so any node can independently verify the parameter history.
 *
 * Security principle:
 * Parameter changes must be deterministic and auditable. The proposalId ties
 * each change to the governance vote that authorized it.
 */
class GovernanceParameterChange {
public:
    GovernanceParameterChange();

    GovernanceParameterChange(
        std::string proposalId,
        GovernanceParameterTarget target,
        std::string previousValue,
        std::string newValue,
        std::uint64_t effectiveAtHeight,
        std::int64_t appliedAt
    );

    static GovernanceParameterChange pending(
        std::string proposalId,
        GovernanceParameterTarget target,
        std::string newValue,
        std::uint64_t effectiveAtHeight
    );

    const std::string& proposalId() const;
    GovernanceParameterTarget target() const;
    const std::string& previousValue() const;
    const std::string& newValue() const;
    std::uint64_t effectiveAtHeight() const;
    std::int64_t appliedAt() const;

    bool isApplied() const;
    bool isValid() const;
    std::string serialize() const;
    static GovernanceParameterChange deserialize(const std::string& s);

private:
    std::string m_proposalId;
    GovernanceParameterTarget m_target;
    std::string m_previousValue;
    std::string m_newValue;
    std::uint64_t m_effectiveAtHeight;
    std::int64_t m_appliedAt;   // 0 = pending (not yet applied)
};

enum class GovernanceExecutionStatus {
    APPLIED,
    PENDING,
    REJECTED_UNKNOWN_TARGET,
    REJECTED_INVALID_VALUE,
    REJECTED_NOT_YET_EFFECTIVE
};

std::string governanceExecutionStatusToString(GovernanceExecutionStatus status);

class GovernanceExecutionResult {
public:
    static GovernanceExecutionResult applied(
        GovernanceParameterChange change
    );
    static GovernanceExecutionResult pending(
        std::string proposalId,
        std::uint64_t effectiveAtHeight,
        std::uint64_t currentHeight
    );
    static GovernanceExecutionResult rejected(
        GovernanceExecutionStatus reason,
        std::string detail
    );

    GovernanceExecutionStatus status() const;
    const std::string& detail() const;
    bool isApplied() const;
    bool isPending() const;
    const GovernanceParameterChange& change() const;
    std::string serialize() const;

private:
    GovernanceExecutionStatus m_status;
    std::string m_detail;
    GovernanceParameterChange m_change;

    GovernanceExecutionResult(
        GovernanceExecutionStatus status,
        std::string detail,
        GovernanceParameterChange change
    );
};

/*
 * GovernanceExecutor applies approved governance decisions to live protocol
 * parameters. It is the single authority that transforms a DECIDED_APPROVED
 * governance lifecycle state into a concrete parameter change.
 *
 * Proposal payload format (key=value;key=value):
 *   target=EPOCH_DURATION_SECONDS;value=7200;effectiveHeight=1000
 *
 * Security principle:
 * Double-execution is prevented by tracking every executed proposalId.
 * Parameter changes are validated before application; unknown targets or
 * out-of-range values are rejected with an audit record.
 */
class GovernanceExecutor {
public:
    GovernanceExecutor();

    bool submitProposal(
        const std::string& proposalId,
        const std::string& proposerAddress,
        const std::string& proposalPayload,
        std::uint64_t currentHeight,
        std::int64_t now,
        std::string& reason
    );

    bool castVote(
        const std::string& proposalId,
        const std::string& validatorAddress,
        bool approve,
        std::uint64_t votingWeight,
        std::uint64_t totalEligibleWeight,
        std::uint64_t currentHeight,
        std::int64_t now,
        std::string& reason
    );

    bool hasProposal(const std::string& proposalId) const;
    bool hasVote(const std::string& proposalId, const std::string& validatorAddress) const;
    bool proposalApproved(const std::string& proposalId) const;

    // Activates every pending change whose effective height has been reached.
    // Returns the number of changes activated in deterministic insertion order.
    std::size_t advanceToHeight(
        std::uint64_t currentHeight,
        std::int64_t now
    );

    // Returns all applied changes in chronological order.
    const std::vector<GovernanceParameterChange>& appliedChanges() const;

    // Returns pending changes not yet effective.
    std::vector<GovernanceParameterChange> pendingChanges() const;

    // Check if a proposal has already been executed (prevents double-execution).
    bool hasBeenExecuted(const std::string& proposalId) const;

    // Get the current effective value for a parameter target.
    // Returns empty string if no governance change has been applied for this target.
    std::string currentValueForTarget(GovernanceParameterTarget target) const;

    std::string serialize() const;

private:
    struct VoteState {
        bool approve = false;
        std::uint64_t weight = 0;
    };
    struct ProposalState {
        std::string proposerAddress;
        std::string payload;
        std::uint64_t createdHeight = 0;
        std::int64_t createdAt = 0;
        bool approved = false;
        bool rejected = false;
        std::map<std::string, VoteState> votes;
    };
    std::vector<GovernanceParameterChange> m_appliedChanges;
    std::vector<GovernanceParameterChange> m_pendingChanges;
    std::map<std::string, ProposalState> m_proposals;

    GovernanceExecutionResult applyApprovedProposal(
        const std::string& proposalId,
        const std::string& proposalPayload,
        std::uint64_t currentHeight,
        std::int64_t now
    );

    static GovernanceParameterTarget parseTarget(const std::string& payload);
    static std::string parseNewValue(const std::string& payload);
    static std::uint64_t parseEffectiveHeight(const std::string& payload);

    bool validateValue(GovernanceParameterTarget target, const std::string& value) const;
};

} // namespace nodo::node

#endif
