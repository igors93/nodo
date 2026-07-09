#include "node/ProtocolDomainCodec.hpp"

#include "core/StateRootCalculator.hpp"
#include "crypto/Hex.hpp"
#include "serialization/CanonicalHash.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

constexpr std::size_t MAX_DOMAIN_CODEC_FIELD_BYTES = 16 * 1024 * 1024;
constexpr const char *DOMAIN_CODEC_VERSION = "NODO_PROTOCOL_DOMAIN_CODEC_V1";

std::vector<unsigned char> hexToBytesStrict(const std::string &encodedHex,
                                            const char *domainName) {
  try {
    return crypto::hexDecode(encodedHex);
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("Malformed hex payload for ") +
                             domainName + " domain: " + error.what());
  }
}

void writeHeader(serialization::CanonicalWriter &writer,
                 const std::string &typeName) {
  writer.writeString(DOMAIN_CODEC_VERSION);
  writer.writeString(typeName);
}

void readHeader(serialization::CanonicalReader &reader,
               const std::string &expectedType) {
  const std::string version = reader.readString();
  const std::string type = reader.readString();
  if (version != DOMAIN_CODEC_VERSION) {
    throw std::runtime_error("Unsupported protocol domain codec version.");
  }
  if (type != expectedType) {
    throw std::runtime_error("Unexpected protocol domain codec type: " +
                             type);
  }
}

// ---------------------------------------------------------------------------
// supply
// ---------------------------------------------------------------------------

void writeSupply(serialization::CanonicalWriter &writer,
                 const utils::Amount &supply) {
  writer.writeInt64(supply.rawUnits());
}

utils::Amount readSupply(serialization::CanonicalReader &reader) {
  return utils::Amount::fromRawUnits(reader.readInt64());
}

// ---------------------------------------------------------------------------
// burns
// ---------------------------------------------------------------------------

void writeBurns(serialization::CanonicalWriter &writer,
                const std::vector<economics::BurnRecord> &burns) {
  writer.writeUInt32(static_cast<std::uint32_t>(burns.size()));
  for (const economics::BurnRecord &burn : burns) {
    writer.writeString(burn.burnId());
    writer.writeUInt64(burn.blockHeight());
    writer.writeUInt64(burn.epoch());
    writer.writeString(burn.sourceAddress());
    writer.writeInt64(burn.amount().rawUnits());
    writer.writeString(burn.reason());
    writer.writeString(economics::burnTypeToString(burn.burnType()));
  }
}

std::vector<economics::BurnRecord>
readBurns(serialization::CanonicalReader &reader) {
  const std::uint32_t count = reader.readUInt32();
  std::vector<economics::BurnRecord> burns;
  burns.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::string burnId = reader.readString();
    const std::uint64_t blockHeight = reader.readUInt64();
    const std::uint64_t epoch = reader.readUInt64();
    const std::string sourceAddress = reader.readString();
    const utils::Amount amount =
        utils::Amount::fromRawUnits(reader.readInt64());
    const std::string reason = reader.readString();
    const economics::BurnType burnType =
        economics::burnTypeFromString(reader.readString());
    economics::BurnRecord record(burnId, blockHeight, epoch, sourceAddress,
                                 amount, reason, burnType);
    if (!record.isValid()) {
      throw std::runtime_error("Decoded burn record is invalid: " +
                               record.rejectionReason());
    }
    burns.push_back(std::move(record));
  }
  return burns;
}

// ---------------------------------------------------------------------------
// staking
// ---------------------------------------------------------------------------

std::uint32_t encodeStakePositionStatus(StakePositionStatus status) {
  return static_cast<std::uint32_t>(status);
}

StakePositionStatus decodeStakePositionStatus(std::uint32_t raw) {
  if (raw > static_cast<std::uint32_t>(StakePositionStatus::TOMBSTONED)) {
    throw std::runtime_error("Invalid stake position status.");
  }
  return static_cast<StakePositionStatus>(raw);
}

