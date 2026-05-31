#include "node/TransactionAdmissionValidator.hpp"

#include <limits>
#include <sstream>
#include <utility>

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
    const crypto::SignatureProvider& provider
) {
    if (!networkParameters.isValid()) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_NETWORK,
            "Network parameters are invalid."
        );
    }

    if (!signingKey.isValid() ||
        !signingKey.isLocalnetOnly()) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_KEY,
            "Signing key metadata is invalid or not localnet-only."
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

    if (!allSignaturesUseSigningKey(
            transaction,
            signingKey
        )) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_SIGNATURE,
            "Transaction signature does not use the expected local signing key."
        );
    }

    const std::uint64_t minimumFee =
        networkParameters.minimumFeeRawUnits();

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

    if (!transaction.signatureBundle().verifyForPolicy(
            transaction.signingPayload(),
            policy,
            context,
            provider
        )) {
        return TransactionAdmissionResult::rejected(
            TransactionAdmissionStatus::INVALID_SIGNATURE,
            "Transaction signature verification failed."
        );
    }

    return TransactionAdmissionResult::acceptedResult();
}

} // namespace nodo::node
