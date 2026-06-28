#include "node/TransactionAdmissionValidator.hpp"

#include "utils/Amount.hpp"

#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

bool samePublicKey(
    const crypto::PublicKey& left,
    const crypto::PublicKey& right
) {
    return left.algorithm() == right.algorithm() &&
           left.keyMaterial() == right.keyMaterial();
}

bool allSignaturesUseSigningKey(
    const core::Transaction& transaction,
    const crypto::StoredKeyMetadata& signingKey
) {
    if (!transaction.hasSignatureBundle()) {
        return false;
    }

    for (const crypto::Signature& signature : transaction.signatureBundle().signatures()) {
        if (!samePublicKey(
                signature.publicKey(),
                signingKey.publicKey()
            )) {
            return false;
        }
    }

    return true;
}

TransactionAdmissionResult rejectIfSenderCannotPay(
    const core::Transaction& transaction,
    const core::AccountState& sender
) {
    try {
        const utils::Amount required =
            transaction.amount() + transaction.fee();

        if (sender.balance() < required) {
            return TransactionAdmissionResult::rejected(
                TransactionAdmissionStatus::INSUFFICIENT_BALANCE,
                "Transaction sender balance is insufficient for amount plus fee."
            );
        }
    } catch (const std::exception& error) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_TRANSACTION,
            std::string("Transaction amount plus fee is invalid: ") + error.what()
        );
    }

    return TransactionAdmissionResult::acceptedResult();
}

utils::Amount transactionDebit(
    const core::Transaction& transaction
) {
    return transaction.amount() + transaction.fee();
}

TransactionAdmissionResult validateTransactionAgainstRuntimeState(
    const core::Transaction& transaction,
    const core::AccountStateView& accountStateView,
    const mempool::Mempool& mempool
) {
    if (!accountStateView.isValid()) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_TRANSACTION,
            "Current account state is invalid."
        );
    }

    if (mempool.contains(transaction.id())) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::DUPLICATE_TRANSACTION,
            "Transaction already exists in mempool."
        );
    }

    if (!accountStateView.hasAccount(transaction.fromAddress())) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_TRANSACTION,
            "Transaction sender account does not exist in current state."
        );
    }

    const core::AccountState sender =
        accountStateView.accountOrDefault(transaction.fromAddress());

    if (sender.nonce() == std::numeric_limits<std::uint64_t>::max()) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::OLD_NONCE,
            "Current sender nonce cannot advance without overflow."
        );
    }

    if (transaction.nonce() <= sender.nonce()) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::OLD_NONCE,
            "Transaction nonce is older than current account state."
        );
    }

    utils::Amount reservedByPendingSenderTransactions =
        utils::Amount::fromRawUnits(0);

    const std::vector<core::Transaction> pendingTransactions =
        mempool.transactionsForBlock(
            mempool.size()
        );

    for (const core::Transaction& pending : pendingTransactions) {
        if (pending.fromAddress() != transaction.fromAddress()) {
            continue;
        }

        if (pending.nonce() == transaction.nonce()) {
            if (!mempool.replaceByHigherFee()) {
                return TransactionAdmissionResult::rejected(
                    TransactionAdmissionStatus::CONFLICTING_NONCE,
                    "A transaction with the same sender nonce is already pending."
                );
            }

            if (transaction.fee().rawUnits() <= pending.fee().rawUnits()) {
                return TransactionAdmissionResult::rejected(
                    TransactionAdmissionStatus::CONFLICTING_NONCE,
                    "Replacement transaction must pay a strictly higher fee."
                );
            }

            continue;
        }

        if (pending.nonce() <= sender.nonce()) {
            return TransactionAdmissionResult::rejected(
                TransactionAdmissionStatus::OLD_NONCE,
                "A pending sender transaction has already-expired nonce."
            );
        }

        try {
            reservedByPendingSenderTransactions =
                reservedByPendingSenderTransactions +
                transactionDebit(pending);
        } catch (const std::exception& error) {
            return TransactionAdmissionResult::rejected(
                TransactionAdmissionStatus::INVALID_TRANSACTION,
                std::string("Pending sender debit is invalid: ") + error.what()
            );
        }
    }

    const TransactionAdmissionResult balance =
        rejectIfSenderCannotPay(
            transaction,
            sender
        );

    if (!balance.accepted()) {
        return balance;
    }

    try {
        const utils::Amount totalReserved =
            reservedByPendingSenderTransactions +
            transactionDebit(transaction);

        if (sender.balance() < totalReserved) {
            return TransactionAdmissionResult::rejected(
                TransactionAdmissionStatus::INSUFFICIENT_BALANCE,
                "Sender balance cannot cover all queued pending transactions."
            );
        }
    } catch (const std::exception& error) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_TRANSACTION,
            std::string("Queued sender debit is invalid: ") + error.what()
        );
    }

    return TransactionAdmissionResult::acceptedResult();
}

} // namespace