void writeStaking(serialization::CanonicalWriter &writer,
                  const StakingRegistry &staking) {
  const auto &accounts = staking.accounts();
  writer.writeUInt32(static_cast<std::uint32_t>(accounts.size()));
  for (const auto &[address, account] : accounts) {
    writer.writeString(account.validatorAddress());
    writer.writeInt64(account.bondedAmount().rawUnits());
    writer.writeInt64(account.slashedAmount().rawUnits());
    writer.writeBool(account.jailed());
    writer.writeBool(account.tombstoned());
  }

  const std::vector<StakePositionView> positions = staking.positions();
  writer.writeUInt32(static_cast<std::uint32_t>(positions.size()));
  for (const StakePositionView &position : positions) {
    writer.writeString(position.ownerAddress);
    writer.writeString(position.validatorAddress);
    writer.writeInt64(position.activeAmount.rawUnits());
    writer.writeInt64(position.pendingActivationAmount.rawUnits());
    writer.writeInt64(position.pendingUnbondingAmount.rawUnits());
    writer.writeInt64(position.withdrawnAmount.rawUnits());
    writer.writeInt64(position.slashedAmount.rawUnits());
    writer.writeInt64(position.rewardsPending.rawUnits());
    writer.writeUInt64(position.lockHeight);
    writer.writeUInt64(position.activationHeight);
    writer.writeUInt64(position.unbondingStartHeight);
    writer.writeUInt64(position.withdrawableHeight);
    writer.writeUInt32(encodeStakePositionStatus(position.status));
  }

  const std::vector<StakeLifecycleRecord> &lifecycle =
      staking.lifecycleRecords();
  writer.writeUInt32(static_cast<std::uint32_t>(lifecycle.size()));
  for (const StakeLifecycleRecord &record : lifecycle) {
    writer.writeString(record.recordId);
    writer.writeString(record.action);
    writer.writeString(record.transactionId);
    writer.writeString(record.ownerAddress);
    writer.writeString(record.validatorAddress);
    writer.writeInt64(record.amount.rawUnits());
    writer.writeInt64(record.activeAfter.rawUnits());
    writer.writeInt64(record.pendingActivationAfter.rawUnits());
    writer.writeInt64(record.pendingUnbondingAfter.rawUnits());
    writer.writeInt64(record.slashedAfter.rawUnits());
    writer.writeUInt64(record.blockHeight);
    writer.writeUInt64(record.activationHeight);
    writer.writeUInt64(record.withdrawableHeight);
    writer.writeString(record.reason);
  }
}

StakingRegistry readStaking(serialization::CanonicalReader &reader) {
  const std::uint32_t accountCount = reader.readUInt32();
  std::map<std::string, economics::StakeAccount> accounts;
  for (std::uint32_t i = 0; i < accountCount; ++i) {
    const std::string address = reader.readString();
    const utils::Amount bonded =
        utils::Amount::fromRawUnits(reader.readInt64());
    const utils::Amount slashed =
        utils::Amount::fromRawUnits(reader.readInt64());
    const bool jailed = reader.readBool();
    const bool tombstoned = reader.readBool();
    economics::StakeAccount account(address, bonded, slashed, jailed,
                                    tombstoned);
    if (!account.isValid()) {
      throw std::runtime_error("Decoded stake account is invalid.");
    }
    if (!accounts.emplace(address, std::move(account)).second) {
      throw std::runtime_error("Duplicate stake account address: " + address);
    }
  }

  const std::uint32_t positionCount = reader.readUInt32();
  std::vector<StakePositionView> positions;
  positions.reserve(positionCount);
  for (std::uint32_t i = 0; i < positionCount; ++i) {
    StakePositionView view;
    view.ownerAddress = reader.readString();
    view.validatorAddress = reader.readString();
    view.activeAmount = utils::Amount::fromRawUnits(reader.readInt64());
    view.pendingActivationAmount =
        utils::Amount::fromRawUnits(reader.readInt64());
    view.pendingUnbondingAmount =
        utils::Amount::fromRawUnits(reader.readInt64());
    view.withdrawnAmount = utils::Amount::fromRawUnits(reader.readInt64());
    view.slashedAmount = utils::Amount::fromRawUnits(reader.readInt64());
    view.rewardsPending = utils::Amount::fromRawUnits(reader.readInt64());
    view.lockHeight = reader.readUInt64();
    view.activationHeight = reader.readUInt64();
    view.unbondingStartHeight = reader.readUInt64();
    view.withdrawableHeight = reader.readUInt64();
    view.status = decodeStakePositionStatus(reader.readUInt32());
    view.positionId =
        StakingRegistry::stakePositionId(view.ownerAddress,
                                         view.validatorAddress);
    positions.push_back(std::move(view));
  }

  const std::uint32_t lifecycleCount = reader.readUInt32();
  std::vector<StakeLifecycleRecord> lifecycle;
  lifecycle.reserve(lifecycleCount);
  for (std::uint32_t i = 0; i < lifecycleCount; ++i) {
    StakeLifecycleRecord record;
    record.recordId = reader.readString();
    record.action = reader.readString();
    record.transactionId = reader.readString();
    record.ownerAddress = reader.readString();
    record.validatorAddress = reader.readString();
    record.amount = utils::Amount::fromRawUnits(reader.readInt64());
    record.activeAfter = utils::Amount::fromRawUnits(reader.readInt64());
    record.pendingActivationAfter =
        utils::Amount::fromRawUnits(reader.readInt64());
    record.pendingUnbondingAfter =
        utils::Amount::fromRawUnits(reader.readInt64());
    record.slashedAfter = utils::Amount::fromRawUnits(reader.readInt64());
    record.blockHeight = reader.readUInt64();
    record.activationHeight = reader.readUInt64();
    record.withdrawableHeight = reader.readUInt64();
    record.reason = reader.readString();
    if (!record.isValid()) {
      throw std::runtime_error("Decoded stake lifecycle record is invalid.");
    }
    lifecycle.push_back(std::move(record));
  }

  return StakingRegistry::restore(std::move(accounts), std::move(positions),
                                  std::move(lifecycle));
}

