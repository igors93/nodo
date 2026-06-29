#ifndef NODO_CORE_TRANSACTION_TYPE_HPP
#define NODO_CORE_TRANSACTION_TYPE_HPP

#include <string>

namespace nodo::core {

/*
 * TransactionType defines every user-submittable state-changing action in Nodo.
 *
 * Important:
 * A transaction type is not only a label. It determines validation rules,
 * required signatures, and how the transaction will affect the ledger.
 */
enum class TransactionType {
    TRANSFER,
    BURN,
    STAKE_DEPOSIT,
    STAKE_UNLOCK,
    STAKE_WITHDRAW,
    STAKE_TOP_UP,
    VALIDATOR_REGISTER,
    VALIDATOR_EXIT_REQUEST,
    VALIDATOR_UNJAIL_REQUEST,
    GOVERNANCE_PROPOSE,
    GOVERNANCE_VOTE,
    COUNT
};

std::string transactionTypeToString(TransactionType type);
TransactionType transactionTypeFromString(const std::string& value);

bool requiresUserSignature(TransactionType type);
bool isStakingTransaction(TransactionType type);
bool isValidatorLifecycleTransaction(TransactionType type);
bool isGovernanceTransaction(TransactionType type);

} // namespace nodo::core

#endif
