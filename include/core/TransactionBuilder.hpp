#ifndef NODO_CORE_TRANSACTION_BUILDER_HPP
#define NODO_CORE_TRANSACTION_BUILDER_HPP

#include "core/Transaction.hpp"
#include "core/TransactionPayload.hpp"
#include "crypto/Signer.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

class TransactionBuildRequest {
public:
  TransactionBuildRequest();

  TransactionBuildRequest(std::string toAddress, utils::Amount amount,
                          utils::Amount fee, std::uint64_t nonce,
                          std::int64_t timestamp);

  const std::string &toAddress() const;
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
  static Transaction buildSignedTransfer(const TransactionBuildRequest &request,
                                         const crypto::Signer &signer,
                                         const std::string &chainId);

  static Transaction
  buildSignedStakeDeposit(const TransactionBuildRequest &request,
                          const crypto::Signer &signer,
                          const std::string &chainId);

  static Transaction
  buildSignedStakeTopUp(const TransactionBuildRequest &request,
                        const crypto::Signer &signer,
                        const std::string &chainId);

  static Transaction
  buildSignedStakeUnlock(const TransactionBuildRequest &request,
                         const crypto::Signer &signer,
                         const std::string &chainId);

  static Transaction
  buildSignedStakeWithdraw(const TransactionBuildRequest &request,
                           const crypto::Signer &signer,
                           const std::string &chainId);

  static Transaction buildSignedBurn(const TransactionBuildRequest &request,
                                     const crypto::Signer &signer,
                                     const std::string &chainId);

  static Transaction
  buildSignedValidatorRegistration(const TransactionBuildRequest &request,
                                   const crypto::PublicKey &validatorPublicKey,
                                   const std::string &metadataHash,
                                   const crypto::Signer &signer,
                                   const std::string &chainId);

  static Transaction
  buildSignedValidatorExitRequest(const TransactionBuildRequest &request,
                                  const crypto::Signer &signer,
                                  const std::string &chainId);

  static Transaction
  buildSignedValidatorUnjailRequest(const TransactionBuildRequest &request,
                                    const crypto::Signer &signer,
                                    const std::string &chainId);

  static Transaction buildSignedGovernanceProposal(
      const std::string &proposalPayload, utils::Amount fee,
      std::uint64_t nonce, std::int64_t timestamp, const crypto::Signer &signer,
      const std::string &chainId);

  static Transaction buildSignedGovernanceVote(
      const std::string &proposalId, const GovernanceVotePayload &vote,
      utils::Amount fee, std::uint64_t nonce, std::int64_t timestamp,
      const crypto::Signer &signer, const std::string &chainId);

  // Triggers execution of an APPROVED treasury-spend proposal once its
  // timelock has elapsed. Permissionless: any funded account may submit it
  // once the proposal is eligible — the vote already authorized the spend,
  // this transaction only asks the network to carry it out.
  static Transaction buildSignedGovernanceExecute(const std::string &proposalId,
                                                  utils::Amount fee,
                                                  std::uint64_t nonce,
                                                  std::int64_t timestamp,
                                                  const crypto::Signer &signer,
                                                  const std::string &chainId);
};

} // namespace nodo::core

#endif