// ---------------------------------------------------------------------------
// governance
// ---------------------------------------------------------------------------

std::uint32_t
encodeProposalStatus(GovernanceProposalStatus status) {
  return static_cast<std::uint32_t>(status);
}

GovernanceProposalStatus decodeProposalStatus(std::uint32_t raw) {
  if (raw > static_cast<std::uint32_t>(
                GovernanceProposalStatus::FAILED_EXECUTION)) {
    throw std::runtime_error("Invalid governance proposal status.");
  }
  return static_cast<GovernanceProposalStatus>(raw);
}

void writeParameterChange(serialization::CanonicalWriter &writer,
                          const GovernanceParameterChange &change) {
  writer.writeString(change.proposalId());
  writer.writeString(governanceParameterTargetToString(change.target()));
  writer.writeString(change.previousValue());
  writer.writeString(change.newValue());
  writer.writeUInt64(change.effectiveAtHeight());
  writer.writeInt64(change.appliedAt());
}

GovernanceParameterChange
readParameterChange(serialization::CanonicalReader &reader) {
  const std::string proposalId = reader.readString();
  const GovernanceParameterTarget target =
      governanceParameterTargetFromString(reader.readString());
  const std::string previousValue = reader.readString();
  const std::string newValue = reader.readString();
  const std::uint64_t effectiveAtHeight = reader.readUInt64();
  const std::int64_t appliedAt = reader.readInt64();
  GovernanceParameterChange change(proposalId, target, previousValue,
                                   newValue, effectiveAtHeight, appliedAt);
  if (!change.isValid()) {
    throw std::runtime_error(
        "Decoded governance parameter change is invalid.");
  }
  return change;
}

void writeProposalPayload(serialization::CanonicalWriter &writer,
                          const core::GovernanceProposalPayload &payload) {
  writer.writeString(core::governanceProposalTypeToString(payload.type()));
  writer.writeString(payload.title());
  writer.writeString(payload.description());
  writer.writeUInt64(payload.votingStartDelayBlocks());
  writer.writeUInt64(payload.votingPeriodBlocks());
  writer.writeUInt64(payload.quorumNumerator());
  writer.writeUInt64(payload.quorumDenominator());
  writer.writeUInt64(payload.approvalNumerator());
  writer.writeUInt64(payload.approvalDenominator());
  writer.writeString(payload.parameterTarget());
  writer.writeString(payload.parameterValue());
  writer.writeUInt64(payload.parameterEffectiveHeight());
  writer.writeString(payload.treasuryRecipient());
  writer.writeInt64(payload.treasuryAmountRaw());
}

core::GovernanceProposalPayload
readProposalPayload(serialization::CanonicalReader &reader) {
  const core::GovernanceProposalType type =
      core::governanceProposalTypeFromString(reader.readString());
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

  core::GovernanceProposalPayload payload(
      type, title, description, votingStartDelayBlocks, votingPeriodBlocks,
      quorumNumerator, quorumDenominator, approvalNumerator,
      approvalDenominator, parameterTarget, parameterValue,
      parameterEffectiveHeight, treasuryRecipient, treasuryAmountRaw);
  if (!payload.isValid()) {
    throw std::runtime_error(
        "Decoded governance proposal payload is invalid.");
  }
  return payload;
}

void writeTally(serialization::CanonicalWriter &writer,
                const GovernanceTallySnapshot &tally) {
  writer.writeString(tally.proposalId());
  writer.writeUInt64(tally.yesWeight());
  writer.writeUInt64(tally.noWeight());
  writer.writeUInt64(tally.abstainWeight());
  writer.writeUInt64(tally.participatingWeight());
  writer.writeUInt64(tally.totalEligibleWeight());
  writer.writeBool(tally.quorumMet());
  writer.writeBool(tally.approvalThresholdMet());
}

