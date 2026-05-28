#include "core/Transaction.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

std::string transactionTypeToString(TransactionType type) {
    switch (type) {
        case TransactionType::TRANSFER:
            return "TRANSFER";

        case TransactionType::MINT_REWARD:
            return "MINT_REWARD";

        case TransactionType::LOCK_SECURITY:
            return "LOCK_SECURITY";

        case TransactionType::UNLOCK_SECURITY:
            return "UNLOCK_SECURITY";

        case TransactionType::VALIDATOR_REGISTER:
            return "VALIDATOR_REGISTER";

        case TransactionType::VALIDATOR_VOTE:
            return "VALIDATOR_VOTE";

        case TransactionType::PENALTY:
            return "PENALTY";

        case TransactionType::BURN:
            return "BURN";

        default:
            return "UNKNOWN";
    }
}

bool isMintTransaction(TransactionType type) {
    return type == TransactionType::MINT_REWARD;
}

bool isSecurityLockTransaction(TransactionType type) {
    return type == TransactionType::LOCK_SECURITY ||
           type == TransactionType::UNLOCK_SECURITY;
}

bool requiresUserSignature(TransactionType type) {
    return type == TransactionType::TRANSFER ||
           type == TransactionType::LOCK_SECURITY ||
           type == TransactionType::UNLOCK_SECURITY ||
           type == TransactionType::VALIDATOR_REGISTER ||
           type == TransactionType::VALIDATOR_VOTE ||
           type == TransactionType::BURN;
}

Transaction::Transaction(
    TransactionType type,
    std::string fromAddress,
    std::string toAddress,
    utils::Amount amount,
    utils::Amount fee,
    std::uint64_t nonce,
    std::int64_t timestamp
)
    : m_id(""),
      m_type(type),
      m_fromAddress(std::move(fromAddress)),
      m_toAddress(std::move(toAddress)),
      m_amount(amount),
      m_fee(fee),
      m_nonce(nonce),
      m_timestamp(timestamp),
      m_signatureBundle(),
      m_hasSignatureBundle(false) {
    m_id = computeTransactionIdFromPayload(signingPayload());
}

const std::string& Transaction::id() const {
    return m_id;
}

TransactionType Transaction::type() const {
    return m_type;
}

const std::string& Transaction::fromAddress() const {
    return m_fromAddress;
}

const std::string& Transaction::toAddress() const {
    return m_toAddress;
}

utils::Amount Transaction::amount() const {
    return m_amount;
}

utils::Amount Transaction::fee() const {
    return m_fee;
}

std::uint64_t Transaction::nonce() const {
    return m_nonce;
}

std::int64_t Transaction::timestamp() const {
    return m_timestamp;
}

bool Transaction::hasSignatureBundle() const {
    return m_hasSignatureBundle;
}

const crypto::SignatureBundle& Transaction::signatureBundle() const {
    if (!m_hasSignatureBundle) {
        throw std::logic_error("Transaction has no SignatureBundle attached.");
    }

    return m_signatureBundle;
}

void Transaction::attachSignatureBundle(
    const crypto::SignatureBundle& signatureBundle
) {
    if (signatureBundle.empty()) {
        throw std::invalid_argument("Empty SignatureBundle rejected by Transaction.");
    }

    m_signatureBundle = signatureBundle;
    m_hasSignatureBundle = true;
}

std::string Transaction::signingPayload() const {
    std::ostringstream oss;

    /*
     * Deterministic ordering is critical.
     *
     * If two nodes serialize the same transaction differently,
     * signatures and transaction IDs will not match.
     */
    oss << "TransactionPayload{"
        << "type=" << transactionTypeToString(m_type)
        << ";from=" << m_fromAddress
        << ";to=" << m_toAddress
        << ";amountRaw=" << m_amount.rawUnits()
        << ";feeRaw=" << m_fee.rawUnits()
        << ";nonce=" << m_nonce
        << ";timestamp=" << m_timestamp
        << "}";

    return oss.str();
}

std::string Transaction::serialize() const {
    std::ostringstream oss;

    oss << "Transaction{"
        << "id=" << m_id
        << ";payload=" << signingPayload();

    if (m_hasSignatureBundle) {
        oss << ";signatureBundle=" << m_signatureBundle.serialize();
    } else {
        oss << ";signatureBundle=NONE";
    }

    oss << "}";

    return oss.str();
}

bool Transaction::isStructurallyValid(
    const crypto::CryptoPolicy& policy,
    crypto::SecurityContext context
) const {
    if (m_id.empty()) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    if (!m_amount.isPositive()) {
        return false;
    }

    /*
     * The transaction id must always match the current signing payload.
     * This prevents accidental mutation after id generation.
     */
    if (m_id != computeTransactionIdFromPayload(signingPayload())) {
        return false;
    }

    if (m_type == TransactionType::TRANSFER) {
        if (m_fromAddress.empty() || m_toAddress.empty()) {
            return false;
        }

        if (m_fromAddress == m_toAddress) {
            return false;
        }
    }

    if (requiresUserSignature(m_type)) {
        if (!m_hasSignatureBundle) {
            return false;
        }

        if (!m_signatureBundle.isValidForPolicy(policy, context)) {
            return false;
        }
    }

    return true;
}

std::string Transaction::computeTransactionIdFromPayload(
    const std::string& signingPayload
) {
    char output[65] = {0};
    nodo_hash_string(signingPayload.c_str(), output, sizeof(output));

    return std::string(output);
}

} // namespace nodo::core