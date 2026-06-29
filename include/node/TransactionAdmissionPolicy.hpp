#ifndef NODO_NODE_TRANSACTION_ADMISSION_POLICY_HPP
#define NODO_NODE_TRANSACTION_ADMISSION_POLICY_HPP

#include "core/AccountStateView.hpp"
#include "core/Transaction.hpp"
#include "core/ValidatorRegistry.hpp"
#include "mempool/Mempool.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/StakingRegistry.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

class TransactionAdmissionContext {
public:
    TransactionAdmissionContext(
        const core::AccountStateView& accounts,
        const mempool::Mempool& mempool,
        const StakingRegistry& staking,
        const core::ValidatorRegistry& validators,
        const GovernanceExecutor& governance,
        std::uint64_t nextBlockHeight
    );

    const core::AccountStateView& accounts() const;
    const mempool::Mempool& mempool() const;
    const StakingRegistry& staking() const;
    const core::ValidatorRegistry& validators() const;
    const GovernanceExecutor& governance() const;
    std::uint64_t nextBlockHeight() const;

private:
    const core::AccountStateView* m_accounts;
    const mempool::Mempool* m_mempool;
    const StakingRegistry* m_staking;
    const core::ValidatorRegistry* m_validators;
    const GovernanceExecutor* m_governance;
    std::uint64_t m_nextBlockHeight;
};

class TransactionAdmissionPolicy {
public:
    static bool validateTypeAndPayload(
        const core::Transaction& transaction,
        std::string& reason
    );
    static bool validateDomain(
        const core::Transaction& transaction,
        const TransactionAdmissionContext& context,
        std::string& reason
    );
};

} // namespace nodo::node

#endif
