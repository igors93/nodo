#include "core/TransactionReceipt.hpp"

#include "crypto/hash.h"
#include "core/Transaction.hpp"
#include "core/TransactionTypePolicy.hpp"

#include <algorithm>
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
      m_transactionType(TransactionType::TRANSFER),
      m_status(TransactionReceiptStatus::REJECTED),
      m_fromAddress(""),
      m_toAddress(""),
      m_amount(),
      m_fee(),
      m_senderNonceBefore(0),
      m_senderNonceAfter(0),
      m_stateRootAfter(""),
      m_touchedDomains(),
      m_reason("Uninitialized transaction receipt.") {}

TransactionReceipt::TransactionReceipt(
    std::string transactionId,
    TransactionType transactionType,
    TransactionReceiptStatus status,
    std::string fromAddress,
    std::string toAddress,
    utils::Amount amount,
    utils::Amount fee,
    std::uint64_t senderNonceBefore,
    std::uint64_t senderNonceAfter,
    std::string stateRootAfter,
    std::vector<std::string> touchedDomains,
    std::string reason
) : m_transactionId(std::move(transactionId)),
    m_transactionType(transactionType),
    m_status(status),
    m_fromAddress(std::move(fromAddress)),
    m_toAddress(std::move(toAddress)),
    m_amount(amount),
    m_fee(fee),
    m_senderNonceBefore(senderNonceBefore),
    m_senderNonceAfter(senderNonceAfter),
    m_stateRootAfter(std::move(stateRootAfter)),
    m_touchedDomains(std::move(touchedDomains)),
    m_reason(std::move(reason)) {}

TransactionReceipt TransactionReceipt::applied(
    std::string transactionId,
    TransactionType transactionType,
    std::string fromAddress,
    std::string toAddress,
    utils::Amount amount,
    utils::Amount fee,
    std::uint64_t senderNonceBefore,
    std::uint64_t senderNonceAfter,
    std::string stateRootAfter,
    std::vector<std::string> touchedDomains
) {
    return TransactionReceipt(
        std::move(transactionId),
        transactionType,
        TransactionReceiptStatus::APPLIED,
        std::move(fromAddress),
        std::move(toAddress),
        amount,
        fee,
        senderNonceBefore,
        senderNonceAfter,
        std::move(stateRootAfter),
        std::move(touchedDomains),
        ""
    );
}

TransactionReceipt TransactionReceipt::rejected(
    std::string transactionId,
    TransactionType transactionType,
    std::string fromAddress,
    std::string toAddress,
    utils::Amount amount,
    utils::Amount fee,
    std::uint64_t senderNonceBefore,
    std::string stateRootAfter,
    std::vector<std::string> touchedDomains,
    std::string reason
) {
    return TransactionReceipt(
        std::move(transactionId),
        transactionType,
        TransactionReceiptStatus::REJECTED,
        std::move(fromAddress),
        std::move(toAddress),
        amount,
        fee,
        senderNonceBefore,
        senderNonceBefore,
        std::move(stateRootAfter),
        std::move(touchedDomains),
        std::move(reason)
    );
}

const std::string& TransactionReceipt::transactionId() const {
    return m_transactionId;
}

TransactionType TransactionReceipt::transactionType() const { return m_transactionType; }

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

const std::vector<std::string>& TransactionReceipt::touchedDomains() const {
    return m_touchedDomains;
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
        !TransactionTypePolicyRegistry::hasPolicy(m_transactionType) ||
        m_amount.isNegative() ||
        m_fee.isNegative()) {
        return false;
    }

    if (m_touchedDomains.empty() ||
        !std::is_sorted(m_touchedDomains.begin(), m_touchedDomains.end()) ||
        std::adjacent_find(m_touchedDomains.begin(), m_touchedDomains.end()) != m_touchedDomains.end()) {
        return false;
    }
    for (const auto& domain : m_touchedDomains) if (!isSafeScalar(domain, 64)) return false;

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
           << ";transactionType=" << transactionTypeToString(m_transactionType)
           << ";status=" << transactionReceiptStatusToString(m_status)
           << ";fromAddress=" << m_fromAddress
           << ";toAddress=" << m_toAddress
           << ";amountRaw=" << m_amount.rawUnits()
           << ";feeRaw=" << m_fee.rawUnits()
           << ";senderNonceBefore=" << m_senderNonceBefore
           << ";senderNonceAfter=" << m_senderNonceAfter
           << ";stateRootAfter=" << m_stateRootAfter
           << ";touchedDomains=";
    for (std::size_t i = 0; i < m_touchedDomains.size(); ++i) {
        if (i != 0) output << ',';
        output << m_touchedDomains[i];
    }
    output
           << ";reason=" << m_reason
           << "}";

    return output.str();
}

std::string TransactionReceipt::receiptHash() const {
    return hashString("NODO_TRANSACTION_RECEIPT_V2|" + serialize());
}

} // namespace nodo::core
