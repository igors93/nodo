#include "core/TransactionReceipt.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::TransactionReceipt;
using nodo::core::TransactionReceiptStatus;
using nodo::core::TransactionType;
using nodo::utils::Amount;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testAppliedReceiptIsDeterministicAndValid() {
    const TransactionReceipt receipt =
        TransactionReceipt::applied(
            std::string(64, 'a'),
            TransactionType::TRANSFER,
            "sender-1",
            "recipient-1",
            Amount::fromRawUnits(100),
            Amount::fromRawUnits(5),
            7,
            8,
            std::string(64, 'b'),
            {"accounts"}
        );

    const TransactionReceipt sameReceipt =
        TransactionReceipt::applied(
            std::string(64, 'a'),
            TransactionType::TRANSFER,
            "sender-1",
            "recipient-1",
            Amount::fromRawUnits(100),
            Amount::fromRawUnits(5),
            7,
            8,
            std::string(64, 'b'),
            {"accounts"}
        );

    requireCondition(
        receipt.isValid() &&
        receipt.applied() &&
        receipt.status() == TransactionReceiptStatus::APPLIED &&
        receipt.transactionType() == TransactionType::TRANSFER &&
        receipt.touchedDomains() == std::vector<std::string>{"accounts"} &&
        receipt.receiptHash() == sameReceipt.receiptHash() &&
        !receipt.receiptHash().empty(),
        "Applied transaction receipt should be valid and hash deterministically."
    );
}

void testAppliedReceiptRejectsNonceDiscontinuity() {
    const TransactionReceipt receipt =
        TransactionReceipt::applied(
            std::string(64, 'a'),
            TransactionType::TRANSFER,
            "sender-1",
            "recipient-1",
            Amount::fromRawUnits(100),
            Amount::fromRawUnits(5),
            7,
            9,
            std::string(64, 'b'),
            {"accounts"}
        );

    requireCondition(
        !receipt.isValid(),
        "Applied transaction receipt should reject non-contiguous sender nonce."
    );
}

void testRejectedReceiptCarriesReasonWithoutAdvancingNonce() {
    const TransactionReceipt receipt =
        TransactionReceipt::rejected(
            std::string(64, 'a'),
            TransactionType::TRANSFER,
            "sender-1",
            "recipient-1",
            Amount::fromRawUnits(100),
            Amount::fromRawUnits(5),
            7,
            std::string(64, 'b'),
            {"accounts"},
            "insufficient balance"
        );

    requireCondition(
        receipt.isValid() &&
        !receipt.applied() &&
        receipt.senderNonceBefore() == 7 &&
        receipt.senderNonceAfter() == 7 &&
        !receipt.reason().empty(),
        "Rejected transaction receipt should keep nonce stable and preserve reason."
    );
}

} // namespace

int main() {
    try {
        testAppliedReceiptIsDeterministicAndValid();
        testAppliedReceiptRejectsNonceDiscontinuity();
        testRejectedReceiptCarriesReasonWithoutAdvancingNonce();

        std::cout << "Nodo transaction receipt tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo transaction receipt tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
