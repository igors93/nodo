#include "core/TransactionPayload.hpp"

#include "crypto/AddressDerivation.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::core {

namespace {
constexpr const char *REGISTRATION_SCHEMA =
    "NODO_VALIDATOR_REGISTRATION_PAYLOAD_V1";
constexpr const char *KEY_ROTATION_SCHEMA =
    "NODO_VALIDATOR_KEY_ROTATION_PAYLOAD_V1";
constexpr const char *PROPOSAL_SCHEMA = "NODO_GOVERNANCE_PROPOSAL_PAYLOAD_V1";
constexpr const char *VOTE_SCHEMA = "NODO_GOVERNANCE_VOTE_PAYLOAD_V2";
constexpr std::size_t MAX_GOVERNANCE_TITLE_BYTES = 120;
constexpr std::size_t MAX_GOVERNANCE_DESCRIPTION_BYTES = 1024;
constexpr std::size_t MAX_GOVERNANCE_SCALAR_BYTES = 256;
constexpr std::uint64_t MAX_GOVERNANCE_VOTING_DELAY_BLOCKS = 1000000;
constexpr std::uint64_t MAX_GOVERNANCE_VOTING_PERIOD_BLOCKS = 1000000;

bool safeScalar(const std::string &value, std::size_t limit = 256) {
  if (value.empty() || value.size() > limit)
    return false;
  for (char c : value) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                    c == '.' || c == ':';
    if (!ok)
      return false;
  }
  return true;
}

bool safeText(const std::string &value, std::size_t limit) {
  if (value.empty() || value.size() > limit)
    return false;
  for (char c : value) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (uc < 0x20)
      return false;
  }
  return true;
}

bool validRatio(std::uint64_t numerator, std::uint64_t denominator) {
  return denominator > 0 && numerator > 0 && numerator <= denominator;
}
} // namespace

ValidatorRegistrationPayload::ValidatorRegistrationPayload() = default;

ValidatorRegistrationPayload::ValidatorRegistrationPayload(
    crypto::PublicKey validatorPublicKey, std::string metadataHash)
    : m_validatorPublicKey(std::move(validatorPublicKey)),
      m_metadataHash(std::move(metadataHash)) {}

const crypto::PublicKey &
ValidatorRegistrationPayload::validatorPublicKey() const {
  return m_validatorPublicKey;
}
const std::string &ValidatorRegistrationPayload::metadataHash() const {
  return m_metadataHash;
}
bool ValidatorRegistrationPayload::isValid() const {
  return m_validatorPublicKey.isValid() &&
         crypto::isValidatorAlgorithm(m_validatorPublicKey.algorithm()) &&
         safeScalar(m_metadataHash);
}

std::string ValidatorRegistrationPayload::serialize() const {
  if (!isValid())
    throw std::invalid_argument("Invalid validator registration payload.");
  serialization::CanonicalWriter writer;
  writer.writeString(REGISTRATION_SCHEMA);
  writer.writeString(
      crypto::cryptoAlgorithmToString(m_validatorPublicKey.algorithm()));
  writer.writeString(m_validatorPublicKey.keyMaterial());
  writer.writeString(m_metadataHash);
  return writer.byteString();
}

ValidatorRegistrationPayload
ValidatorRegistrationPayload::deserialize(const std::string &serialized) {
  serialization::CanonicalReader reader(serialized, 4096);
  if (reader.readString() != REGISTRATION_SCHEMA) {
    throw std::invalid_argument(
        "Unknown validator registration payload schema.");
  }
  const std::string algorithmStr = reader.readString();
  const std::string keyMaterial = reader.readString();
  const std::string metadataHash = reader.readString();
  ValidatorRegistrationPayload payload(
      crypto::PublicKey(crypto::cryptoAlgorithmFromString(algorithmStr),
                        keyMaterial),
      metadataHash);
  reader.requireFullyConsumed();
  if (!payload.isValid() || payload.serialize() != serialized) {
    throw std::invalid_argument(
        "Non-canonical validator registration payload.");
  }
  return payload;
}

ValidatorKeyRotationPayload::ValidatorKeyRotationPayload()
    : m_oldValidatorAddress(""), m_newValidatorPublicKey(), m_metadataHash(""),
      m_activationEpoch(0), m_reasonHash("") {}

