#ifndef NODO_NODE_SLASHING_EXECUTOR_HPP
#define NODO_NODE_SLASHING_EXECUTOR_HPP

#include "consensus/ValidatorPenaltyApplication.hpp"
#include "economics/StakeAccount.hpp"
#include "utils/Amount.hpp"

#include <string>

namespace nodo::node {

/*
 * SlashingExecutionRecord is the auditable artifact of one slash execution.
 *
 * It records what penalty decision was applied, to which StakeAccount, and
 * what the resulting stake state is.  This record must be persisted inside
 * the finalized block artifact so that chain audit can verify that the
 * slash was applied exactly once with the correct amount.
 */
class SlashingExecutionRecord {
public:
    SlashingExecutionRecord();

    SlashingExecutionRecord(
        std::string                             penaltyId,
        std::string                             evidenceId,
        std::string                             validatorAddress,
        utils::Amount                           slashedAmount,
        bool                                    jailed,
        bool                                    tombstoned,
        utils::Amount                           bondedAmountBefore,
        utils::Amount                           bondedAmountAfter,
        std::int64_t                            executedAt
    );

    const std::string& penaltyId()          const;
    const std::string& evidenceId()         const;
    const std::string& validatorAddress()   const;
    utils::Amount      slashedAmount()      const;
    bool               jailed()             const;
    bool               tombstoned()         const;
    utils::Amount      bondedAmountBefore() const;
    utils::Amount      bondedAmountAfter()  const;
    std::int64_t       executedAt()         const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string   m_penaltyId;
    std::string   m_evidenceId;
    std::string   m_validatorAddress;
    utils::Amount m_slashedAmount;
    bool          m_jailed;
    bool          m_tombstoned;
    utils::Amount m_bondedAmountBefore;
    utils::Amount m_bondedAmountAfter;
    std::int64_t  m_executedAt;
};

enum class SlashingExecutionStatus {
    APPLIED,
    DUPLICATE,
    INSUFFICIENT_STAKE,
    INVALID_DECISION,
    NO_ACTION_REQUIRED
};

std::string slashingExecutionStatusToString(SlashingExecutionStatus status);

class SlashingExecutionResult {
public:
    SlashingExecutionResult();

    static SlashingExecutionResult applied(
        economics::StakeAccount    updatedStake,
        SlashingExecutionRecord    record
    );
    static SlashingExecutionResult noAction(const std::string& reason);
    static SlashingExecutionResult rejected(
        SlashingExecutionStatus status,
        std::string             reason
    );

    SlashingExecutionStatus        status()       const;
    const std::string&             reason()       const;
    bool                           applied()      const;
    const economics::StakeAccount& updatedStake() const;
    const SlashingExecutionRecord& record()       const;

    std::string serialize() const;

private:
    SlashingExecutionStatus m_status;
    std::string             m_reason;
    economics::StakeAccount m_updatedStake;
    SlashingExecutionRecord m_record;
};

/*
 * SlashingExecutor applies a ValidatorPenaltyDecision to a StakeAccount.
 *
 * Idempotency guarantee:
 * The caller must track applied penaltyIds and pass alreadyApplied=true when
 * replaying state.  This executor does not maintain internal state — it is
 * the caller's responsibility to prevent double-application.
 *
 * Security principle:
 * Slash amounts are capped at the current bondedAmount to prevent negative
 * stake.  The executor never invents a slash amount — it uses exactly what
 * the ValidatorPenaltyDecision contains, which was derived deterministically
 * from the ValidatorPenaltyPolicy at the time evidence was admitted.
 */
class SlashingExecutor {
public:
    static SlashingExecutionResult execute(
        const consensus::ValidatorPenaltyDecision& decision,
        const economics::StakeAccount&             currentStake,
        std::int64_t                               now,
        bool                                       alreadyApplied = false
    );
};

} // namespace nodo::node

#endif