GovernanceTallySnapshot readTally(serialization::CanonicalReader &reader) {
  const std::string proposalId = reader.readString();
  const std::uint64_t yesWeight = reader.readUInt64();
  const std::uint64_t noWeight = reader.readUInt64();
  const std::uint64_t abstainWeight = reader.readUInt64();
  const std::uint64_t participatingWeight = reader.readUInt64();
  const std::uint64_t totalEligibleWeight = reader.readUInt64();
  const bool quorumMet = reader.readBool();
  const bool approvalThresholdMet = reader.readBool();
  return GovernanceTallySnapshot(proposalId, yesWeight, noWeight,
                                 abstainWeight, participatingWeight,
                                 totalEligibleWeight, quorumMet,
                                 approvalThresholdMet);
}

void writeProposal(serialization::CanonicalWriter &writer,
                   const GovernanceExecutor::ProposalRecord &record) {
  writer.writeString(record.proposalId);
  writer.writeString(record.proposerAddress);
  writeProposalPayload(writer, record.payload);
  writer.writeUInt64(record.createdHeight);
  writer.writeInt64(record.createdAt);
  writer.writeUInt64(record.votingStartHeight);
  writer.writeUInt64(record.votingEndHeight);
  writer.writeUInt64(record.totalEligibleWeight);
  writer.writeUInt32(encodeProposalStatus(record.status));
  writeTally(writer, record.finalTally);
  writer.writeUInt64(record.decidedAtHeight);
  writer.writeInt64(record.decidedAt);
  writer.writeUInt64(record.executedAtHeight);
  writer.writeInt64(record.executedAt);
  writer.writeString(record.executionDetail);
  writer.writeUInt64(record.treasuryExecutableAtHeight);
  writer.writeInt64(record.treasuryBalanceBeforeExecution.rawUnits());

  writer.writeUInt32(static_cast<std::uint32_t>(record.votes.size()));
  for (const GovernanceExecutor::GovernanceVoteInfo &vote : record.votes) {
    writer.writeString(vote.validatorAddress);
    writer.writeString(core::governanceVoteChoiceToString(vote.choice));
    writer.writeUInt64(vote.weight);
    writer.writeUInt64(vote.castHeight);
    writer.writeInt64(vote.castAt);
    writer.writeString(vote.transactionId);
  }
}

GovernanceExecutor::ProposalRecord
readProposal(serialization::CanonicalReader &reader) {
  GovernanceExecutor::ProposalRecord record;
  record.proposalId = reader.readString();
  record.proposerAddress = reader.readString();
  record.payload = readProposalPayload(reader);
  record.createdHeight = reader.readUInt64();
  record.createdAt = reader.readInt64();
  record.votingStartHeight = reader.readUInt64();
  record.votingEndHeight = reader.readUInt64();
  record.totalEligibleWeight = reader.readUInt64();
  record.status = decodeProposalStatus(reader.readUInt32());
  record.finalTally = readTally(reader);
  record.decidedAtHeight = reader.readUInt64();
  record.decidedAt = reader.readInt64();
  record.executedAtHeight = reader.readUInt64();
  record.executedAt = reader.readInt64();
  record.executionDetail = reader.readString();
  record.treasuryExecutableAtHeight = reader.readUInt64();
  record.treasuryBalanceBeforeExecution =
      utils::Amount::fromRawUnits(reader.readInt64());

  const std::uint32_t voteCount = reader.readUInt32();
  record.votes.reserve(voteCount);
  for (std::uint32_t i = 0; i < voteCount; ++i) {
    GovernanceExecutor::GovernanceVoteInfo vote;
    vote.validatorAddress = reader.readString();
    vote.choice = core::governanceVoteChoiceFromString(reader.readString());
    vote.weight = reader.readUInt64();
    vote.castHeight = reader.readUInt64();
    vote.castAt = reader.readInt64();
    vote.transactionId = reader.readString();
    record.votes.push_back(std::move(vote));
  }

  if (record.proposalId.empty()) {
    throw std::runtime_error("Decoded governance proposal id is empty.");
  }
  return record;
}

void writeGovernance(serialization::CanonicalWriter &writer,
                     const GovernanceExecutor &governance) {
  const std::vector<GovernanceParameterChange> &applied =
      governance.appliedChanges();
  writer.writeUInt32(static_cast<std::uint32_t>(applied.size()));
  for (const GovernanceParameterChange &change : applied) {
    writeParameterChange(writer, change);
  }

  const std::vector<GovernanceParameterChange> pending =
      governance.pendingChanges();
  writer.writeUInt32(static_cast<std::uint32_t>(pending.size()));
  for (const GovernanceParameterChange &change : pending) {
    writeParameterChange(writer, change);
  }

  const std::vector<GovernanceExecutor::ProposalRecord> proposals =
      governance.allProposalRecords();
  writer.writeUInt32(static_cast<std::uint32_t>(proposals.size()));
  for (const GovernanceExecutor::ProposalRecord &proposal : proposals) {
    writeProposal(writer, proposal);
  }
}