ValidatorKeyRotationPayload::ValidatorKeyRotationPayload(
    std::string oldValidatorAddress, crypto::PublicKey newValidatorPublicKey,
    std::string metadataHash, std::uint64_t activationEpoch,
    std::string reasonHash)
    : m_oldValidatorAddress(std::move(oldValidatorAddress)),
      m_newValidatorPublicKey(std::move(newValidatorPublicKey)),
      m_metadataHash(std::move(metadataHash)),
      m_activationEpoch(activationEpoch), m_reasonHash(std::move(reasonHash)) {}

const std::string &ValidatorKeyRotationPayload::oldValidatorAddress() const {
  return m_oldValidatorAddress;
}

const crypto::PublicKey &
ValidatorKeyRotationPayload::newValidatorPublicKey() const {
  return m_newValidatorPublicKey;
}

const std::string &ValidatorKeyRotationPayload::metadataHash() const {
  return m_metadataHash;
}

std::uint64_t ValidatorKeyRotationPayload::activationEpoch() const {
  return m_activationEpoch;
}

const std::string &ValidatorKeyRotationPayload::reasonHash() const {
  return m_reasonHash;
}

std::string ValidatorKeyRotationPayload::newValidatorAddress() const {
  if (!m_newValidatorPublicKey.isValid()) {
    return "";
  }
  return crypto::AddressDerivation::deriveFromPublicKey(m_newValidatorPublicKey)
      .value();
}

bool ValidatorKeyRotationPayload::isValid() const {
  return safeScalar(m_oldValidatorAddress) &&
         m_newValidatorPublicKey.isValid() &&
         crypto::isValidatorAlgorithm(m_newValidatorPublicKey.algorithm()) &&
         safeScalar(m_metadataHash) && safeScalar(m_reasonHash) &&
         m_activationEpoch > 0 && !newValidatorAddress().empty() &&
         newValidatorAddress() != m_oldValidatorAddress;
}

std::string ValidatorKeyRotationPayload::serialize() const {
  if (!isValid())
    throw std::invalid_argument("Invalid validator key rotation payload.");
  serialization::CanonicalWriter writer;
  writer.writeString(KEY_ROTATION_SCHEMA);
  writer.writeString(m_oldValidatorAddress);
  writer.writeString(
      crypto::cryptoAlgorithmToString(m_newValidatorPublicKey.algorithm()));
  writer.writeString(m_newValidatorPublicKey.keyMaterial());
  writer.writeString(m_metadataHash);
  writer.writeUInt64(m_activationEpoch);
  writer.writeString(m_reasonHash);
  return writer.byteString();
}

ValidatorKeyRotationPayload
ValidatorKeyRotationPayload::deserialize(const std::string &serialized) {
  serialization::CanonicalReader reader(serialized, 4096);
  if (reader.readString() != KEY_ROTATION_SCHEMA) {
    throw std::invalid_argument(
        "Unknown validator key rotation payload schema.");
  }
  const std::string oldValidatorAddress = reader.readString();
  const std::string algorithmStr = reader.readString();
  const std::string keyMaterial = reader.readString();
  const std::string metadataHash = reader.readString();
  const std::uint64_t activationEpoch = reader.readUInt64();
  const std::string reasonHash = reader.readString();
  ValidatorKeyRotationPayload payload(
      oldValidatorAddress,
      crypto::PublicKey(crypto::cryptoAlgorithmFromString(algorithmStr),
                        keyMaterial),
      metadataHash, activationEpoch, reasonHash);
  reader.requireFullyConsumed();
  if (!payload.isValid() || payload.serialize() != serialized) {
    throw std::invalid_argument(
        "Non-canonical validator key rotation payload.");
  }
  return payload;
}

std::string governanceProposalTypeToString(GovernanceProposalType type) {
  switch (type) {
  case GovernanceProposalType::PARAMETER_CHANGE:
    return "PARAMETER_CHANGE";
  case GovernanceProposalType::TREASURY_SPEND:
    return "TREASURY_SPEND";
  case GovernanceProposalType::TEXT:
    return "TEXT";
  }
  return "TEXT";
}

GovernanceProposalType
governanceProposalTypeFromString(const std::string &value) {
  if (value == "PARAMETER_CHANGE")
    return GovernanceProposalType::PARAMETER_CHANGE;
  if (value == "TREASURY_SPEND")
    return GovernanceProposalType::TREASURY_SPEND;
  if (value == "TEXT")
    return GovernanceProposalType::TEXT;
  throw std::invalid_argument("Unknown governance proposal type.");
}

