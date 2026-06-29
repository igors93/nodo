#include "core/TransactionBuilder.hpp"
#include "crypto/AddressDerivation.hpp"

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
    const crypto::Signer& signer,
    const std::string& chainId
) {
    if (!request.isValid() || chainId.empty()) {
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
    transaction.withChainId(chainId);

    return signer.signTransaction(
        transaction,
        request.timestamp()
    );
}

Transaction TransactionBuilder::buildSignedStakeDeposit(
    const TransactionBuildRequest& request,
    const crypto::Signer& signer,
    const std::string& chainId
) {
    if (!request.isValid() || chainId.empty()) {
        throw std::invalid_argument("Stake deposit build request is invalid.");
    }

    Transaction tx(
        TransactionType::STAKE_DEPOSIT,
        signer.address(),
        request.toAddress(),
        request.amount(),
        request.fee(),
        request.nonce(),
        request.timestamp()
    );
    tx.withChainId(chainId);
    return signer.signTransaction(tx, request.timestamp());
}

Transaction TransactionBuilder::buildSignedStakeTopUp(
    const TransactionBuildRequest& request,
    const crypto::Signer& signer,
    const std::string& chainId
) {
    if (!request.isValid() || chainId.empty()) {
        throw std::invalid_argument("Stake top-up build request is invalid.");
    }

    Transaction tx(
        TransactionType::STAKE_TOP_UP,
        signer.address(),
        request.toAddress(),
        request.amount(),
        request.fee(),
        request.nonce(),
        request.timestamp()
    );
    tx.withChainId(chainId);
    return signer.signTransaction(tx, request.timestamp());
}

Transaction TransactionBuilder::buildSignedStakeWithdraw(
    const TransactionBuildRequest& request,
    const crypto::Signer& signer,
    const std::string& chainId
) {
    if (!request.isValid() || chainId.empty()) {
        throw std::invalid_argument("Stake withdraw build request is invalid.");
    }

    Transaction tx(
        TransactionType::STAKE_WITHDRAW,
        signer.address(),
        request.toAddress(),
        request.amount(),
        request.fee(),
        request.nonce(),
        request.timestamp()
    );
    tx.withChainId(chainId);
    return signer.signTransaction(tx, request.timestamp());
}

Transaction TransactionBuilder::buildSignedBurn(
    const TransactionBuildRequest& request,
    const crypto::Signer& signer,
    const std::string& chainId
) {
    if (!request.isValid() || chainId.empty()) {
        throw std::invalid_argument("Burn build request is invalid.");
    }
    Transaction tx(
        TransactionType::BURN, signer.address(), "nodo_burn",
        request.amount(), request.fee(), request.nonce(), request.timestamp()
    );
    tx.withChainId(chainId);
    return signer.signTransaction(tx, request.timestamp());
}

Transaction TransactionBuilder::buildSignedValidatorRegistration(
    const TransactionBuildRequest& request,
    const crypto::PublicKey& validatorPublicKey,
    const std::string& metadataHash,
    const crypto::Signer& signer,
    const std::string& chainId
) {
    const ValidatorRegistrationPayload payload(validatorPublicKey, metadataHash);
    if (!request.isValid() || !payload.isValid() || chainId.empty() ||
        request.toAddress() != crypto::AddressDerivation::deriveFromPublicKey(
            validatorPublicKey).value()) {
        throw std::invalid_argument("Validator registration build request is invalid.");
    }
    Transaction tx(
        TransactionType::VALIDATOR_REGISTER,
        signer.address(), request.toAddress(), request.amount(), request.fee(),
        request.nonce(), request.timestamp(), payload.serialize()
    );
    tx.withChainId(chainId);
    return signer.signTransaction(tx, request.timestamp());
}

Transaction TransactionBuilder::buildSignedValidatorExitRequest(
    const TransactionBuildRequest& request,
    const crypto::Signer& signer,
    const std::string& chainId
) {
    if (request.toAddress().empty() || request.fee().isNegative() ||
        request.nonce() == 0 || request.timestamp() <= 0 || chainId.empty()) {
        throw std::invalid_argument("Validator exit request requires a non-empty validator address and non-negative fee.");
    }

    Transaction tx(
        TransactionType::VALIDATOR_EXIT_REQUEST,
        signer.address(),
        request.toAddress(),
        utils::Amount(),
        request.fee(),
        request.nonce(),
        request.timestamp()
    );
    tx.withChainId(chainId);
    return signer.signTransaction(tx, request.timestamp());
}

Transaction TransactionBuilder::buildSignedValidatorUnjailRequest(
    const TransactionBuildRequest& request,
    const crypto::Signer& signer,
    const std::string& chainId
) {
    if (request.toAddress().empty() || request.fee().isNegative() ||
        request.nonce() == 0 || request.timestamp() <= 0 || chainId.empty()) {
        throw std::invalid_argument("Validator unjail request requires a non-empty validator address and non-negative fee.");
    }

    Transaction tx(
        TransactionType::VALIDATOR_UNJAIL_REQUEST,
        signer.address(),
        request.toAddress(),
        utils::Amount(),
        request.fee(),
        request.nonce(),
        request.timestamp()
    );
    tx.withChainId(chainId);
    return signer.signTransaction(tx, request.timestamp());
}

Transaction TransactionBuilder::buildSignedGovernanceProposal(
    const std::string& proposalPayload,
    utils::Amount fee,
    std::uint64_t nonce,
    std::int64_t timestamp,
    const crypto::Signer& signer,
    const std::string& chainId
) {
    if (proposalPayload.empty() || fee.isNegative() || nonce == 0 ||
        timestamp <= 0 || chainId.empty()) {
        throw std::invalid_argument("Governance proposal request is invalid.");
    }
    (void)GovernanceProposalPayload::deserialize(proposalPayload);

    Transaction transaction(
        TransactionType::GOVERNANCE_PROPOSE,
        signer.address(),
        "nodo_governance",
        utils::Amount(),
        fee,
        nonce,
        timestamp,
        proposalPayload
    );
    transaction.withChainId(chainId);
    return signer.signTransaction(transaction, timestamp);
}

Transaction TransactionBuilder::buildSignedGovernanceVote(
    const std::string& proposalId,
    const GovernanceVotePayload& vote,
    utils::Amount fee,
    std::uint64_t nonce,
    std::int64_t timestamp,
    const crypto::Signer& signer,
    const std::string& chainId
) {
    if (proposalId.empty() || !vote.isValid() || fee.isNegative() || nonce == 0 ||
        timestamp <= 0 || chainId.empty() || vote.proposalId() != proposalId) {
        throw std::invalid_argument("Governance vote request is invalid.");
    }
    Transaction tx(
        TransactionType::GOVERNANCE_VOTE, signer.address(), proposalId,
        utils::Amount(), fee, nonce, timestamp, vote.serialize()
    );
    tx.withChainId(chainId);
    return signer.signTransaction(tx, timestamp);
}

} // namespace nodo::core
