#ifndef NODO_CORE_TRANSACTION_BUILDER_HPP
#define NODO_CORE_TRANSACTION_BUILDER_HPP

#include "core/Transaction.hpp"
#include "crypto/Signer.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

class TransactionBuildRequest {
public:
    TransactionBuildRequest();

    TransactionBuildRequest(
        std::string toAddress,
        utils::Amount amount,
        utils::Amount fee,
        std::uint64_t nonce,
        std::int64_t timestamp
    );

    const std::string& toAddress() const;
    utils::Amount amount() const;
    utils::Amount fee() const;
    std::uint64_t nonce() const;
    std::int64_t timestamp() const;
    bool isValid() const;

private:
    std::string m_toAddress;
    utils::Amount m_amount;
    utils::Amount m_fee;
    std::uint64_t m_nonce;
    std::int64_t m_timestamp;
};

class TransactionBuilder {
public:
    static Transaction buildSignedTransfer(
        const TransactionBuildRequest& request,
        const crypto::Signer& signer
    );
};

} // namespace nodo::core

#endif