GovernanceProposalPayload::GovernanceProposalPayload()
    : m_type(GovernanceProposalType::TEXT), m_votingStartDelayBlocks(0),
      m_votingPeriodBlocks(0), m_quorumNumerator(0), m_quorumDenominator(0),
      m_approvalNumerator(0), m_approvalDenominator(0),
      m_parameterEffectiveHeight(0), m_treasuryAmountRaw(0) {}

GovernanceProposalPayload::GovernanceProposalPayload(
    GovernanceProposalType type, std::string title, std::string description,
    std::uint64_t votingStartDelayBlocks, std::uint64_t votingPeriodBlocks,
    std::uint64_t quorumNumerator, std::uint64_t quorumDenominator,
    std::uint64_t approvalNumerator, std::uint64_t approvalDenominator,
    std::string parameterTarget, std::string parameterValue,
    std::uint64_t parameterEffectiveHeight, std::string treasuryRecipient,
    std::int64_t treasuryAmountRaw)
    : m_type(type), m_title(std::move(title)),
      m_description(std::move(description)),
      m_votingStartDelayBlocks(votingStartDelayBlocks),
      m_votingPeriodBlocks(votingPeriodBlocks),
      m_quorumNumerator(quorumNumerator),
      m_quorumDenominator(quorumDenominator),
      m_approvalNumerator(approvalNumerator),
      m_approvalDenominator(approvalDenominator),
      m_parameterTarget(std::move(parameterTarget)),
      m_parameterValue(std::move(parameterValue)),
      m_parameterEffectiveHeight(parameterEffectiveHeight),
      m_treasuryRecipient(std::move(treasuryRecipient)),
      m_treasuryAmountRaw(treasuryAmountRaw) {}

GovernanceProposalPayload GovernanceProposalPayload::parameterChange(
    std::string title, std::string description, std::string target,
    std::string value, std::uint64_t effectiveHeight,
    std::uint64_t votingStartDelayBlocks, std::uint64_t votingPeriodBlocks,
    std::uint64_t quorumNumerator, std::uint64_t quorumDenominator,
    std::uint64_t approvalNumerator, std::uint64_t approvalDenominator) {
  return GovernanceProposalPayload(
      GovernanceProposalType::PARAMETER_CHANGE, std::move(title),
      std::move(description), votingStartDelayBlocks, votingPeriodBlocks,
      quorumNumerator, quorumDenominator, approvalNumerator,
      approvalDenominator, std::move(target), std::move(value), effectiveHeight,
      "", 0);
}

GovernanceProposalPayload GovernanceProposalPayload::treasurySpend(
    std::string title, std::string description, std::string recipient,
    std::int64_t amountRaw, std::uint64_t votingStartDelayBlocks,
    std::uint64_t votingPeriodBlocks, std::uint64_t quorumNumerator,
    std::uint64_t quorumDenominator, std::uint64_t approvalNumerator,
    std::uint64_t approvalDenominator) {
  return GovernanceProposalPayload(
      GovernanceProposalType::TREASURY_SPEND, std::move(title),
      std::move(description), votingStartDelayBlocks, votingPeriodBlocks,
      quorumNumerator, quorumDenominator, approvalNumerator,
      approvalDenominator, "", "", 0, std::move(recipient), amountRaw);
}

GovernanceProposalPayload GovernanceProposalPayload::text(
    std::string title, std::string description,
    std::uint64_t votingStartDelayBlocks, std::uint64_t votingPeriodBlocks,
    std::uint64_t quorumNumerator, std::uint64_t quorumDenominator,
    std::uint64_t approvalNumerator, std::uint64_t approvalDenominator) {
  return GovernanceProposalPayload(
      GovernanceProposalType::TEXT, std::move(title), std::move(description),
      votingStartDelayBlocks, votingPeriodBlocks, quorumNumerator,
      quorumDenominator, approvalNumerator, approvalDenominator, "", "", 0, "",
      0);
}

