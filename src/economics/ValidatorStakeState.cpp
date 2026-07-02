#include "economics/ValidatorStakeState.hpp"

#include <algorithm>
#include <sstream>

namespace nodo::economics {

std::string bondingStatusToString(BondingStatus status) {
  switch (status) {
  case BondingStatus::BONDED:
    return "BONDED";
  case BondingStatus::UNBONDING:
    return "UNBONDING";
  case BondingStatus::JAILED:
    return "JAILED";
  case BondingStatus::TOMBSTONED:
    return "TOMBSTONED";
  default:
    return "UNKNOWN";
  }
}

ValidatorStakeState::ValidatorStakeState()
    : m_account(), m_status(BondingStatus::BONDED) {}

ValidatorStakeState::ValidatorStakeState(StakeAccount account)
    : m_account(std::move(account)), m_status(BondingStatus::BONDED) {
  updateBondingStatus();
}

const StakeAccount &ValidatorStakeState::account() const { return m_account; }
StakeAccount &ValidatorStakeState::account() { return m_account; }
BondingStatus ValidatorStakeState::bondingStatus() const { return m_status; }

bool ValidatorStakeState::hasAppliedEvidence(
    const std::string &evidenceId) const {
  return std::find(m_appliedEvidenceIds.begin(), m_appliedEvidenceIds.end(),
                   evidenceId) != m_appliedEvidenceIds.end();
}

void ValidatorStakeState::recordAppliedEvidence(const std::string &evidenceId) {
  if (!hasAppliedEvidence(evidenceId)) {
    m_appliedEvidenceIds.push_back(evidenceId);
  }
}

void ValidatorStakeState::updateBondingStatus() {
  if (m_account.tombstoned()) {
    m_status = BondingStatus::TOMBSTONED;
  } else if (m_account.jailed()) {
    m_status = BondingStatus::JAILED;
  } else {
    m_status = BondingStatus::BONDED;
  }
}

bool ValidatorStakeState::isValid() const { return m_account.isValid(); }

std::string ValidatorStakeState::serialize() const {
  std::ostringstream oss;
  oss << "ValidatorStakeState{"
      << "status=" << bondingStatusToString(m_status)
      << ";evidenceCount=" << m_appliedEvidenceIds.size() << ";"
      << m_account.serialize() << "}";
  return oss.str();
}

} // namespace nodo::economics
