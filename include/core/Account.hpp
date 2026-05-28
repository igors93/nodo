#ifndef NODO_CORE_ACCOUNT_HPP
#define NODO_CORE_ACCOUNT_HPP

#include <cstdint>
#include <string>

namespace nodo::core {

/*
 * Account represents replay-protection state for an address.
 *
 * Security principle:
 * A valid account tracks the next expected transaction nonce.
 * This prevents the same sender from applying two different transactions
 * with the same nonce.
 */
class Account {
public:
    static Account create(
        std::string address,
        std::uint64_t createdAtBlock
    );

    Account(
        std::string address,
        std::uint64_t nextNonce,
        std::uint64_t createdAtBlock,
        std::uint64_t lastUpdatedAtBlock
    );

    const std::string& address() const;
    std::uint64_t nextNonce() const;
    std::uint64_t createdAtBlock() const;
    std::uint64_t lastUpdatedAtBlock() const;

    bool isValid() const;

    /*
     * Returns true only when the transaction nonce is exactly the next
     * expected nonce for this account.
     */
    bool canApplyNonce(std::uint64_t nonce) const;

    /*
     * Advances the account nonce after a transaction is accepted.
     *
     * Security rule:
     * This must be called only after the transaction has passed validation
     * and has been applied to the reconstructed State.
     */
    void markTransactionApplied(
        std::uint64_t nonce,
        std::uint64_t blockIndex
    );

    std::string serialize() const;

private:
    std::string m_address;
    std::uint64_t m_nextNonce;
    std::uint64_t m_createdAtBlock;
    std::uint64_t m_lastUpdatedAtBlock;
};

} // namespace nodo::core

#endif