GovernanceProposalType GovernanceProposalPayload::type() const {
  return m_type;
}
const std::string &GovernanceProposalPayload::title() const { return m_title; }
const std::string &GovernanceProposalPayload::description() const {
  return m_description;
}
std::uint64_t GovernanceProposalPayload::votingStartDelayBlocks() const {
  return m_votingStartDelayBlocks;
}
std::uint64_t GovernanceProposalPayload::votingPeriodBlocks() const {
  return m_votingPeriodBlocks;
}
std::uint64_t GovernanceProposalPayload::quorumNumerator() const {
  return m_quorumNumerator;
}
std::uint64_t GovernanceProposalPayload::quorumDenominator() const {
  return m_quorumDenominator;
}
std::uint64_t GovernanceProposalPayload::approvalNumerator() const {
  return m_approvalNumerator;
}
std::uint64_t GovernanceProposalPayload::approvalDenominator() const {
  return m_approvalDenominator;
}
const std::string &GovernanceProposalPayload::parameterTarget() const {
  return m_parameterTarget;
}
const std::string &GovernanceProposalPayload::parameterValue() const {
  return m_parameterValue;
}
std::uint64_t GovernanceProposalPayload::parameterEffectiveHeight() const {
  return m_parameterEffectiveHeight;
}
const std::string &GovernanceProposalPayload::treasuryRecipient() const {
  return m_treasuryRecipient;
}
std::int64_t GovernanceProposalPayload::treasuryAmountRaw() const {
  return m_treasuryAmountRaw;
}

bool GovernanceProposalPayload::isValid() const {
  if (!safeText(m_title, MAX_GOVERNANCE_TITLE_BYTES) ||
      !safeText(m_description, MAX_GOVERNANCE_DESCRIPTION_BYTES) ||
      m_votingStartDelayBlocks > MAX_GOVERNANCE_VOTING_DELAY_BLOCKS ||
      m_votingPeriodBlocks == 0 ||
      m_votingPeriodBlocks > MAX_GOVERNANCE_VOTING_PERIOD_BLOCKS ||
      !validRatio(m_quorumNumerator, m_quorumDenominator) ||
      !validRatio(m_approvalNumerator, m_approvalDenominator)) {
    return false;
  }

  switch (m_type) {
  case GovernanceProposalType::PARAMETER_CHANGE:
    return safeScalar(m_parameterTarget, MAX_GOVERNANCE_SCALAR_BYTES) &&
           safeScalar(m_parameterValue, MAX_GOVERNANCE_SCALAR_BYTES) &&
           m_parameterEffectiveHeight > 0 && m_treasuryRecipient.empty() &&
           m_treasuryAmountRaw == 0;
  case GovernanceProposalType::TREASURY_SPEND:
    return m_parameterTarget.empty() && m_parameterValue.empty() &&
           m_parameterEffectiveHeight == 0 &&
           safeScalar(m_treasuryRecipient, MAX_GOVERNANCE_SCALAR_BYTES) &&
           m_treasuryAmountRaw > 0;
  case GovernanceProposalType::TEXT:
    return m_parameterTarget.empty() && m_parameterValue.empty() &&
           m_parameterEffectiveHeight == 0 && m_treasuryRecipient.empty() &&
           m_treasuryAmountRaw == 0;
  }
  return false;
}

std::string GovernanceProposalPayload::serialize() const {
  if (!isValid())
    throw std::invalid_argument("Invalid governance proposal payload.");
  serialization::CanonicalWriter writer;
  writer.writeString(PROPOSAL_SCHEMA);
  writer.writeString(governanceProposalTypeToString(m_type));
  writer.writeString(m_title);
  writer.writeString(m_description);
  writer.writeUInt64(m_votingStartDelayBlocks);
  writer.writeUInt64(m_votingPeriodBlocks);
  writer.writeUInt64(m_quorumNumerator);
  writer.writeUInt64(m_quorumDenominator);
  writer.writeUInt64(m_approvalNumerator);
  writer.writeUInt64(m_approvalDenominator);
  writer.writeString(m_parameterTarget);
  writer.writeString(m_parameterValue);
  writer.writeUInt64(m_parameterEffectiveHeight);
  writer.writeString(m_treasuryRecipient);
  writer.writeInt64(m_treasuryAmountRaw);
  return writer.byteString();
}