GovernanceExecutor readGovernance(serialization::CanonicalReader &reader) {
  const std::uint32_t appliedCount = reader.readUInt32();
  std::vector<GovernanceParameterChange> applied;
  applied.reserve(appliedCount);
  for (std::uint32_t i = 0; i < appliedCount; ++i) {
    applied.push_back(readParameterChange(reader));
  }

  const std::uint32_t pendingCount = reader.readUInt32();
  std::vector<GovernanceParameterChange> pending;
  pending.reserve(pendingCount);
  for (std::uint32_t i = 0; i < pendingCount; ++i) {
    pending.push_back(readParameterChange(reader));
  }

  const std::uint32_t proposalCount = reader.readUInt32();
  std::vector<GovernanceExecutor::ProposalRecord> proposals;
  proposals.reserve(proposalCount);
  for (std::uint32_t i = 0; i < proposalCount; ++i) {
    proposals.push_back(readProposal(reader));
  }

  return GovernanceExecutor::restore(std::move(applied), std::move(pending),
                                     std::move(proposals));
}

// ---------------------------------------------------------------------------
// validators
// ---------------------------------------------------------------------------

void writeValidators(serialization::CanonicalWriter &writer,
                     const core::ValidatorRegistry &validators) {
  const std::vector<std::string> addresses = validators.validatorAddresses();
  writer.writeUInt32(static_cast<std::uint32_t>(addresses.size()));
  for (const std::string &address : addresses) {
    const core::ValidatorRegistryEntry *entry =
        validators.entryForAddress(address);
    if (entry == nullptr) {
      throw std::logic_error(
          "Validator address listed but entry is missing.");
    }
    const core::ValidatorRegistrationRecord &record =
        entry->registrationRecord();
    writer.writeString(record.validatorAddress());
    writer.writeString(
        crypto::cryptoAlgorithmToString(record.validatorPublicKey().algorithm()));
    writer.writeString(record.validatorPublicKey().keyMaterial());
    writer.writeUInt64(record.activationEpoch());
    writer.writeString(record.metadataHash());
    writer.writeInt64(record.registeredAt());
    writer.writeString(
        core::validatorRegistrationStatusToString(entry->status()));
    writer.writeInt64(entry->lastUpdatedAt());
    writer.writeUInt64(entry->stakeAmount());
    writer.writeUInt64(entry->jailUntilEpoch());
    writer.writeUInt64(entry->exitRequestHeight());
    writer.writeString(entry->ownerAddress());
  }
}

core::ValidatorRegistry readValidators(serialization::CanonicalReader &reader) {
  const std::uint32_t count = reader.readUInt32();
  core::ValidatorRegistry registry;
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::string validatorAddress = reader.readString();
    const crypto::CryptoAlgorithm algorithm =
        crypto::cryptoAlgorithmFromString(reader.readString());
    const std::string keyMaterial = reader.readString();
    const std::uint64_t activationEpoch = reader.readUInt64();
    const std::string metadataHash = reader.readString();
    const std::int64_t registeredAt = reader.readInt64();
    const core::ValidatorRegistrationStatus status =
        core::validatorRegistrationStatusFromString(reader.readString());
    const std::int64_t lastUpdatedAt = reader.readInt64();
    const std::uint64_t stakeAmount = reader.readUInt64();
    const std::uint64_t jailUntilEpoch = reader.readUInt64();
    const std::uint64_t exitRequestHeight = reader.readUInt64();
    const std::string ownerAddress = reader.readString();

    const crypto::PublicKey publicKey(algorithm, keyMaterial);
    const core::ValidatorRegistrationRecord record(
        validatorAddress, publicKey, activationEpoch, metadataHash,
        registeredAt);
    const core::ValidatorRegistryEntry entry(
        record, status, lastUpdatedAt, stakeAmount, jailUntilEpoch,
        exitRequestHeight, ownerAddress);

    if (!registry.restoreEntry(entry)) {
      throw std::runtime_error(
          "Decoded validator registry entry is invalid or duplicate: " +
          validatorAddress);
    }
  }
  return registry;
}

// ---------------------------------------------------------------------------
// slashing
// ---------------------------------------------------------------------------

