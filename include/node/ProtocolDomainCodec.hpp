#ifndef NODO_NODE_PROTOCOL_DOMAIN_CODEC_HPP
#define NODO_NODE_PROTOCOL_DOMAIN_CODEC_HPP

#include "consensus/ValidatorPenaltyApplication.hpp"
#include "core/ValidatorRegistry.hpp"
#include "economics/BurnRecord.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/ProtocolTransactionDomainExecutor.hpp"
#include "node/StakingRegistry.hpp"
#include "utils/Amount.hpp"

#include <map>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * Canonical per-domain codecs for the protocol execution state carried by
 * ProtocolExecutionState (supply, burns, staking, governance, validators,
 * slashing, validator_weights).
 *
 * Every domain (except validator_weights, which is root-only) gets:
 *   - encode(value)          deterministic binary encode, hex-encoded so it
 *                             is safe to embed in the text-based snapshot
 *                             document (KeyValueFileCodec) that carries it.
 *   - decode(encodedHex)     strict decode; throws on malformed, invalid, or
 *                             non-round-tripping input.
 *   - calculateRoot(value)   domain-separated CanonicalHash over the same
 *                             canonical bytes encode() produces.
 *   - validateRoot(value, r) recomputes calculateRoot(value) and compares.
 *
 * This is the sole replacement for the old regex/substring-based
 * ProtocolExecutionStateParser: no domain reconstruction anywhere in this
 * system scans free text for field boundaries.
 */
class SupplyDomainCodec {
public:
  static std::string encode(const utils::Amount &supply);
  static utils::Amount decode(const std::string &encodedHex);
  static std::string calculateRoot(const utils::Amount &supply);
  static bool validateRoot(const utils::Amount &supply,
                           const std::string &expectedRoot);
};

class BurnsDomainCodec {
public:
  static std::string encode(const std::vector<economics::BurnRecord> &burns);
  static std::vector<economics::BurnRecord>
  decode(const std::string &encodedHex);
  static std::string
  calculateRoot(const std::vector<economics::BurnRecord> &burns);
  static bool validateRoot(const std::vector<economics::BurnRecord> &burns,
                           const std::string &expectedRoot);
};

class StakingDomainCodec {
public:
  static std::string encode(const StakingRegistry &staking);
  static StakingRegistry decode(const std::string &encodedHex);
  static std::string calculateRoot(const StakingRegistry &staking);
  static bool validateRoot(const StakingRegistry &staking,
                           const std::string &expectedRoot);
};

class GovernanceDomainCodec {
public:
  static std::string encode(const GovernanceExecutor &governance);
  static GovernanceExecutor decode(const std::string &encodedHex);
  static std::string calculateRoot(const GovernanceExecutor &governance);
  static bool validateRoot(const GovernanceExecutor &governance,
                           const std::string &expectedRoot);
};

class ValidatorsDomainCodec {
public:
  static std::string encode(const core::ValidatorRegistry &validators);
  static core::ValidatorRegistry decode(const std::string &encodedHex);
  static std::string calculateRoot(const core::ValidatorRegistry &validators);
  static bool validateRoot(const core::ValidatorRegistry &validators,
                           const std::string &expectedRoot);
};

class SlashingDomainCodec {
public:
  static std::string encode(const consensus::ValidatorPenaltyLedger &ledger);
  static consensus::ValidatorPenaltyLedger
  decode(const std::string &encodedHex);
  static std::string
  calculateRoot(const consensus::ValidatorPenaltyLedger &ledger);
  static bool validateRoot(const consensus::ValidatorPenaltyLedger &ledger,
                           const std::string &expectedRoot);
};

/*
 * validator_weights has no independent payload: it is the existing
 * SMT-based commitment over validator consensus weights
 * (StateRootCalculator::calculateValidatorStateRoot), fully derived from the
 * `validators` domain. Root-only, so decodeState can cross-validate it
 * against the just-decoded validators registry.
 */
class ValidatorWeightsDomainCodec {
public:
  static std::string calculateRoot(const core::ValidatorRegistry &validators);
  static bool validateRoot(const core::ValidatorRegistry &validators,
                           const std::string &expectedRoot);
};

/*
 * Orchestrates all seven domains as one system, matching the map shape
 * ProtocolTransactionDomainExecutor and ProtocolExecutionStateParser already
 * plumb everywhere else in the codebase.
 */
class ProtocolDomainCodec {
public:
  static std::map<std::string, std::string>
  encodeState(const ProtocolExecutionState &state);

  // Throws std::runtime_error if any domain fails to decode, or if the
  // decoded `validators` payload does not match the shipped
  // `validator_weights` root.
  static ProtocolExecutionState
  decodeState(const std::map<std::string, std::string> &domains);
};

} // namespace nodo::node

#endif
