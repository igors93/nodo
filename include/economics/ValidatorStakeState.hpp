#ifndef NODO_ECONOMICS_VALIDATOR_STAKE_STATE_HPP
#define NODO_ECONOMICS_VALIDATOR_STAKE_STATE_HPP

#include "economics/StakeAccount.hpp"

#include <string>
#include <vector>

namespace nodo::economics {

enum class BondingStatus {
    BONDED,
    UNBONDING,
    JAILED,
    TOMBSTONED
};

std::string bondingStatusToString(BondingStatus status);

/*
 * ValidatorStakeState wraps a StakeAccount with the validator lifecycle
 * status and an idempotency log of applied slash evidence IDs.
 *
 * Security principle:
 * Slash idempotency is a first-class invariant. The same slashing evidence
 * must not reduce the stake twice. All evidence IDs are recorded here before
 * the slash is applied, so that any re-delivery of evidence is detected.
 */
class ValidatorStakeState {
public:
    ValidatorStakeState();

    explicit ValidatorStakeState(StakeAccount account);

    const StakeAccount& account() const;
    StakeAccount& account();
    BondingStatus bondingStatus() const;

    bool hasAppliedEvidence(const std::string& evidenceId) const;
    void recordAppliedEvidence(const std::string& evidenceId);

    void updateBondingStatus();

    bool isValid() const;
    std::string serialize() const;

private:
    StakeAccount m_account;
    BondingStatus m_status;
    std::vector<std::string> m_appliedEvidenceIds;
};

} // namespace nodo::economics

#endif
