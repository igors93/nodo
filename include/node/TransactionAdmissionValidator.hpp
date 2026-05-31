#ifndef NODO_NODE_TRANSACTION_ADMISSION_VALIDATOR_HPP
#define NODO_NODE_TRANSACTION_ADMISSION_VALIDATOR_HPP

#include "config/NetworkParameters.hpp"
#include "core/AccountStateView.hpp"
#include "core/Transaction.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyStore.hpp"
#include "crypto/SignatureProvider.hpp"
#include "mempool/Mempool.hpp"

#include <string>

namespace nodo::node {

enum class TransactionAdmissionStatus {
    ACCEPTED,
    INVALID_NETWORK,
    INVALID_KEY,
    INVALID_TRANSACTION,
    BELOW_MINIMUM_FEE,
    DUPLICATE_TRANSACTION,
    CONFLICTING_NONCE,
    OLD_NONCE,
    FUTURE_NONCE,
    INVALID_SIGNATURE
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
 * and verify through the configured signature provider.
 */
class TransactionAdmissionValidator {
public:
    static TransactionAdmissionResult validateLocalSubmission(
        const core::Transaction& transaction,
        const crypto::StoredKeyMetadata& signingKey,
        const config::NetworkParameters& networkParameters,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        const crypto::SignatureProvider& provider
    );

    static TransactionAdmissionResult validateRuntimeSubmission(
        const core::Transaction& transaction,
        const crypto::StoredKeyMetadata& signingKey,
        const config::NetworkParameters& networkParameters,
        const core::AccountStateView& accountStateView,
        const mempool::Mempool& mempool,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        const crypto::SignatureProvider& provider
    );
};

} // namespace nodo::node

#endif
