#include "node/SlashingExecutor.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace nodo::node {

SlashingExecutionRecord::SlashingExecutionRecord()
    : m_slashedAmount(utils::Amount())
    , m_jailed(false)
    , m_tombstoned(false)
    , m_bondedAmountBefore(utils::Amount())
    , m_bondedAmountAfter(utils::Amount())
    , m_executedAt(0)
{}

SlashingExecutionRecord::SlashingExecutionRecord(
    std::string   penaltyId,
    std::string   evidenceId,
    std::string   validatorAddress,
    utils::Amount slashedAmount,
    bool          jailed,
    bool          tombstoned,
    utils::Amount bondedAmountBefore,
    utils::Amount bondedAmountAfter,
    std::int64_t  executedAt
)
    : m_penaltyId(std::move(penaltyId))
    , m_evidenceId(std::move(evidenceId))
    , m_validatorAddress(std::move(validatorAddress))
    , m_slashedAmount(slashedAmount)
    , m_jailed(jailed)
    , m_tombstoned(tombstoned)
    , m_bondedAmountBefore(bondedAmountBefore)
    , m_bondedAmountAfter(bondedAmountAfter)
    , m_executedAt(executedAt)
{}

const std::string& SlashingExecutionRecord::penaltyId()          const { return m_penaltyId; }
const std::string& SlashingExecutionRecord::evidenceId()         const { return m_evidenceId; }
const std::string& SlashingExecutionRecord::validatorAddress()   const { return m_validatorAddress; }
utils::Amount      SlashingExecutionRecord::slashedAmount()      const { return m_slashedAmount; }
bool               SlashingExecutionRecord::jailed()             const { return m_jailed; }
bool               SlashingExecutionRecord::tombstoned()         const { return m_tombstoned; }
utils::Amount      SlashingExecutionRecord::bondedAmountBefore() const { return m_bondedAmountBefore; }
utils::Amount      SlashingExecutionRecord::bondedAmountAfter()  const { return m_bondedAmountAfter; }
std::int64_t       SlashingExecutionRecord::executedAt()         const { return m_executedAt; }

bool SlashingExecutionRecord::isValid() const {
    return !m_penaltyId.empty() &&
           !m_evidenceId.empty() &&
           !m_validatorAddress.empty() &&
           m_executedAt > 0 &&
           m_bondedAmountAfter.rawUnits() <= m_bondedAmountBefore.rawUnits();
}

std::string SlashingExecutionRecord::serialize() const {
    std::ostringstream oss;
    oss << "SlashingExecutionRecord{"
        << "penaltyId=" << m_penaltyId
        << ";evidenceId=" << m_evidenceId
        << ";validator=" << m_validatorAddress
        << ";slashed=" << m_slashedAmount.rawUnits()
        << ";jailed=" << (m_jailed ? "true" : "false")
        << ";tombstoned=" << (m_tombstoned ? "true" : "false")
        << ";bondedBefore=" << m_bondedAmountBefore.rawUnits()
        << ";bondedAfter=" << m_bondedAmountAfter.rawUnits()
        << ";executedAt=" << m_executedAt
        << "}";
    return oss.str();
}

std::string slashingExecutionStatusToString(SlashingExecutionStatus status) {
    switch (status) {
        case SlashingExecutionStatus::APPLIED:              return "APPLIED";
        case SlashingExecutionStatus::DUPLICATE:            return "DUPLICATE";
        case SlashingExecutionStatus::INSUFFICIENT_STAKE:   return "INSUFFICIENT_STAKE";
        case SlashingExecutionStatus::INVALID_DECISION:     return "INVALID_DECISION";
        case SlashingExecutionStatus::NO_ACTION_REQUIRED:   return "NO_ACTION_REQUIRED";
        default:                                            return "INVALID_DECISION";
    }
}

SlashingExecutionResult::SlashingExecutionResult()
    : m_status(SlashingExecutionStatus::INVALID_DECISION)
    , m_reason("Uninitialized.")
    , m_updatedStake()
    , m_record()
{}

SlashingExecutionResult SlashingExecutionResult::applied(
    economics::StakeAccount updatedStake,
    SlashingExecutionRecord record
) {
    SlashingExecutionResult r;
    r.m_status       = SlashingExecutionStatus::APPLIED;
    r.m_reason       = "";
    r.m_updatedStake = std::move(updatedStake);
    r.m_record       = std::move(record);
    return r;
}

