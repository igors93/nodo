#ifndef NODO_CORE_TRANSACTION_RECEIPT_HPP
#define NODO_CORE_TRANSACTION_RECEIPT_HPP

#include "utils/Amount.hpp"
#include "core/TransactionType.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

enum class TransactionReceiptStatus {
    APPLIED,
    REJECTED
};

std::string transactionReceiptStatusToString(
    TransactionReceiptStatus status
);

class TransactionReceipt {
public:
    TransactionReceipt();

    TransactionReceipt(
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
    );

    static TransactionReceipt applied(
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
    );

    static TransactionReceipt rejected(
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
    );

    const std::string& transactionId() const;
    TransactionType transactionType() const;
    TransactionReceiptStatus status() const;
    const std::string& fromAddress() const;
    const std::string& toAddress() const;
    utils::Amount amount() const;
    utils::Amount fee() const;
    std::uint64_t senderNonceBefore() const;
    std::uint64_t senderNonceAfter() const;
    const std::string& stateRootAfter() const;
    const std::vector<std::string>& touchedDomains() const;
    const std::string& reason() const;

    bool applied() const;
    bool isValid() const;
    std::string serialize() const;
    std::string receiptHash() const;

private:
    std::string m_transactionId;
    TransactionType m_transactionType;
    TransactionReceiptStatus m_status;
    std::string m_fromAddress;
    std::string m_toAddress;
    utils::Amount m_amount;
    utils::Amount m_fee;
    std::uint64_t m_senderNonceBefore;
    std::uint64_t m_senderNonceAfter;
    std::string m_stateRootAfter;
    std::vector<std::string> m_touchedDomains;
    std::string m_reason;
};

} // namespace nodo::core

#endif
