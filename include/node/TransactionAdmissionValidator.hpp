#ifndef NODO_NODE_TRANSACTION_ADMISSION_VALIDATOR_HPP
#define NODO_NODE_TRANSACTION_ADMISSION_VALIDATOR_HPP

#include "config/NetworkParameters.hpp"
#include "core/AccountStateView.hpp"
#include "core/Transaction.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/SignatureProvider.hpp"
#include "mempool/Mempool.hpp"
#include "node/TransactionAdmissionPolicy.hpp"

#include <string>
#include <optional>

namespace nodo::node {

enum class TransactionAdmissionStatus {
    ACCEPTED,
    INVALID_NETWORK,
    INVALID_KEY,
    INVALID_TRANSACTION,
    BELOW_MINIMUM_FEE,
    INSUFFICIENT_BALANCE,
    DUPLICATE_TRANSACTION,
    CONFLICTING_NONCE,
    OLD_NONCE,
    FUTURE_NONCE,
    INVALID_SIGNATURE,
    DOMAIN_REJECTED
};

std::string transactionAdmissionStatusToString(
    TransactionAdmissionStatus status
);

class TransactionAdmissionResult {
public:
    TransactionAdmissionResult();

    static TransactionAdmissionResult acceptedResult();

    static TransactionAdmissionResult rejected(
        TransactionAdmissionStatus status,
        std::string reason
    );

    TransactionAdmissionStatus status() const;
    const std::string& reason() const;
    bool accepted() const;

    std::string serialize() const;

private:
    TransactionAdmissionStatus m_status;
    std::string m_reason;
};

/*
 * TransactionAdmissionValidator is the last local gate before a transaction is
 * persisted into the mempool.
 *
 * It is intentionally stricter than simple structural parsing. A transaction
 * admitted here must match the local signing key, respect the network fee rule,
 * have the expected account nonce, have enough spendable balance, and verify
 * through the configured signature provider.
 */
class TransactionAdmissionValidator {
public:
    static TransactionAdmissionResult validateRuntimeState(
        const core::Transaction& transaction,
        const core::AccountStateView& accountStateView,
        const mempool::Mempool& mempool
    );

    static TransactionAdmissionResult validateLocalSubmission(
        const core::Transaction& transaction,
        const crypto::StoredKeyMetadata& signingKey,
        const config::NetworkParameters& networkParameters,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        const crypto::SignatureProvider& provider,
        std::optional<std::uint64_t> effectiveMinimumFeeRawUnits = std::nullopt,
        const TransactionAdmissionContext* admissionContext = nullptr
    );

    static TransactionAdmissionResult validateRuntimeSubmission(
        const core::Transaction& transaction,
        const crypto::StoredKeyMetadata& signingKey,
        const config::NetworkParameters& networkParameters,
        const core::AccountStateView& accountStateView,
        const mempool::Mempool& mempool,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        const crypto::SignatureProvider& provider,
        std::optional<std::uint64_t> effectiveMinimumFeeRawUnits = std::nullopt,
        const TransactionAdmissionContext* admissionContext = nullptr
    );

    static TransactionAdmissionResult validateNetworkSubmission(
        const core::Transaction& transaction,
        const config::NetworkParameters& networkParameters,
        const core::AccountStateView& accountStateView,
        const mempool::Mempool& mempool,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        const crypto::SignatureProvider& provider,
        std::optional<std::uint64_t> effectiveMinimumFeeRawUnits = std::nullopt,
        const TransactionAdmissionContext* admissionContext = nullptr
    );
};

} // namespace nodo::node

#endif