void writeSlashing(serialization::CanonicalWriter &writer,
                   const consensus::ValidatorPenaltyLedger &ledger) {
  const std::vector<consensus::ValidatorPenaltyDecision> decisions =
      ledger.allDecisions();
  writer.writeUInt32(static_cast<std::uint32_t>(decisions.size()));
  for (const consensus::ValidatorPenaltyDecision &decision : decisions) {
    writer.writeString(decision.penaltyId());
    writer.writeString(decision.evidenceId());
    writer.writeString(decision.validatorAddress());
    writer.writeString(
        consensus::slashingEvidenceTypeToString(decision.evidenceType()));
    writer.writeString(consensus::slashingEvidenceSeverityToString(
        decision.evidenceSeverity()));
    writer.writeString(
        consensus::validatorPenaltyActionToString(decision.action()));
    writer.writeInt64(decision.slashAmountRawUnits());
    writer.writeUInt64(decision.jailEpochs());
    writer.writeInt64(decision.createdAt());
  }
}

consensus::ValidatorPenaltyLedger
readSlashing(serialization::CanonicalReader &reader) {
  const std::uint32_t count = reader.readUInt32();
  consensus::ValidatorPenaltyLedger ledger;
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::string penaltyId = reader.readString();
    const std::string evidenceId = reader.readString();
    const std::string validatorAddress = reader.readString();
    const consensus::SlashingEvidenceType evidenceType =
        consensus::slashingEvidenceTypeFromString(reader.readString());
    const consensus::SlashingEvidenceSeverity evidenceSeverity =
        consensus::slashingEvidenceSeverityFromString(reader.readString());
    const consensus::ValidatorPenaltyAction action =
        consensus::validatorPenaltyActionFromString(reader.readString());
    const std::int64_t slashAmountRawUnits = reader.readInt64();
    const std::uint64_t jailEpochs = reader.readUInt64();
    const std::int64_t createdAt = reader.readInt64();

    const consensus::ValidatorPenaltyDecision decision(
        penaltyId, evidenceId, validatorAddress, evidenceType,
        evidenceSeverity, action, slashAmountRawUnits, jailEpochs, createdAt);
    if (!decision.isValid()) {
      throw std::runtime_error(
          "Decoded validator penalty decision is invalid.");
    }

    const consensus::ValidatorPenaltyApplicationResult result =
        ledger.applyDecision(decision);
    if (!result.applied() && !result.duplicate()) {
      throw std::runtime_error(
          "Failed to restore validator penalty decision: " + result.reason());
    }
  }
  return ledger;
}

} // namespace

// ---------------------------------------------------------------------------
// SupplyDomainCodec
// ---------------------------------------------------------------------------

std::string SupplyDomainCodec::encode(const utils::Amount &supply) {
  serialization::CanonicalWriter writer;
  writeHeader(writer, "Supply");
  writeSupply(writer, supply);
  return writer.hex();
}

utils::Amount SupplyDomainCodec::decode(const std::string &encodedHex) {
  serialization::CanonicalReader reader(
      hexToBytesStrict(encodedHex, "supply"), MAX_DOMAIN_CODEC_FIELD_BYTES);
  readHeader(reader, "Supply");
  const utils::Amount supply = readSupply(reader);
  reader.requireFullyConsumed();
  if (encode(supply) != encodedHex) {
    throw std::runtime_error("Supply domain round-trip mismatch.");
  }
  return supply;
}

std::string SupplyDomainCodec::calculateRoot(const utils::Amount &supply) {
  serialization::CanonicalWriter writer;
  writeSupply(writer, supply);
  return serialization::CanonicalHash::hashBytes(writer.bytes(),
                                                 "NODO_SUPPLY_DOMAIN_ROOT_V1");
}

bool SupplyDomainCodec::validateRoot(const utils::Amount &supply,
                                     const std::string &expectedRoot) {
  return calculateRoot(supply) == expectedRoot;
}

// ---------------------------------------------------------------------------
// BurnsDomainCodec
// ---------------------------------------------------------------------------

std::string
BurnsDomainCodec::encode(const std::vector<economics::BurnRecord> &burns) {
  std::vector<economics::BurnRecord> sorted = burns;
  std::sort(sorted.begin(), sorted.end(),
           [](const economics::BurnRecord &left,
              const economics::BurnRecord &right) {
             return left.burnId() < right.burnId();
           });
  serialization::CanonicalWriter writer;
  writeHeader(writer, "Burns");
  writeBurns(writer, sorted);
  return writer.hex();
}

