#include "core/TransactionTypePolicy.hpp"

#include "core/Transaction.hpp"

#include <stdexcept>

namespace nodo::core {

const std::vector<TransactionTypePolicy> &
TransactionTypePolicyRegistry::policies() {
  static const std::vector<TransactionTypePolicy> registered = {
      {TransactionType::TRANSFER,
       "TRANSFER",
       TransactionHandler::TRANSFER,
       TransactionAmountRule::POSITIVE,
       TransactionDebitRule::AMOUNT_PLUS_FEE,
       TransactionCreditRule::TARGET_AMOUNT,
       false,
       false,
       {"accounts"}},
      {TransactionType::BURN,
       "BURN",
       TransactionHandler::BURN,
       TransactionAmountRule::POSITIVE,
       TransactionDebitRule::AMOUNT_PLUS_FEE,
       TransactionCreditRule::NONE,
       false,
       false,
       {"accounts", "supply", "burns"}},
      {TransactionType::STAKE_DEPOSIT,
       "STAKE_DEPOSIT",
       TransactionHandler::STAKE_DEPOSIT,
       TransactionAmountRule::POSITIVE,
       TransactionDebitRule::AMOUNT_PLUS_FEE,
       TransactionCreditRule::NONE,
       false,
       false,
       {"accounts", "staking", "validators"}},
      {TransactionType::STAKE_UNLOCK,
       "STAKE_UNLOCK",
       TransactionHandler::STAKE_UNLOCK,
       TransactionAmountRule::POSITIVE,
       TransactionDebitRule::FEE_ONLY,
       TransactionCreditRule::NONE,
       false,
       false,
       {"accounts", "staking", "validators"}},
      {TransactionType::STAKE_WITHDRAW,
       "STAKE_WITHDRAW",
       TransactionHandler::STAKE_WITHDRAW,
       TransactionAmountRule::POSITIVE,
       TransactionDebitRule::FEE_ONLY,
       TransactionCreditRule::SENDER_AMOUNT,
       false,
       false,
       {"accounts", "staking"}},
      {TransactionType::STAKE_TOP_UP,
       "STAKE_TOP_UP",
       TransactionHandler::STAKE_TOP_UP,
       TransactionAmountRule::POSITIVE,
       TransactionDebitRule::AMOUNT_PLUS_FEE,
       TransactionCreditRule::NONE,
       false,
       false,
       {"accounts", "staking", "validators"}},
      {TransactionType::VALIDATOR_REGISTER,
       "VALIDATOR_REGISTER",
       TransactionHandler::VALIDATOR_REGISTER,
       TransactionAmountRule::POSITIVE,
       TransactionDebitRule::AMOUNT_PLUS_FEE,
       TransactionCreditRule::NONE,
       true,
       true,
       {"accounts", "staking", "validators"}},
      {TransactionType::VALIDATOR_EXIT_REQUEST,
       "VALIDATOR_EXIT_REQUEST",
       TransactionHandler::VALIDATOR_EXIT_REQUEST,
       TransactionAmountRule::ZERO,
       TransactionDebitRule::FEE_ONLY,
       TransactionCreditRule::NONE,
       false,
       false,
       {"accounts", "staking", "validators"}},
      {TransactionType::VALIDATOR_UNJAIL_REQUEST,
       "VALIDATOR_UNJAIL_REQUEST",
       TransactionHandler::VALIDATOR_UNJAIL_REQUEST,
       TransactionAmountRule::ZERO,
       TransactionDebitRule::FEE_ONLY,
       TransactionCreditRule::NONE,
       false,
       false,
       {"accounts", "staking", "validators"}},
      {TransactionType::VALIDATOR_KEY_ROTATE,
       "VALIDATOR_KEY_ROTATE",
       TransactionHandler::VALIDATOR_KEY_ROTATE,
       TransactionAmountRule::ZERO,
       TransactionDebitRule::FEE_ONLY,
       TransactionCreditRule::NONE,
       true,
       true,
       {"accounts", "staking", "validators"}},
      {TransactionType::GOVERNANCE_PROPOSE,
       "GOVERNANCE_PROPOSE",
       TransactionHandler::GOVERNANCE_PROPOSE,
       TransactionAmountRule::ZERO,
       TransactionDebitRule::FEE_ONLY,
       TransactionCreditRule::NONE,
       true,
       true,
       {"accounts", "governance"}},
      {TransactionType::GOVERNANCE_VOTE,
       "GOVERNANCE_VOTE",
       TransactionHandler::GOVERNANCE_VOTE,
       TransactionAmountRule::ZERO,
       TransactionDebitRule::FEE_ONLY,
       TransactionCreditRule::NONE,
       true,
       true,
       {"accounts", "governance", "validators", "staking"}},
      {TransactionType::GOVERNANCE_EXECUTE,
       "GOVERNANCE_EXECUTE",
       TransactionHandler::GOVERNANCE_EXECUTE,
       TransactionAmountRule::ZERO,
       TransactionDebitRule::FEE_ONLY,
       TransactionCreditRule::NONE,
       false,
       false,
       {"accounts", "governance"}}};
  return registered;
}

std::vector<TransactionType> TransactionTypePolicyRegistry::allTypes() {
  std::vector<TransactionType> types;
  types.reserve(policies().size());
  for (const auto &policy : policies())
    types.push_back(policy.type);
  return types;
}

const TransactionTypePolicy &
TransactionTypePolicyRegistry::policyFor(TransactionType type) {
  for (const auto &policy : policies()) {
    if (policy.type == type)
      return policy;
  }
  throw std::invalid_argument("TransactionType has no registered policy.");
}

bool TransactionTypePolicyRegistry::hasPolicy(TransactionType type) {
  for (const auto &policy : policies())
    if (policy.type == type)
      return true;
  return false;
}

bool TransactionTypePolicyRegistry::isMempoolType(TransactionType type) {
  return hasPolicy(type);
}

bool TransactionTypePolicyRegistry::validateShape(
    const Transaction &transaction, std::string &reason) {
  if (!hasPolicy(transaction.type())) {
    reason = "Transaction type is not registered.";
    return false;
  }
  const auto &policy = policyFor(transaction.type());
  if (policy.amountRule == TransactionAmountRule::ZERO &&
      !transaction.amount().isZero()) {
    reason = "Transaction amount must be zero for this type.";
    return false;
  }
  if (policy.amountRule == TransactionAmountRule::POSITIVE &&
      !transaction.amount().isPositive()) {
    reason = "Transaction amount must be positive for this type.";
    return false;
  }
  if (transaction.toAddress().empty()) {
    reason = "Transaction target is required.";
    return false;
  }
  if (policy.payloadRequired && transaction.data().empty()) {
    reason = "Transaction payload is required for this type.";
    return false;
  }
  if (!policy.payloadAllowed && !transaction.data().empty()) {
    reason = "Transaction payload is not allowed for this type.";
    return false;
  }
  if (transaction.type() == TransactionType::TRANSFER &&
      transaction.fromAddress() == transaction.toAddress()) {
    reason = "Transfer sender and recipient must differ.";
    return false;
  }
  if (transaction.type() == TransactionType::BURN &&
      transaction.toAddress() != "nodo_burn") {
    reason = "Burn target must be the canonical burn address.";
    return false;
  }
  if (transaction.type() == TransactionType::GOVERNANCE_PROPOSE &&
      transaction.toAddress() != "nodo_governance") {
    reason =
        "Governance proposal target must be the canonical governance address.";
    return false;
  }
  reason.clear();
  return true;
}

utils::Amount
TransactionTypePolicyRegistry::maximumDebit(const Transaction &transaction) {
  const auto &policy = policyFor(transaction.type());
  return policy.debitRule == TransactionDebitRule::AMOUNT_PLUS_FEE
             ? transaction.amount() + transaction.fee()
             : transaction.fee();
}

} // namespace nodo::core