GovernanceProposalPayload
GovernanceProposalPayload::deserialize(const std::string &serialized) {
  serialization::CanonicalReader reader(serialized, 4096);
  if (reader.readString() != PROPOSAL_SCHEMA) {
    throw std::invalid_argument("Unknown governance proposal payload schema.");
  }
  const GovernanceProposalType type =
      governanceProposalTypeFromString(reader.readString());
  const std::string title = reader.readString();
  const std::string description = reader.readString();
  const std::uint64_t votingStartDelayBlocks = reader.readUInt64();
  const std::uint64_t votingPeriodBlocks = reader.readUInt64();
  const std::uint64_t quorumNumerator = reader.readUInt64();
  const std::uint64_t quorumDenominator = reader.readUInt64();
  const std::uint64_t approvalNumerator = reader.readUInt64();
  const std::uint64_t approvalDenominator = reader.readUInt64();
  const std::string parameterTarget = reader.readString();
  const std::string parameterValue = reader.readString();
  const std::uint64_t parameterEffectiveHeight = reader.readUInt64();
  const std::string treasuryRecipient = reader.readString();
  const std::int64_t treasuryAmountRaw = reader.readInt64();

  GovernanceProposalPayload payload(
      type, title, description, votingStartDelayBlocks, votingPeriodBlocks,
      quorumNumerator, quorumDenominator, approvalNumerator,
      approvalDenominator, parameterTarget, parameterValue,
      parameterEffectiveHeight, treasuryRecipient, treasuryAmountRaw);
  reader.requireFullyConsumed();
  if (!payload.isValid() || payload.serialize() != serialized) {
    throw std::invalid_argument("Non-canonical governance proposal payload.");
  }
  return payload;
}

std::string governanceVoteChoiceToString(GovernanceVoteChoice choice) {
  switch (choice) {
  case GovernanceVoteChoice::YES:
    return "YES";
  case GovernanceVoteChoice::NO:
    return "NO";
  case GovernanceVoteChoice::ABSTAIN:
    return "ABSTAIN";
  }
  return "ABSTAIN";
}

GovernanceVoteChoice governanceVoteChoiceFromString(const std::string &value) {
  if (value == "YES")
    return GovernanceVoteChoice::YES;
  if (value == "NO")
    return GovernanceVoteChoice::NO;
  if (value == "ABSTAIN")
    return GovernanceVoteChoice::ABSTAIN;
  throw std::invalid_argument("Unknown governance vote choice.");
}

GovernanceVotePayload::GovernanceVotePayload()
    : m_choice(GovernanceVoteChoice::NO) {}

GovernanceVotePayload::GovernanceVotePayload(std::string proposalId,
                                             std::string validatorAddress,
                                             GovernanceVoteChoice choice)
    : m_proposalId(std::move(proposalId)),
      m_validatorAddress(std::move(validatorAddress)), m_choice(choice) {}

const std::string &GovernanceVotePayload::proposalId() const {
  return m_proposalId;
}
const std::string &GovernanceVotePayload::validatorAddress() const {
  return m_validatorAddress;
}
GovernanceVoteChoice GovernanceVotePayload::choice() const { return m_choice; }
bool GovernanceVotePayload::isValid() const {
  return safeScalar(m_proposalId) && safeScalar(m_validatorAddress);
}

std::string GovernanceVotePayload::serialize() const {
  if (!isValid())
    throw std::invalid_argument("Invalid governance vote payload.");
  serialization::CanonicalWriter writer;
  writer.writeString(VOTE_SCHEMA);
  writer.writeString(m_proposalId);
  writer.writeString(m_validatorAddress);
  writer.writeString(governanceVoteChoiceToString(m_choice));
  return writer.byteString();
}

GovernanceVotePayload
GovernanceVotePayload::deserialize(const std::string &serialized) {
  serialization::CanonicalReader reader(serialized, 4096);
  if (reader.readString() != VOTE_SCHEMA) {
    throw std::invalid_argument("Unknown governance vote payload schema.");
  }
  const std::string proposalId = reader.readString();
  const std::string validatorAddress = reader.readString();
  const std::string choiceStr = reader.readString();
  GovernanceVotePayload payload(proposalId, validatorAddress,
                                governanceVoteChoiceFromString(choiceStr));
  reader.requireFullyConsumed();
  if (!payload.isValid() || payload.serialize() != serialized) {
    throw std::invalid_argument("Non-canonical governance vote payload.");
  }
  return payload;
}

} // namespace nodo::core