std::vector<economics::BurnRecord>
BurnsDomainCodec::decode(const std::string &encodedHex) {
  serialization::CanonicalReader reader(hexToBytesStrict(encodedHex, "burns"),
                                        MAX_DOMAIN_CODEC_FIELD_BYTES);
  readHeader(reader, "Burns");
  std::vector<economics::BurnRecord> burns = readBurns(reader);
  reader.requireFullyConsumed();
  if (encode(burns) != encodedHex) {
    throw std::runtime_error("Burns domain round-trip mismatch.");
  }
  return burns;
}

std::string BurnsDomainCodec::calculateRoot(
    const std::vector<economics::BurnRecord> &burns) {
  std::vector<economics::BurnRecord> sorted = burns;
  std::sort(sorted.begin(), sorted.end(),
           [](const economics::BurnRecord &left,
              const economics::BurnRecord &right) {
             return left.burnId() < right.burnId();
           });
  serialization::CanonicalWriter writer;
  writeBurns(writer, sorted);
  return serialization::CanonicalHash::hashBytes(writer.bytes(),
                                                 "NODO_BURNS_DOMAIN_ROOT_V1");
}

bool BurnsDomainCodec::validateRoot(
    const std::vector<economics::BurnRecord> &burns,
    const std::string &expectedRoot) {
  return calculateRoot(burns) == expectedRoot;
}

// ---------------------------------------------------------------------------
// StakingDomainCodec
// ---------------------------------------------------------------------------

std::string StakingDomainCodec::encode(const StakingRegistry &staking) {
  serialization::CanonicalWriter writer;
  writeHeader(writer, "Staking");
  writeStaking(writer, staking);
  return writer.hex();
}

StakingRegistry StakingDomainCodec::decode(const std::string &encodedHex) {
  serialization::CanonicalReader reader(
      hexToBytesStrict(encodedHex, "staking"), MAX_DOMAIN_CODEC_FIELD_BYTES);
  readHeader(reader, "Staking");
  StakingRegistry staking = readStaking(reader);
  reader.requireFullyConsumed();
  if (encode(staking) != encodedHex) {
    throw std::runtime_error("Staking domain round-trip mismatch.");
  }
  return staking;
}

std::string StakingDomainCodec::calculateRoot(const StakingRegistry &staking) {
  serialization::CanonicalWriter writer;
  writeStaking(writer, staking);
  return serialization::CanonicalHash::hashBytes(
      writer.bytes(), "NODO_STAKING_DOMAIN_ROOT_V1");
}

bool StakingDomainCodec::validateRoot(const StakingRegistry &staking,
                                      const std::string &expectedRoot) {
  return calculateRoot(staking) == expectedRoot;
}

// ---------------------------------------------------------------------------
// GovernanceDomainCodec
// ---------------------------------------------------------------------------

std::string GovernanceDomainCodec::encode(const GovernanceExecutor &governance) {
  serialization::CanonicalWriter writer;
  writeHeader(writer, "Governance");
  writeGovernance(writer, governance);
  return writer.hex();
}

GovernanceExecutor GovernanceDomainCodec::decode(const std::string &encodedHex) {
  serialization::CanonicalReader reader(
      hexToBytesStrict(encodedHex, "governance"),
      MAX_DOMAIN_CODEC_FIELD_BYTES);
  readHeader(reader, "Governance");
  GovernanceExecutor governance = readGovernance(reader);
  reader.requireFullyConsumed();
  if (encode(governance) != encodedHex) {
    throw std::runtime_error("Governance domain round-trip mismatch.");
  }
  return governance;
}

std::string
GovernanceDomainCodec::calculateRoot(const GovernanceExecutor &governance) {
  serialization::CanonicalWriter writer;
  writeGovernance(writer, governance);
  return serialization::CanonicalHash::hashBytes(
      writer.bytes(), "NODO_GOVERNANCE_DOMAIN_ROOT_V1");
}

bool GovernanceDomainCodec::validateRoot(const GovernanceExecutor &governance,
                                         const std::string &expectedRoot) {
  return calculateRoot(governance) == expectedRoot;
}

// ---------------------------------------------------------------------------
// ValidatorsDomainCodec
// ---------------------------------------------------------------------------

std::string
ValidatorsDomainCodec::encode(const core::ValidatorRegistry &validators) {
  serialization::CanonicalWriter writer;
  writeHeader(writer, "Validators");
  writeValidators(writer, validators);
  return writer.hex();
}

core::ValidatorRegistry
ValidatorsDomainCodec::decode(const std::string &encodedHex) {
  serialization::CanonicalReader reader(
      hexToBytesStrict(encodedHex, "validators"),
      MAX_DOMAIN_CODEC_FIELD_BYTES);
  readHeader(reader, "Validators");
  core::ValidatorRegistry validators = readValidators(reader);
  reader.requireFullyConsumed();
  if (encode(validators) != encodedHex) {
    throw std::runtime_error("Validators domain round-trip mismatch.");
  }
  return validators;
}

