#include "core/Transaction.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

namespace {

std::string extractField(
    const std::string& serialized,
    const std::string& key
) {
    const std::string prefix = key + "=";
    const std::size_t start = serialized.find(prefix);

    if (start == std::string::npos) {
        throw std::invalid_argument("Missing serialized Transaction field: " + key);
    }

    const std::size_t valueStart = start + prefix.size();
    std::size_t valueEnd = serialized.find(';', valueStart);

    if (valueEnd == std::string::npos) {
        valueEnd = serialized.find('}', valueStart);
    }

    if (valueEnd == std::string::npos || valueEnd <= valueStart) {
        throw std::invalid_argument("Invalid serialized Transaction field: " + key);
    }

    return serialized.substr(valueStart, valueEnd - valueStart);
}

std::string extractTransactionPayload(
    const std::string& serialized
) {
    const std::string prefix = "payload=TransactionPayload{";
    const std::string suffix = ";signatureBundle=";

    const std::size_t payloadPrefixStart = serialized.find(prefix);

    if (payloadPrefixStart == std::string::npos) {
        throw std::invalid_argument("Serialized Transaction payload not found.");
    }

    const std::size_t payloadStart = payloadPrefixStart + std::string("payload=").size();
    const std::size_t payloadEnd = serialized.find(suffix, payloadStart);

    if (payloadEnd == std::string::npos || payloadEnd <= payloadStart) {
        throw std::invalid_argument("Serialized Transaction payload is malformed.");
    }

    return serialized.substr(payloadStart, payloadEnd - payloadStart);
}

} // namespace

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

TransactionType transactionTypeFromString(const std::string& value) {
    if (value == "TRANSFER") {
        return TransactionType::TRANSFER;
    }

    if (value == "MINT_REWARD") {
        return TransactionType::MINT_REWARD;
    }

    if (value == "LOCK_SECURITY") {
        return TransactionType::LOCK_SECURITY;
    }

    if (value == "UNLOCK_SECURITY") {
        return TransactionType::UNLOCK_SECURITY;
    }

    if (value == "VALIDATOR_REGISTER") {
        return TransactionType::VALIDATOR_REGISTER;
    }

    if (value == "VALIDATOR_VOTE") {
        return TransactionType::VALIDATOR_VOTE;
    }

    if (value == "PENALTY") {
        return TransactionType::PENALTY;
    }

    if (value == "BURN") {
        return TransactionType::BURN;
    }

    throw std::invalid_argument("Unknown TransactionType: " + value);
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

Transaction Transaction::deserializeForStateReplay(
    const std::string& serialized
) {
    if (serialized.rfind("Transaction{", 0) != 0) {
        throw std::invalid_argument("Serialized data is not a Transaction.");
    }

    const std::string serializedId = extractField(serialized, "id");
    const std::string payload = extractTransactionPayload(serialized);

    if (payload.rfind("TransactionPayload{", 0) != 0) {
        throw std::invalid_argument("Serialized Transaction payload is invalid.");
    }

    Transaction transaction(
        transactionTypeFromString(extractField(payload, "type")),
        extractField(payload, "from"),
        extractField(payload, "to"),
        utils::Amount::fromRawUnits(std::stoll(extractField(payload, "amountRaw"))),
        utils::Amount::fromRawUnits(std::stoll(extractField(payload, "feeRaw"))),
        static_cast<std::uint64_t>(std::stoull(extractField(payload, "nonce"))),
        std::stoll(extractField(payload, "timestamp"))
    );

    if (transaction.id() != serializedId) {
        throw std::invalid_argument("Serialized Transaction id does not match payload.");
    }

    if (transaction.signingPayload() != payload) {
        throw std::invalid_argument("Transaction serialization round-trip failed.");
    }

    return transaction;
}

} // namespace nodo::core