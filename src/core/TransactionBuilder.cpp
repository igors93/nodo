#include "core/TransactionBuilder.hpp"

#include <stdexcept>
#include <utility>

namespace nodo::core {

TransactionBuildRequest::TransactionBuildRequest()
    : m_toAddress(""),
      m_amount(utils::Amount::fromRawUnits(0)),
      m_fee(utils::Amount::fromRawUnits(0)),
      m_nonce(0),
      m_timestamp(0) {}

TransactionBuildRequest::TransactionBuildRequest(
    std::string toAddress,
    utils::Amount amount,
    utils::Amount fee,
    std::uint64_t nonce,
    std::int64_t timestamp
)
    : m_toAddress(std::move(toAddress)),
      m_amount(amount),
      m_fee(fee),
      m_nonce(nonce),
      m_timestamp(timestamp) {}

const std::string& TransactionBuildRequest::toAddress() const {
    return m_toAddress;
}

utils::Amount TransactionBuildRequest::amount() const {
    return m_amount;
}

utils::Amount TransactionBuildRequest::fee() const {
    return m_fee;
}

std::uint64_t TransactionBuildRequest::nonce() const {
    return m_nonce;
}

std::int64_t TransactionBuildRequest::timestamp() const {
    return m_timestamp;
}

bool TransactionBuildRequest::isValid() const {
    return !m_toAddress.empty() &&
           m_amount.isPositive() &&
           !m_fee.isNegative() &&
           m_nonce > 0 &&
           m_timestamp > 0;
}

Transaction TransactionBuilder::buildSignedTransfer(
    const TransactionBuildRequest& request,
    const crypto::Signer& signer
) {
    if (!request.isValid()) {
        throw std::invalid_argument("Transaction build request is invalid.");
    }

    Transaction transaction(
        TransactionType::TRANSFER,
        signer.address(),
        request.toAddress(),
        request.amount(),
        request.fee(),
        request.nonce(),
        request.timestamp()
    );

    return signer.signTransaction(
        transaction,
        request.timestamp()
    );
}

} // namespace nodo::core