std::string ValidatorsDomainCodec::calculateRoot(
    const core::ValidatorRegistry &validators) {
  serialization::CanonicalWriter writer;
  writeValidators(writer, validators);
  return serialization::CanonicalHash::hashBytes(
      writer.bytes(), "NODO_VALIDATORS_DOMAIN_ROOT_V1");
}

bool ValidatorsDomainCodec::validateRoot(
    const core::ValidatorRegistry &validators,
    const std::string &expectedRoot) {
  return calculateRoot(validators) == expectedRoot;
}

// ---------------------------------------------------------------------------
// SlashingDomainCodec
// ---------------------------------------------------------------------------

std::string
SlashingDomainCodec::encode(const consensus::ValidatorPenaltyLedger &ledger) {
  serialization::CanonicalWriter writer;
  writeHeader(writer, "Slashing");
  writeSlashing(writer, ledger);
  return writer.hex();
}

consensus::ValidatorPenaltyLedger
SlashingDomainCodec::decode(const std::string &encodedHex) {
  serialization::CanonicalReader reader(
      hexToBytesStrict(encodedHex, "slashing"),
      MAX_DOMAIN_CODEC_FIELD_BYTES);
  readHeader(reader, "Slashing");
  consensus::ValidatorPenaltyLedger ledger = readSlashing(reader);
  reader.requireFullyConsumed();
  if (encode(ledger) != encodedHex) {
    throw std::runtime_error("Slashing domain round-trip mismatch.");
  }
  return ledger;
}

std::string SlashingDomainCodec::calculateRoot(
    const consensus::ValidatorPenaltyLedger &ledger) {
  serialization::CanonicalWriter writer;
  writeSlashing(writer, ledger);
  return serialization::CanonicalHash::hashBytes(
      writer.bytes(), "NODO_SLASHING_DOMAIN_ROOT_V1");
}

bool SlashingDomainCodec::validateRoot(
    const consensus::ValidatorPenaltyLedger &ledger,
    const std::string &expectedRoot) {
  return calculateRoot(ledger) == expectedRoot;
}

// ---------------------------------------------------------------------------
// ValidatorWeightsDomainCodec
// ---------------------------------------------------------------------------

std::string ValidatorWeightsDomainCodec::calculateRoot(
    const core::ValidatorRegistry &validators) {
  return core::StateRootCalculator::calculateValidatorStateRoot(validators);
}

bool ValidatorWeightsDomainCodec::validateRoot(
    const core::ValidatorRegistry &validators,
    const std::string &expectedRoot) {
  return calculateRoot(validators) == expectedRoot;
}

// ---------------------------------------------------------------------------
// ProtocolDomainCodec
// ---------------------------------------------------------------------------

std::map<std::string, std::string>
ProtocolDomainCodec::encodeState(const ProtocolExecutionState &state) {
  return {{"burns", BurnsDomainCodec::encode(state.burns)},
          {"governance", GovernanceDomainCodec::encode(state.governance)},
          {"slashing", SlashingDomainCodec::encode(state.penaltyLedger)},
          {"staking", StakingDomainCodec::encode(state.staking)},
          {"supply", SupplyDomainCodec::encode(state.supply)},
          {"validators", ValidatorsDomainCodec::encode(state.validators)},
          {"validator_weights",
           ValidatorWeightsDomainCodec::calculateRoot(state.validators)}};
}

ProtocolExecutionState ProtocolDomainCodec::decodeState(
    const std::map<std::string, std::string> &domains) {
  auto requireDomain = [&domains](const char *name) -> const std::string & {
    const auto found = domains.find(name);
    if (found == domains.end()) {
      throw std::runtime_error(std::string("Missing protocol domain: ") +
                               name);
    }
    return found->second;
  };

  ProtocolExecutionState state;
  state.validators = ValidatorsDomainCodec::decode(requireDomain("validators"));

  if (!ValidatorWeightsDomainCodec::validateRoot(
          state.validators, requireDomain("validator_weights"))) {
    throw std::runtime_error(
        "validator_weights root does not match the validators domain "
        "payload.");
  }

  state.governance = GovernanceDomainCodec::decode(requireDomain("governance"));
  state.penaltyLedger = SlashingDomainCodec::decode(requireDomain("slashing"));
  state.staking = StakingDomainCodec::decode(requireDomain("staking"));
  state.supply = SupplyDomainCodec::decode(requireDomain("supply"));
  state.burns = BurnsDomainCodec::decode(requireDomain("burns"));
  return state;
}

} // namespace nodo::node
