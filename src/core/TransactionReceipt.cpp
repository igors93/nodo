#include "core/TransactionReceipt.hpp"

#include "crypto/hash.h"

#include <limits>
#include <sstream>
#include <utility>

namespace nodo::core {

namespace {

bool isSafeScalar(
    const std::string& value,
    std::size_t maxSize
) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.' ||
            character == ':' ||
            character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::string hashString(
    const std::string& value
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));
    return std::string(output);
}

} // namespace

std::string transactionReceiptStatusToString(
    TransactionReceiptStatus status
) {
    switch (status) {
        case TransactionReceiptStatus::APPLIED:
            return "APPLIED";
        case TransactionReceiptStatus::REJECTED:
            return "REJECTED";
        default:
            return "REJECTED";
    }
}

TransactionReceipt::TransactionReceipt()
    : m_transactionId(""),
      m_status(TransactionReceiptStatus::REJECTED),
      m_fromAddress(""),
      m_toAddress(""),
      m_amount(),
      m_fee(),
      m_senderNonceBefore(0),
      m_senderNonceAfter(0),
      m_stateRootAfter(""),
      m_reason("Uninitialized transaction receipt.") {}

TransactionReceipt::TransactionReceipt(
    std::string transactionId,
    TransactionReceiptStatus status,
    std::string fromAddress,
    std::string toAddress,
    utils::Amount amount,
    utils::Amount fee,
    std::uint64_t senderNonceBefore,
    std::uint64_t senderNonceAfter,
    std::string stateRootAfter,
    std::string reason
) : m_transactionId(std::move(transactionId)),
    m_status(status),
    m_fromAddress(std::move(fromAddress)),
    m_toAddress(std::move(toAddress)),
    m_amount(amount),
    m_fee(fee),
    m_senderNonceBefore(senderNonceBefore),
    m_senderNonceAfter(senderNonceAfter),
    m_stateRootAfter(std::move(stateRootAfter)),
    m_reason(std::move(reason)) {}

TransactionReceipt TransactionReceipt::applied(
    std::string transactionId,
    std::string fromAddress,
    std::string toAddress,
    utils::Amount amount,
    utils::Amount fee,
    std::uint64_t senderNonceBefore,
    std::uint64_t senderNonceAfter,
    std::string stateRootAfter
) {
    return TransactionReceipt(
        std::move(transactionId),
        TransactionReceiptStatus::APPLIED,
        std::move(fromAddress),
        std::move(toAddress),
        amount,
        fee,
        senderNonceBefore,
        senderNonceAfter,
        std::move(stateRootAfter),
        ""
    );
}

TransactionReceipt TransactionReceipt::rejected(
    std::string transactionId,
    std::string fromAddress,
    std::string toAddress,
    utils::Amount amount,
    utils::Amount fee,
    std::uint64_t senderNonceBefore,
    std::string stateRootAfter,
    std::string reason
) {
    return TransactionReceipt(
        std::move(transactionId),
        TransactionReceiptStatus::REJECTED,
        std::move(fromAddress),
        std::move(toAddress),
        amount,
        fee,
        senderNonceBefore,
        senderNonceBefore,
        std::move(stateRootAfter),
        std::move(reason)
    );
}

const std::string& TransactionReceipt::transactionId() const {
    return m_transactionId;
}

TransactionReceiptStatus TransactionReceipt::status() const {
    return m_status;
}

const std::string& TransactionReceipt::fromAddress() const {
    return m_fromAddress;
}

const std::string& TransactionReceipt::toAddress() const {
    return m_toAddress;
}

utils::Amount TransactionReceipt::amount() const {
    return m_amount;
}

utils::Amount TransactionReceipt::fee() const {
    return m_fee;
}

std::uint64_t TransactionReceipt::senderNonceBefore() const {
    return m_senderNonceBefore;
}

std::uint64_t TransactionReceipt::senderNonceAfter() const {
    return m_senderNonceAfter;
}

const std::string& TransactionReceipt::stateRootAfter() const {
    return m_stateRootAfter;
}

const std::string& TransactionReceipt::reason() const {
    return m_reason;
}

bool TransactionReceipt::applied() const {
    return m_status == TransactionReceiptStatus::APPLIED;
}

bool TransactionReceipt::isValid() const {
    if (!isSafeScalar(m_transactionId, 128) ||
        !isSafeScalar(m_fromAddress, 200) ||
        !isSafeScalar(m_toAddress, 200) ||
        !isSafeScalar(m_stateRootAfter, 128) ||
        m_amount.isNegative() ||
        m_fee.isNegative()) {
        return false;
    }

    if (applied()) {
        return m_senderNonceBefore != std::numeric_limits<std::uint64_t>::max() &&
               m_senderNonceAfter == m_senderNonceBefore + 1 &&
               m_reason.empty();
    }

    return m_senderNonceAfter == m_senderNonceBefore &&
           !m_reason.empty() &&
           m_reason.size() <= 240;
}

std::string TransactionReceipt::serialize() const {
    std::ostringstream output;

    output << "TransactionReceipt{"
           << "transactionId=" << m_transactionId
           << ";status=" << transactionReceiptStatusToString(m_status)
           << ";fromAddress=" << m_fromAddress
           << ";toAddress=" << m_toAddress
           << ";amountRaw=" << m_amount.rawUnits()
           << ";feeRaw=" << m_fee.rawUnits()
           << ";senderNonceBefore=" << m_senderNonceBefore
           << ";senderNonceAfter=" << m_senderNonceAfter
           << ";stateRootAfter=" << m_stateRootAfter
           << ";reason=" << m_reason
           << "}";

    return output.str();
}

std::string TransactionReceipt::receiptHash() const {
    return hashString("NODO_TRANSACTION_RECEIPT_V1|" + serialize());
}

} // namespace nodo::core
