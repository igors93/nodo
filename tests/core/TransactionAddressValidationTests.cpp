#include "core/Transaction.hpp"
#include "crypto/CryptoPolicy.hpp"

#include <cassert>
#include <stdexcept>

namespace {

using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::utils::Amount;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::SecurityContext;

Transaction makeTransfer(
    const std::string& from,
    const std::string& to
) {
    return Transaction(
        TransactionType::TRANSFER,
        from,
        to,
        Amount::fromRawUnits(100),
        Amount::fromRawUnits(10),
        1,
        1700000000
    );
}

void testSafeAddressPassesStructuralValidation() {
    const Transaction tx = makeTransfer("alice-wallet", "bob-wallet");
    assert(!tx.id().empty());
}

void testToAddressWithSemicolonFailsValidation() {
    const Transaction tx = makeTransfer("alice", "bob;evil");
    assert(!tx.isStructurallyValid(
        CryptoPolicy::developmentPolicy(), SecurityContext::USER_TRANSACTION));
}

void testToAddressWithEqualSignFailsValidation() {
    const Transaction tx = makeTransfer("alice", "to=injected");
    assert(!tx.isStructurallyValid(
        CryptoPolicy::developmentPolicy(), SecurityContext::USER_TRANSACTION));
}

void testFromAddressWithBraceFailsValidation() {
    const Transaction tx = makeTransfer("from{bad}", "alice");
    assert(!tx.isStructurallyValid(
        CryptoPolicy::developmentPolicy(), SecurityContext::USER_TRANSACTION));
}

void testFromEqualsToFails() {
    const Transaction tx = makeTransfer("same-addr", "same-addr");
    assert(!tx.isStructurallyValid(
        CryptoPolicy::developmentPolicy(), SecurityContext::USER_TRANSACTION));
}

void testValidRoundTrip() {
    const Transaction tx = makeTransfer("nodo1abc", "nodo2xyz");
    const std::string serialized = tx.serialize();
    const Transaction restored = Transaction::deserializeForStateReplay(serialized);
    assert(tx.id() == restored.id());
    assert(tx.fromAddress() == restored.fromAddress());
    assert(tx.toAddress() == restored.toAddress());
}

} // namespace

int main() {
    testSafeAddressPassesStructuralValidation();
    testToAddressWithSemicolonFailsValidation();
    testToAddressWithEqualSignFailsValidation();
    testFromAddressWithBraceFailsValidation();
    testFromEqualsToFails();
    testValidRoundTrip();
    return 0;
}