std::string transactionAdmissionStatusToString(
    TransactionAdmissionStatus status
) {
    switch (status) {
        case TransactionAdmissionStatus::ACCEPTED:
            return "ACCEPTED";
        case TransactionAdmissionStatus::INVALID_NETWORK:
            return "INVALID_NETWORK";
        case TransactionAdmissionStatus::INVALID_KEY:
            return "INVALID_KEY";
        case TransactionAdmissionStatus::INVALID_TRANSACTION:
            return "INVALID_TRANSACTION";
        case TransactionAdmissionStatus::BELOW_MINIMUM_FEE:
            return "BELOW_MINIMUM_FEE";
        case TransactionAdmissionStatus::INSUFFICIENT_BALANCE:
            return "INSUFFICIENT_BALANCE";
        case TransactionAdmissionStatus::DUPLICATE_TRANSACTION:
            return "DUPLICATE_TRANSACTION";
        case TransactionAdmissionStatus::CONFLICTING_NONCE:
            return "CONFLICTING_NONCE";
        case TransactionAdmissionStatus::OLD_NONCE:
            return "OLD_NONCE";
        case TransactionAdmissionStatus::FUTURE_NONCE:
            return "FUTURE_NONCE";
        case TransactionAdmissionStatus::INVALID_SIGNATURE:
            return "INVALID_SIGNATURE";
        default:
            return "INVALID_TRANSACTION";
    }
}

TransactionAdmissionResult::TransactionAdmissionResult()
    : m_status(TransactionAdmissionStatus::INVALID_TRANSACTION),
      m_reason("Uninitialized transaction admission result.") {}

TransactionAdmissionResult TransactionAdmissionResult::acceptedResult() {
    TransactionAdmissionResult result;
    result.m_status = TransactionAdmissionStatus::ACCEPTED;
    result.m_reason = "";
    return result;
}

