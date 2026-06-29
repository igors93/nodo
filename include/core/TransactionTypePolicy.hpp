#ifndef NODO_CORE_TRANSACTION_TYPE_POLICY_HPP
#define NODO_CORE_TRANSACTION_TYPE_POLICY_HPP

#include "core/TransactionType.hpp"
#include "utils/Amount.hpp"

#include <string>
#include <vector>

namespace nodo::core {

class Transaction;

enum class TransactionAmountRule {
    ZERO,
    POSITIVE
};

enum class TransactionDebitRule {
    FEE_ONLY,
    AMOUNT_PLUS_FEE
};

enum class TransactionCreditRule {
    NONE,
    TARGET_AMOUNT,
    SENDER_AMOUNT
};

enum class TransactionHandler {
    TRANSFER,
    BURN,
    STAKE_DEPOSIT,
    STAKE_WITHDRAW,
    STAKE_TOP_UP,
    VALIDATOR_REGISTER,
    VALIDATOR_EXIT_REQUEST,
    VALIDATOR_UNJAIL_REQUEST,
    GOVERNANCE_PROPOSE,
    GOVERNANCE_VOTE
};

struct TransactionTypePolicy {
    TransactionType type;
    const char* name;
    TransactionHandler handler;
    TransactionAmountRule amountRule;
    TransactionDebitRule debitRule;
    TransactionCreditRule creditRule;
    bool payloadRequired;
    bool payloadAllowed;
    std::vector<std::string> touchedDomains;
};

class TransactionTypePolicyRegistry {
public:
    static const std::vector<TransactionTypePolicy>& policies();
    static std::vector<TransactionType> allTypes();
    static const TransactionTypePolicy& policyFor(TransactionType type);
    static bool hasPolicy(TransactionType type);
    static bool isMempoolType(TransactionType type);
    static bool validateShape(const Transaction& transaction, std::string& reason);
    static utils::Amount maximumDebit(const Transaction& transaction);
};

} // namespace nodo::core

#endif
