#include "core/TransactionExecutionRouter.hpp"

#include "core/CoinLotTransactionValidator.hpp"
#include "core/StateRootCalculator.hpp"
#include "core/TransactionTypePolicy.hpp"
#include "crypto/hash.h"

#include <algorithm>
#include <limits>
#include <set>

namespace nodo::core {

namespace {
TransactionDomainExecutionResult
dispatchDomain(const Transaction &tx, const AccountStateView &accounts,
               const TransactionExecutionContext &context,
               TransactionHandler handler) {
  TransactionDomainExecutor *domain = context.domainExecutor();
  if (handler == TransactionHandler::TRANSFER) {
    return TransactionDomainExecutionResult::accepted(
        accounts, domain == nullptr ? std::map<std::string, std::string>{}
                                    : domain->domains());
  }
  if (domain == nullptr) {
    return context.requireDomainExecutor()
               ? TransactionDomainExecutionResult::rejected(
                     "DOMAIN_EXECUTOR_REQUIRED",
                     "Canonical execution context has no domain executor.")
               : TransactionDomainExecutionResult::accepted(accounts, {});
  }
  switch (handler) {
  case TransactionHandler::BURN:
    return domain->applyBurn(tx, accounts, context.blockHeight(),
                             context.blockTimestamp());
  case TransactionHandler::STAKE_DEPOSIT:
    return domain->applyStakeDeposit(tx, accounts, context.blockHeight(),
                                     context.blockTimestamp());
  case TransactionHandler::STAKE_UNLOCK:
    return domain->applyStakeUnlock(tx, accounts, context.blockHeight(),
                                    context.blockTimestamp());
  case TransactionHandler::STAKE_WITHDRAW:
    return domain->applyStakeWithdraw(tx, accounts, context.blockHeight(),
                                      context.blockTimestamp());
  case TransactionHandler::STAKE_TOP_UP:
    return domain->applyStakeTopUp(tx, accounts, context.blockHeight(),
                                   context.blockTimestamp());
  case TransactionHandler::VALIDATOR_REGISTER:
    return domain->applyValidatorRegister(tx, accounts, context.blockHeight(),
                                          context.blockTimestamp());
  case TransactionHandler::VALIDATOR_EXIT_REQUEST:
    return domain->applyValidatorExitRequest(
        tx, accounts, context.blockHeight(), context.blockTimestamp());
  case TransactionHandler::VALIDATOR_UNJAIL_REQUEST:
    return domain->applyValidatorUnjailRequest(
        tx, accounts, context.blockHeight(), context.blockTimestamp());
  case TransactionHandler::GOVERNANCE_PROPOSE:
    return domain->applyGovernanceProposal(tx, accounts, context.blockHeight(),
                                           context.blockTimestamp());
  case TransactionHandler::GOVERNANCE_VOTE:
    return domain->applyGovernanceVote(tx, accounts, context.blockHeight(),
                                       context.blockTimestamp());
  case TransactionHandler::GOVERNANCE_EXECUTE:
    return domain->applyGovernanceExecute(tx, accounts, context.blockHeight(),
                                          context.blockTimestamp());
  case TransactionHandler::TRANSFER:
    break;
  }
  return TransactionDomainExecutionResult::rejected(
      "UNREGISTERED_HANDLER", "Transaction handler is not registered.");
}
} // namespace

TransactionExecutionResult TransactionExecutionRouter::execute(
    const Transaction &transaction,
    const TransactionExecutionContext &context) {
  try {
    std::string shapeReason;
    if (!TransactionTypePolicyRegistry::validateShape(transaction,
                                                      shapeReason)) {
      return TransactionExecutionResult::rejected(
          TransactionExecutionStatus::INVALID_TRANSACTION, "INVALID_SHAPE",
          shapeReason);
    }
    const auto &policy =
        TransactionTypePolicyRegistry::policyFor(transaction.type());
    if (!context.enforceAccountState()) {
      const auto domain = dispatchDomain(transaction, context.accounts(),
                                         context, policy.handler);
      if (!domain.applied()) {
        return TransactionExecutionResult::rejected(
            TransactionExecutionStatus::DOMAIN_REJECTED, domain.code(),
            domain.reason());
      }
      std::vector<std::string> domains = policy.touchedDomains;
      std::sort(domains.begin(), domains.end());
      std::vector<std::string> touched = {transaction.fromAddress()};
      if (policy.creditRule == TransactionCreditRule::TARGET_AMOUNT) {
        touched.push_back(transaction.toAddress());
      }
      return TransactionExecutionResult::applied(
          domain.accounts(), TransactionReceipt(), std::move(touched),
          std::move(domains));
    }

    AccountStateView accounts = context.accounts();
    if (!accounts.hasAccount(transaction.fromAddress()) &&
        !context.allowMissingAccounts()) {
      return TransactionExecutionResult::rejected(
          TransactionExecutionStatus::INVALID_TRANSACTION, "UNKNOWN_SENDER",
          "Sender account does not exist.");
    }
    const AccountState sender =
        accounts.accountOrDefault(transaction.fromAddress());
    if (!sender.isValid() ||
        sender.nonce() == std::numeric_limits<std::uint64_t>::max() ||
        transaction.nonce() != sender.nonce() + 1) {
      return TransactionExecutionResult::rejected(
          TransactionExecutionStatus::INVALID_NONCE, "INVALID_NONCE",
          "Transaction nonce does not match sender state.");
    }
    const utils::Amount debit =
        TransactionTypePolicyRegistry::maximumDebit(transaction);
    if (sender.balance() < debit) {
      return TransactionExecutionResult::rejected(
          TransactionExecutionStatus::INSUFFICIENT_BALANCE,
          "INSUFFICIENT_BALANCE",
          "Sender balance is insufficient for this transaction.");
    }

    utils::Amount senderBalance = sender.balance() - debit;
    if (policy.creditRule == TransactionCreditRule::SENDER_AMOUNT) {
      senderBalance = senderBalance + transaction.amount();
    }
    if (!accounts.putAccount(AccountState(sender.address(), senderBalance,
                                          transaction.nonce()))) {
      return TransactionExecutionResult::rejected(
          TransactionExecutionStatus::STATE_ERROR, "ACCOUNT_UPDATE_FAILED",
          "Sender account update failed.");
    }

    std::set<std::string> touched = {transaction.fromAddress()};
    if (policy.creditRule == TransactionCreditRule::TARGET_AMOUNT) {
      const AccountState recipient =
          accounts.accountOrDefault(transaction.toAddress());
      if (!accounts.putAccount(AccountState(
              transaction.toAddress(),
              recipient.balance() + transaction.amount(), recipient.nonce()))) {
        return TransactionExecutionResult::rejected(
            TransactionExecutionStatus::STATE_ERROR, "RECIPIENT_UPDATE_FAILED",
            "Recipient account update failed.");
      }
      touched.insert(transaction.toAddress());
    }

    if (!context.feeRecipientAddress().empty()) {
      const AccountState feeRecipient =
          accounts.accountOrDefault(context.feeRecipientAddress());
      if (!accounts.putAccount(
              AccountState(context.feeRecipientAddress(),
                           feeRecipient.balance() + transaction.fee(),
                           feeRecipient.nonce()))) {
        return TransactionExecutionResult::rejected(
            TransactionExecutionStatus::STATE_ERROR, "FEE_UPDATE_FAILED",
            "Fee recipient update failed.");
      }
      touched.insert(context.feeRecipientAddress());
    }

    if (policy.handler == TransactionHandler::TRANSFER &&
        context.coinLots() != nullptr) {
      CoinLotRegistry candidate = *context.coinLots();
      CoinLotTransactionValidator::applyTransfer(candidate, transaction,
                                                 context.blockHeight());
      *context.coinLots() = std::move(candidate);
    }

    const auto domain =
        dispatchDomain(transaction, accounts, context, policy.handler);
    if (!domain.applied()) {
      return TransactionExecutionResult::rejected(
          TransactionExecutionStatus::DOMAIN_REJECTED, domain.code(),
          domain.reason());
    }
    accounts = domain.accounts();
    std::vector<std::string> domains = policy.touchedDomains;
    std::map<std::string, std::string> commitmentDomains = domain.domains();
    if (context.coinLots() != nullptr) {
      char digest[NODO_HASH_BUFFER_SIZE] = {0};
      const std::string serialized = context.coinLots()->serialize();
      nodo_hash_string(serialized.c_str(), digest, sizeof(digest));
      commitmentDomains["coin_lots"] = std::string(digest);
      if (policy.handler == TransactionHandler::TRANSFER) {
        domains.push_back("coin_lots");
      }
    }
    std::sort(domains.begin(), domains.end());
    domains.erase(std::unique(domains.begin(), domains.end()), domains.end());

    const std::string root = StateRootCalculator::calculateProtocolStateRoot(
        accounts, commitmentDomains);
    const TransactionReceipt receipt = TransactionReceipt::applied(
        transaction.id(), transaction.type(), transaction.fromAddress(),
        transaction.toAddress(), transaction.amount(), transaction.fee(),
        sender.nonce(), transaction.nonce(), root, domains);
    return TransactionExecutionResult::applied(
        std::move(accounts), receipt,
        std::vector<std::string>(touched.begin(), touched.end()),
        std::move(domains));
  } catch (const std::exception &error) {
    return TransactionExecutionResult::rejected(
        TransactionExecutionStatus::STATE_ERROR, "EXECUTION_EXCEPTION",
        error.what());
  }
}

} // namespace nodo::core