TransactionAdmissionResult TransactionAdmissionResult::rejected(
    TransactionAdmissionStatus status,
    std::string reason
) {
    TransactionAdmissionResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

TransactionAdmissionStatus TransactionAdmissionResult::status() const {
    return m_status;
}

const std::string& TransactionAdmissionResult::reason() const {
    return m_reason;
}

bool TransactionAdmissionResult::accepted() const {
    return m_status == TransactionAdmissionStatus::ACCEPTED;
}

std::string TransactionAdmissionResult::serialize() const {
    std::ostringstream oss;

    oss << "TransactionAdmissionResult{"
        << "status=" << transactionAdmissionStatusToString(m_status)
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

TransactionAdmissionResult TransactionAdmissionValidator::validateLocalSubmission(
    const core::Transaction& transaction,
    const crypto::StoredKeyMetadata& signingKey,
    const config::NetworkParameters& networkParameters,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    const crypto::SignatureProvider& provider,
    std::optional<std::uint64_t> effectiveMinimumFeeRawUnits
) {
    if (!networkParameters.isValid()) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_NETWORK,
            "Network parameters are invalid."
        );
    }

    if (!signingKey.isValid() ||
        !signingKey.isLocalnetOnly() ||
        signingKey.keyType() != crypto::KeyStoreKeyType::USER) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_KEY,
            "Signing key metadata is invalid, not localnet-only, or not a user key."
        );
    }

    if (provider.algorithm() != signingKey.algorithm()) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_KEY,
            "Signature provider does not match signing key algorithm."
        );
    }

    if (transaction.fromAddress() != signingKey.address()) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_KEY,
            "Transaction sender does not match signing key address."
        );
    }

    if (!transaction.isStructurallyValid(
            policy,
            context
        )) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_TRANSACTION,
            "Transaction structure is invalid."
        );
    }

    if (transaction.type() != core::TransactionType::TRANSFER &&
        transaction.type() != core::TransactionType::GOVERNANCE_PROPOSE) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_TRANSACTION,
            "Transaction type has no implemented authoritative state transition."
        );
    }

    if (!allSignaturesUseSigningKey(
            transaction,
            signingKey
        )) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_SIGNATURE,
            "Transaction signature does not use the expected local signing key."
        );
    }

    const std::uint64_t minimumFee = effectiveMinimumFeeRawUnits.value_or(
        networkParameters.minimumFeeRawUnits()
    );

    if (minimumFee > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_NETWORK,
            "Network minimum fee exceeds supported Amount range."
        );
    }

    if (transaction.fee().rawUnits() < static_cast<std::int64_t>(minimumFee)) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::BELOW_MINIMUM_FEE,
            "Transaction fee is below the network minimum fee."
        );
    }

    if (!transaction.verifyAuthorization(
            networkParameters.chainId(), policy, context, provider
        )) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_SIGNATURE,
            "Transaction chain binding, sender binding, or signature verification failed."
        );
    }

    return TransactionAdmissionResult::acceptedResult();
}

TransactionAdmissionResult TransactionAdmissionValidator::validateRuntimeSubmission(
    const core::Transaction& transaction,
    const crypto::StoredKeyMetadata& signingKey,
    const config::NetworkParameters& networkParameters,
    const core::AccountStateView& accountStateView,
    const mempool::Mempool& mempool,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    const crypto::SignatureProvider& provider,
    std::optional<std::uint64_t> effectiveMinimumFeeRawUnits
) {
    const TransactionAdmissionResult local =
        validateLocalSubmission(
            transaction,
            signingKey,
            networkParameters,
            policy,
            context,
            provider,
            effectiveMinimumFeeRawUnits
        );

    if (!local.accepted()) {
        return local;
    }

    return validateTransactionAgainstRuntimeState(
        transaction,
        accountStateView,
        mempool
    );
}

TransactionAdmissionResult TransactionAdmissionValidator::validateNetworkSubmission(
    const core::Transaction& transaction,
    const config::NetworkParameters& networkParameters,
    const core::AccountStateView& accountStateView,
    const mempool::Mempool& mempool,
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context,
    const crypto::SignatureProvider& provider,
    std::optional<std::uint64_t> effectiveMinimumFeeRawUnits
) {
    if (!networkParameters.isValid()) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_NETWORK,
            "Network parameters are invalid."
        );
    }

    if (!transaction.isStructurallyValid(
            policy,
            context
        )) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_TRANSACTION,
            "Transaction structure is invalid."
        );
    }

    if (transaction.type() != core::TransactionType::TRANSFER &&
        transaction.type() != core::TransactionType::GOVERNANCE_PROPOSE) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_TRANSACTION,
            "Transaction type has no implemented authoritative state transition."
        );
    }

    const std::uint64_t minimumFee = effectiveMinimumFeeRawUnits.value_or(
        networkParameters.minimumFeeRawUnits()
    );

    if (minimumFee > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_NETWORK,
            "Network minimum fee exceeds supported Amount range."
        );
    }

    if (transaction.fee().rawUnits() < static_cast<std::int64_t>(minimumFee)) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::BELOW_MINIMUM_FEE,
            "Transaction fee is below the network minimum fee."
        );
    }

    if (!transaction.verifyAuthorization(
            networkParameters.chainId(), policy, context, provider
        )) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_SIGNATURE,
            "Transaction chain binding, sender binding, or signature verification failed."
        );
    }

    return validateTransactionAgainstRuntimeState(
        transaction,
        accountStateView,
        mempool
    );
}

} // namespace nodo::node