SlashingExecutionResult SlashingExecutionResult::noAction(const std::string& reason) {
    SlashingExecutionResult r;
    r.m_status = SlashingExecutionStatus::NO_ACTION_REQUIRED;
    r.m_reason = reason;
    return r;
}

SlashingExecutionResult SlashingExecutionResult::rejected(
    SlashingExecutionStatus status,
    std::string             reason
) {
    SlashingExecutionResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

SlashingExecutionStatus        SlashingExecutionResult::status()       const { return m_status; }
const std::string&             SlashingExecutionResult::reason()       const { return m_reason; }
bool                           SlashingExecutionResult::applied()      const { return m_status == SlashingExecutionStatus::APPLIED; }
const economics::StakeAccount& SlashingExecutionResult::updatedStake() const { return m_updatedStake; }
const SlashingExecutionRecord& SlashingExecutionResult::record()       const { return m_record; }

std::string SlashingExecutionResult::serialize() const {
    std::ostringstream oss;
    oss << "SlashingExecutionResult{"
        << "status=" << slashingExecutionStatusToString(m_status)
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

bool SlashingExecutor::isApplied(const std::string& penaltyId) const {
    return m_appliedPenaltyIds.count(penaltyId) > 0;
}

void SlashingExecutor::reset() {
    m_appliedPenaltyIds.clear();
}

SlashingExecutionResult SlashingExecutor::execute(
    const consensus::ValidatorPenaltyDecision& decision,
    const economics::StakeAccount&             currentStake,
    std::int64_t                               now
) {
    if (!decision.isValid()) {
        return SlashingExecutionResult::rejected(
            SlashingExecutionStatus::INVALID_DECISION,
            "Penalty decision is not valid."
        );
    }

    // Internal idempotency guard — the executor itself tracks applied ids so
    // callers cannot accidentally double-apply by omitting an alreadyApplied flag.
    if (m_appliedPenaltyIds.count(decision.penaltyId()) > 0) {
        return SlashingExecutionResult::rejected(
            SlashingExecutionStatus::DUPLICATE,
            "Penalty " + decision.penaltyId() + " already applied."
        );
    }

    if (decision.action() == consensus::ValidatorPenaltyAction::NONE ||
        decision.action() == consensus::ValidatorPenaltyAction::WARNING) {
        return SlashingExecutionResult::noAction(
            "Penalty action is " +
            consensus::validatorPenaltyActionToString(decision.action()) +
            " — no stake mutation required."
        );
    }

    const utils::Amount bondedBefore = currentStake.bondedAmount();

    economics::StakeAccount updated(
        currentStake.validatorAddress(),
        currentStake.bondedAmount()
    );

    // Re-apply existing jailed/tombstoned state.
    if (currentStake.jailed())     updated.jail();
    if (currentStake.tombstoned()) updated.tombstone();

    // Apply slash if amount > 0.
    utils::Amount actualSlash = utils::Amount();
    if (decision.slashable() && decision.slashAmountRawUnits() > 0) {
        // Cap slash at available bonded amount.
        const std::int64_t available =
            bondedBefore.rawUnits() - currentStake.slashedAmount().rawUnits();
        const std::int64_t slashRaw =
            std::min(decision.slashAmountRawUnits(), available);

        if (slashRaw <= 0) {
            return SlashingExecutionResult::rejected(
                SlashingExecutionStatus::INSUFFICIENT_STAKE,
                "No available stake to slash for validator "
                + decision.validatorAddress()
            );
        }

        actualSlash = utils::Amount::fromRawUnits(slashRaw);
        updated.applySlash(actualSlash);
    }

    // Jail if required.
    if (decision.jailsValidator()) {
        updated.jail();
    }

    // Tombstone if required (permanent).
    if (decision.tombstonesValidator()) {
        updated.tombstone();
    }

    const utils::Amount bondedAfter = utils::Amount::fromRawUnits(
        bondedBefore.rawUnits() - actualSlash.rawUnits()
    );

    SlashingExecutionRecord record(
        decision.penaltyId(),
        decision.evidenceId(),
        decision.validatorAddress(),
        actualSlash,
        updated.jailed(),
        updated.tombstoned(),
        bondedBefore,
        bondedAfter,
        now
    );

    m_appliedPenaltyIds.insert(decision.penaltyId());
    return SlashingExecutionResult::applied(std::move(updated), std::move(record));
}

} // namespace nodo::node
