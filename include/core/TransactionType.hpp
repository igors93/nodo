#ifndef NODO_CORE_TRANSACTION_TYPE_HPP
#define NODO_CORE_TRANSACTION_TYPE_HPP

#include <string>

namespace nodo::core {

/*
 * TransactionType defines every state-changing action planned for Nodo.
 *
 * Important:
 * A transaction type is not only a label. It determines validation rules,
 * required signatures, and how the transaction will affect the ledger.
 */
enum class TransactionType {
    TRANSFER,
    MINT_REWARD,
    LOCK_SECURITY,
    UNLOCK_SECURITY,
    VALIDATOR_REGISTER,
    VALIDATOR_VOTE,
    PENALTY,
    BURN
};

std::string transactionTypeToString(TransactionType type);

bool isMintTransaction(TransactionType type);
bool isSecurityLockTransaction(TransactionType type);
bool requiresUserSignature(TransactionType type);

} // namespace nodo::core

#endif