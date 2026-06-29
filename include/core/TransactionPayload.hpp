#ifndef NODO_CORE_TRANSACTION_PAYLOAD_HPP
#define NODO_CORE_TRANSACTION_PAYLOAD_HPP

#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

class ValidatorRegistrationPayload {
public:
    ValidatorRegistrationPayload();
    ValidatorRegistrationPayload(crypto::PublicKey validatorPublicKey, std::string metadataHash);

    const crypto::PublicKey& validatorPublicKey() const;
    const std::string& metadataHash() const;
    bool isValid() const;
    std::string serialize() const;
    static ValidatorRegistrationPayload deserialize(const std::string& serialized);

private:
    crypto::PublicKey m_validatorPublicKey;
    std::string m_metadataHash;
};

enum class GovernanceProposalType {
    PARAMETER_CHANGE,
    TREASURY_SPEND,
    TEXT
};

std::string governanceProposalTypeToString(GovernanceProposalType type);
GovernanceProposalType governanceProposalTypeFromString(const std::string& value);

class GovernanceProposalPayload {
public:
    GovernanceProposalPayload();

    GovernanceProposalPayload(
        GovernanceProposalType type,
        std::string title,
        std::string description,
        std::uint64_t votingStartDelayBlocks,
        std::uint64_t votingPeriodBlocks,
        std::uint64_t quorumNumerator,
        std::uint64_t quorumDenominator,
        std::uint64_t approvalNumerator,
        std::uint64_t approvalDenominator,
        std::string parameterTarget,
        std::string parameterValue,
        std::uint64_t parameterEffectiveHeight,
        std::string treasuryRecipient,
        std::int64_t treasuryAmountRaw
    );

    static GovernanceProposalPayload parameterChange(
        std::string title,
        std::string description,
        std::string target,
        std::string value,
        std::uint64_t effectiveHeight,
        std::uint64_t votingStartDelayBlocks = 1,
        std::uint64_t votingPeriodBlocks = 3,
        std::uint64_t quorumNumerator = 1,
        std::uint64_t quorumDenominator = 2,
        std::uint64_t approvalNumerator = 2,
        std::uint64_t approvalDenominator = 3
    );

    static GovernanceProposalPayload treasurySpend(
        std::string title,
        std::string description,
        std::string recipient,
        std::int64_t amountRaw,
        std::uint64_t votingStartDelayBlocks = 1,
        std::uint64_t votingPeriodBlocks = 3,
        std::uint64_t quorumNumerator = 1,
        std::uint64_t quorumDenominator = 2,
        std::uint64_t approvalNumerator = 2,
        std::uint64_t approvalDenominator = 3
    );

    static GovernanceProposalPayload text(
        std::string title,
        std::string description,
        std::uint64_t votingStartDelayBlocks = 1,
        std::uint64_t votingPeriodBlocks = 3,
        std::uint64_t quorumNumerator = 1,
        std::uint64_t quorumDenominator = 2,
        std::uint64_t approvalNumerator = 2,
        std::uint64_t approvalDenominator = 3
    );

    GovernanceProposalType type() const;
    const std::string& title() const;
    const std::string& description() const;
    std::uint64_t votingStartDelayBlocks() const;
    std::uint64_t votingPeriodBlocks() const;
    std::uint64_t quorumNumerator() const;
    std::uint64_t quorumDenominator() const;
    std::uint64_t approvalNumerator() const;
    std::uint64_t approvalDenominator() const;
    const std::string& parameterTarget() const;
    const std::string& parameterValue() const;
    std::uint64_t parameterEffectiveHeight() const;
    const std::string& treasuryRecipient() const;
    std::int64_t treasuryAmountRaw() const;

    bool isValid() const;
    std::string serialize() const;
    static GovernanceProposalPayload deserialize(const std::string& serialized);

private:
    GovernanceProposalType m_type;
    std::string m_title;
    std::string m_description;
    std::uint64_t m_votingStartDelayBlocks;
    std::uint64_t m_votingPeriodBlocks;
    std::uint64_t m_quorumNumerator;
    std::uint64_t m_quorumDenominator;
    std::uint64_t m_approvalNumerator;
    std::uint64_t m_approvalDenominator;
    std::string m_parameterTarget;
    std::string m_parameterValue;
    std::uint64_t m_parameterEffectiveHeight;
    std::string m_treasuryRecipient;
    std::int64_t m_treasuryAmountRaw;
};

enum class GovernanceVoteChoice {
    YES,
    NO,
    ABSTAIN
};

std::string governanceVoteChoiceToString(GovernanceVoteChoice choice);
GovernanceVoteChoice governanceVoteChoiceFromString(const std::string& value);

class GovernanceVotePayload {
public:
    GovernanceVotePayload();
    GovernanceVotePayload(
        std::string proposalId,
        std::string validatorAddress,
        GovernanceVoteChoice choice
    );

    const std::string& proposalId() const;
    const std::string& validatorAddress() const;
    GovernanceVoteChoice choice() const;
    bool isValid() const;
    std::string serialize() const;
    static GovernanceVotePayload deserialize(const std::string& serialized);

private:
    std::string m_proposalId;
    std::string m_validatorAddress;
    GovernanceVoteChoice m_choice;
};

} // namespace nodo::core

#endif
