#ifndef NODO_CORE_TRANSACTION_HPP
#define NODO_CORE_TRANSACTION_HPP

#include "core/TransactionType.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureBundle.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

/*
 * Transaction represents a signed request to change the Nodo ledger state.
 *
 * Security principle:
 * A transaction should not modify State directly.
 * It must first be validated, then recorded into the ledger.
 */
class Transaction {
public:
    Transaction(
        TransactionType type,
        std::string fromAddress,
        std::string toAddress,
        utils::Amount amount,
        utils::Amount fee,
        std::uint64_t nonce,
        std::int64_t timestamp
    );

    const std::string& id() const;
    TransactionType type() const;
    const std::string& fromAddress() const;
    const std::string& toAddress() const;
    utils::Amount amount() const;
    utils::Amount fee() const;
    std::uint64_t nonce() const;
    std::int64_t timestamp() const;

    bool hasSignatureBundle() const;
    const crypto::SignatureBundle& signatureBundle() const;

    /*
     * Attaches a SignatureBundle after the transaction payload is created.
     *
     * Important:
     * The signature must be created from signingPayload().
     */
    void attachSignatureBundle(const crypto::SignatureBundle& signatureBundle);

    /*
     * Deterministic payload used for transaction signing.
     *
     * Security rule:
     * This payload must not include the signature itself.
     */
    std::string signingPayload() const;

    /*
     * Full deterministic serialization.
     *
     * This includes the transaction id and signatures.
     */
    std::string serialize() const;

    /*
     * Validates structure and cryptographic policy.
     *
     * This does not yet verify real cryptographic signatures.
     * Real verification will be added when crypto providers are implemented.
     */
    bool isStructurallyValid(
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context
    ) const;

    static std::string computeTransactionIdFromPayload(
        const std::string& signingPayload
    );

private:
    std::string m_id;
    TransactionType m_type;
    std::string m_fromAddress;
    std::string m_toAddress;
    utils::Amount m_amount;
    utils::Amount m_fee;
    std::uint64_t m_nonce;
    std::int64_t m_timestamp;
    crypto::SignatureBundle m_signatureBundle;
    bool m_hasSignatureBundle;
};

} // namespace nodo::core

#endif