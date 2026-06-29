#ifndef NODO_CORE_TRANSACTION_EXECUTION_ROUTER_HPP
#define NODO_CORE_TRANSACTION_EXECUTION_ROUTER_HPP

#include "core/TransactionExecutionContext.hpp"
#include "core/TransactionExecutionResult.hpp"

namespace nodo::core {

class TransactionExecutionRouter {
public:
    static TransactionExecutionResult execute(
        const Transaction& transaction,
        const TransactionExecutionContext& context
    );
};

} // namespace nodo::core

#endif
