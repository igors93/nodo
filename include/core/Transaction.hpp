#ifndef NODO_CORE_TRANSACTION_HPP
#define NODO_CORE_TRANSACTION_HPP

#include "core/TransactionType.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SignatureProvider.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

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

    /*
     * Explicit-input constructor.
     *
     * Security principle:
     * When input CoinLot ids are declared by the transaction, validators must
     * spend only those declared lots. They must not silently replace them with
     * other available lots owned by the same account.
     */
    Transaction(
        TransactionType type,
        std::string fromAddress,
        std::string toAddress,
        utils::Amount amount,
        utils::Amount fee,
        std::uint64_t nonce,
        std::int64_t timestamp,
        std::vector<std::string> inputCoinLotIds
    );

    const std::string& id() const;
    TransactionType type() const;
    const std::string& fromAddress() const;
    const std::string& toAddress() const;
    utils::Amount amount() const;
    utils::Amount fee() const;
    std::uint64_t nonce() const;
    std::int64_t timestamp() const;

    const std::vector<std::string>& inputCoinLotIds() const;
    bool hasExplicitInputCoinLotIds() const;

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

    const std::string& chainId() const;

    /*
     * Binds a chain identifier to this transaction and recomputes the id.
     *
     * Security rule:
     * All production transactions MUST call withChainId() before signing.
     * Without a chain id the signing payload is identical across all network
     * instances, enabling cross-chain replay attacks.
     */
    Transaction& withChainId(std::string chainId);

    /*
     * Full deterministic serialization.
     *
     * This includes the transaction id and signatures.
     */
    std::string serialize() const;

    /* Validates structure and cryptographic policy. */
    bool isStructurallyValid(
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context
    ) const;

    static std::string computeTransactionIdFromPayload(
        const std::string& signingPayload
    );

    /* Rebuilds the complete, canonical transaction including signatures. */
    static Transaction deserialize(const std::string& serialized);

    /*
     * Verifies the authorization that permits this transaction to mutate
     * protocol state: chain binding, sender/key binding, signature policy and
     * the mathematical signature itself.
     */
    bool verifyAuthorization(
        const std::string& expectedChainId,
        const crypto::CryptoPolicy& policy,
        crypto::SecurityContext context,
        const crypto::SignatureProvider& provider
    ) const;

private:
    std::string m_id;
    TransactionType m_type;
    std::string m_fromAddress;
    std::string m_toAddress;
    utils::Amount m_amount;
    utils::Amount m_fee;
    std::uint64_t m_nonce;
    std::int64_t m_timestamp;
    std::vector<std::string> m_inputCoinLotIds;
    std::string m_chainId;
    crypto::SignatureBundle m_signatureBundle;
    bool m_hasSignatureBundle;
};

} // namespace nodo::core

#endif
