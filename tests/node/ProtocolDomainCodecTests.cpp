#include "consensus/ValidatorPenaltyApplication.hpp"
#include "core/TransactionPayload.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"
#include "economics/BurnRecord.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/ProtocolDomainCodec.hpp"
#include "node/StakingRegistry.hpp"
#include "utils/Amount.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;
constexpr std::int64_t kTimestamp = 1900000000;

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

// Truncating trailing bytes is a reliable corruption test: encode() packs
// fields tightly with no padding, so removing bytes from the end always
// starves whatever field CanonicalReader is mid-read on, guaranteeing a
// throw. Flipping an arbitrary bit is not reliable for this purpose — it may
// land inside a free-text field (e.g. governance title/description) and
// still decode to a different, but perfectly well-formed, value: canonical
// encoding has no checksum, so a still-valid-but-different payload is not
// "corruption" in a way the codec can (or should) detect on its own.
std::string truncateHex(const std::string &hex, std::size_t bytesToRemove) {
  const std::size_t charsToRemove = std::min(hex.size(), bytesToRemove * 2);
  return hex.substr(0, hex.size() - charsToRemove);
}

std::string deterministicValidatorAddress(const std::string &seed) {
  const crypto::KeyPair keyPair =
      crypto::KeyPair::createDeterministicBls12381KeyPair(seed);
  return crypto::AddressDerivation::deriveFromPublicKey(keyPair.publicKey())
      .value();
}

// ---------------------------------------------------------------------------
// supply
// ---------------------------------------------------------------------------

void testSupplyDomainRoundTrip() {
  const utils::Amount supply = utils::Amount::fromRawUnits(123456789);
  const std::string encoded = node::SupplyDomainCodec::encode(supply);
  const utils::Amount decoded = node::SupplyDomainCodec::decode(encoded);
  require(decoded == supply, "supply round-trip must preserve raw units");

  const std::string root = node::SupplyDomainCodec::calculateRoot(supply);
  require(!root.empty(), "supply root must not be empty");
  require(node::SupplyDomainCodec::validateRoot(supply, root),
          "supply root must self-validate");
  require(!node::SupplyDomainCodec::validateRoot(
              utils::Amount::fromRawUnits(1), root),
          "supply root must reject a different value");

  bool threw = false;
  try {
    node::SupplyDomainCodec::decode(truncateHex(encoded, 4));
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "corrupted supply payload must fail to decode");
}

// ---------------------------------------------------------------------------
// burns
// ---------------------------------------------------------------------------

void testBurnsDomainRoundTrip() {
  const std::vector<economics::BurnRecord> burns = {
      economics::BurnRecord("burn-b", 10, 1, "addr-1",
                            utils::Amount::fromRawUnits(500), "fee burn",
                            economics::BurnType::FEE_BURN),
      economics::BurnRecord("burn-a", 20, 2, "addr-2",
                            utils::Amount::fromRawUnits(750), "slash burn",
                            economics::BurnType::SLASH_BURN),
      economics::BurnRecord("burn-c", 30, 3, "addr-3",
                            utils::Amount::fromRawUnits(250),
                            "voluntary burn",
                            economics::BurnType::VOLUNTARY_BURN)};

  const std::string encoded = node::BurnsDomainCodec::encode(burns);
  const std::vector<economics::BurnRecord> decoded =
      node::BurnsDomainCodec::decode(encoded);
  require(decoded.size() == 3, "burns round-trip must preserve record count");
  require(decoded[0].burnId() == "burn-a" && decoded[1].burnId() == "burn-b" &&
              decoded[2].burnId() == "burn-c",
          "burns must be canonically ordered by burnId regardless of input "
          "order");
  require(decoded[1].burnType() == economics::BurnType::FEE_BURN &&
              decoded[1].amount().rawUnits() == 500,
          "decoded burn fields must match the original record");

  require(node::BurnsDomainCodec::validateRoot(
              burns, node::BurnsDomainCodec::calculateRoot(burns)),
          "burns root must self-validate");

  bool threw = false;
  try {
    node::BurnsDomainCodec::decode(truncateHex(encoded, 4));
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "corrupted burns payload must fail to decode");
}

// ---------------------------------------------------------------------------
// staking
// ---------------------------------------------------------------------------

node::StakingRegistry buildStakingFixture() {
  std::map<std::string, economics::StakeAccount> accounts;
  accounts.emplace("validator-1",
                   economics::StakeAccount("validator-1",
                                            utils::Amount::fromRawUnits(1850),
                                            utils::Amount::fromRawUnits(50),
                                            false, false));

  node::StakePositionView active;
  active.ownerAddress = "owner-1";
  active.validatorAddress = "validator-1";
  active.activeAmount = utils::Amount::fromRawUnits(1000);
  active.rewardsPending = utils::Amount::fromRawUnits(50);
  active.lockHeight = 10;
  active.activationHeight = 11;
  active.status = node::StakePositionStatus::ACTIVE;

  node::StakePositionView unbonding;
  unbonding.ownerAddress = "owner-2";
  unbonding.validatorAddress = "validator-1";
  unbonding.activeAmount = utils::Amount::fromRawUnits(500);
  unbonding.pendingActivationAmount = utils::Amount::fromRawUnits(200);
  unbonding.pendingUnbondingAmount = utils::Amount::fromRawUnits(100);
  unbonding.slashedAmount = utils::Amount::fromRawUnits(50);
  unbonding.lockHeight = 20;
  unbonding.activationHeight = 21;
  unbonding.unbondingStartHeight = 30;
  unbonding.withdrawableHeight = 51;
  unbonding.status = node::StakePositionStatus::UNBONDING;

  node::StakeLifecycleRecord record;
  record.recordId = "lifecycle-1";
  record.action = "deposit";
  record.transactionId = "tx-1";
  record.ownerAddress = "owner-1";
  record.validatorAddress = "validator-1";
  record.amount = utils::Amount::fromRawUnits(1000);
  record.activeAfter = utils::Amount::fromRawUnits(1000);
  record.blockHeight = 10;
  record.activationHeight = 11;
  record.reason = "initial deposit";

  return node::StakingRegistry::restore(
      std::move(accounts), {active, unbonding}, {record});
}

void testStakingDomainRoundTrip() {
  const node::StakingRegistry staking = buildStakingFixture();
  const std::string encoded = node::StakingDomainCodec::encode(staking);
  const node::StakingRegistry decoded =
      node::StakingDomainCodec::decode(encoded);

  require(node::StakingDomainCodec::encode(decoded) == encoded,
          "staking decode must re-encode byte-identically");
  require(decoded.accounts().size() == 1,
          "staking round-trip must preserve account count");
  require(decoded.lifecycleRecords().size() == 1,
          "staking round-trip must preserve lifecycle records");

  const std::vector<node::StakePositionView> positions = decoded.positions();
  require(positions.size() == 2,
          "staking round-trip must preserve every position");
  bool foundUnbonding = false;
  for (const auto &position : positions) {
    if (position.ownerAddress == "owner-2") {
      foundUnbonding = true;
      require(position.status == node::StakePositionStatus::UNBONDING,
              "position status must round-trip");
      require(position.pendingUnbondingAmount.rawUnits() == 100,
              "position pending-unbonding amount must round-trip");
      require(position.withdrawableHeight == 51,
              "position withdrawable height must round-trip");
    }
  }
  require(foundUnbonding, "owner-2 position must survive round-trip");

  require(node::StakingDomainCodec::validateRoot(
              staking, node::StakingDomainCodec::calculateRoot(staking)),
          "staking root must self-validate");

  bool threw = false;
  try {
    node::StakingDomainCodec::decode(truncateHex(encoded, 4));
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "corrupted staking payload must fail to decode");
}

// ---------------------------------------------------------------------------
// governance
// ---------------------------------------------------------------------------

node::GovernanceExecutor buildGovernanceFixture() {
  std::vector<node::GovernanceParameterChange> applied = {
      node::GovernanceParameterChange(
          "proposal-applied", node::GovernanceParameterTarget::MINIMUM_FEE_RAW,
          "100", "250", 500, kTimestamp)};
  std::vector<node::GovernanceParameterChange> pending = {
      node::GovernanceParameterChange::pending(
          "proposal-pending",
          node::GovernanceParameterTarget::MINIMUM_VALIDATOR_COUNT, "5",
          900)};

  node::GovernanceExecutor::ProposalRecord parameterProposal;
  parameterProposal.proposalId = "proposal-executed";
  parameterProposal.proposerAddress = "proposer-1";
  parameterProposal.payload = core::GovernanceProposalPayload::parameterChange(
      "Lower fee", "Reduce the minimum transaction fee", "MINIMUM_FEE_RAW",
      "250", 500);
  parameterProposal.createdHeight = 100;
  parameterProposal.createdAt = kTimestamp;
  parameterProposal.votingStartHeight = 101;
  parameterProposal.votingEndHeight = 200;
  parameterProposal.totalEligibleWeight = 1000;
  parameterProposal.status = node::GovernanceProposalStatus::EXECUTED;
  parameterProposal.finalTally = node::GovernanceTallySnapshot(
      "proposal-executed", 700, 100, 50, 850, 1000, true, true);
  parameterProposal.decidedAtHeight = 200;
  parameterProposal.decidedAt = kTimestamp + 200;
  parameterProposal.executedAtHeight = 201;
  parameterProposal.executedAt = kTimestamp + 201;
  parameterProposal.executionDetail = "applied at height 201";
  parameterProposal.votes = {
      {"validator-a", core::GovernanceVoteChoice::YES, 400, 150,
       kTimestamp + 150, "tx-vote-a"},
      {"validator-b", core::GovernanceVoteChoice::NO, 100, 160,
       kTimestamp + 160, "tx-vote-b"},
      {"validator-c", core::GovernanceVoteChoice::ABSTAIN, 50, 170,
       kTimestamp + 170, "tx-vote-c"}};

  node::GovernanceExecutor::ProposalRecord treasuryProposal;
  treasuryProposal.proposalId = "proposal-treasury";
  treasuryProposal.proposerAddress = "proposer-2";
  treasuryProposal.payload = core::GovernanceProposalPayload::treasurySpend(
      "Fund grant", "Pay a community grant", "recipient-address", 5000);
  treasuryProposal.createdHeight = 300;
  treasuryProposal.createdAt = kTimestamp + 300;
  treasuryProposal.votingStartHeight = 301;
  treasuryProposal.votingEndHeight = 400;
  treasuryProposal.totalEligibleWeight = 1000;
  treasuryProposal.status = node::GovernanceProposalStatus::QUEUED_FOR_EXECUTION;
  treasuryProposal.finalTally = node::GovernanceTallySnapshot(
      "proposal-treasury", 900, 0, 0, 900, 1000, true, true);
  treasuryProposal.decidedAtHeight = 400;
  treasuryProposal.decidedAt = kTimestamp + 400;
  treasuryProposal.treasuryExecutableAtHeight = 500;
  treasuryProposal.treasuryBalanceBeforeExecution =
      utils::Amount::fromRawUnits(1'000'000);
  treasuryProposal.votes = {{"validator-a", core::GovernanceVoteChoice::YES,
                             900, 350, kTimestamp + 350, "tx-vote-d"}};

  return node::GovernanceExecutor::restore(
      std::move(applied), std::move(pending),
      {parameterProposal, treasuryProposal});
}

void testGovernanceDomainRoundTrip() {
  const node::GovernanceExecutor governance = buildGovernanceFixture();
  const std::string encoded = node::GovernanceDomainCodec::encode(governance);
  const node::GovernanceExecutor decoded =
      node::GovernanceDomainCodec::decode(encoded);

  require(node::GovernanceDomainCodec::encode(decoded) == encoded,
          "governance decode must re-encode byte-identically");
  require(decoded.appliedChanges().size() == 1,
          "governance round-trip must preserve applied changes");
  require(decoded.pendingChanges().size() == 1,
          "governance round-trip must preserve pending changes");

  const std::vector<node::GovernanceExecutor::ProposalRecord> proposals =
      decoded.allProposalRecords();
  require(proposals.size() == 2,
          "governance round-trip must preserve every proposal");

  bool foundExecuted = false;
  bool foundTreasury = false;
  for (const auto &proposal : proposals) {
    if (proposal.proposalId == "proposal-executed") {
      foundExecuted = true;
      require(proposal.executionDetail == "applied at height 201",
              "executionDetail must survive round-trip (dropped by the old "
              "text serialize())");
      require(proposal.executedAt == kTimestamp + 201,
              "executedAt must survive round-trip (dropped by the old text "
              "serialize())");
      require(proposal.votes.size() == 3,
              "every vote must survive round-trip");
      for (const auto &vote : proposal.votes) {
        if (vote.validatorAddress == "validator-c") {
          require(vote.choice == core::GovernanceVoteChoice::ABSTAIN,
                  "vote choice must round-trip");
          require(vote.castAt == kTimestamp + 170,
                  "vote castAt must survive round-trip (dropped by the old "
                  "text serialize())");
        }
      }
    }
    if (proposal.proposalId == "proposal-treasury") {
      foundTreasury = true;
      require(proposal.treasuryExecutableAtHeight == 500,
              "treasuryExecutableAtHeight must survive round-trip (absent "
              "from the old text serialize())");
      require(proposal.treasuryBalanceBeforeExecution.rawUnits() == 1'000'000,
              "treasuryBalanceBeforeExecution must survive round-trip");
    }
  }
  require(foundExecuted && foundTreasury,
          "both proposals must be present after round-trip");

  require(node::GovernanceDomainCodec::validateRoot(
              governance, node::GovernanceDomainCodec::calculateRoot(governance)),
          "governance root must self-validate");

  bool threw = false;
  try {
    node::GovernanceDomainCodec::decode(truncateHex(encoded, 4));
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "corrupted governance payload must fail to decode");
}

// ---------------------------------------------------------------------------
// validators
// ---------------------------------------------------------------------------

core::ValidatorRegistry buildValidatorsFixture() {
  core::ValidatorRegistry registry;

  struct Fixture {
    std::string seed;
    core::ValidatorRegistrationStatus status;
    std::uint64_t stakeAmount;
    std::uint64_t jailUntilEpoch;
    std::uint64_t exitRequestHeight;
    std::string owner;
  };
  const std::vector<Fixture> fixtures = {
      {"codec-validator-active", core::ValidatorRegistrationStatus::ACTIVE,
       2'000'000, 0, 0, "owner-a"},
      {"codec-validator-jailed", core::ValidatorRegistrationStatus::JAILED,
       1'500'000, 5, 0, "owner-b"},
      {"codec-validator-exit", core::ValidatorRegistrationStatus::EXIT_REQUESTED,
       1'200'000, 0, 42, "owner-c"},
      {"codec-validator-pending",
       core::ValidatorRegistrationStatus::PENDING_ACTIVATION, 1'000'000, 0, 0,
       "owner-d"}};

  for (const Fixture &fixture : fixtures) {
    const crypto::KeyPair keyPair =
        crypto::KeyPair::createDeterministicBls12381KeyPair(fixture.seed);
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(keyPair.publicKey())
            .value();
    const core::ValidatorRegistrationRecord record(
        address, keyPair.publicKey(), 1, "metadata-" + fixture.seed,
        kTimestamp);
    const core::ValidatorRegistryEntry entry(
        record, fixture.status, kTimestamp + 1, fixture.stakeAmount,
        fixture.jailUntilEpoch, fixture.exitRequestHeight, fixture.owner);
    require(registry.restoreEntry(entry),
            "validators fixture restoreEntry failed for " + fixture.seed);
  }
  return registry;
}

void testValidatorsDomainRoundTrip() {
  const core::ValidatorRegistry validators = buildValidatorsFixture();
  const std::string encoded = node::ValidatorsDomainCodec::encode(validators);
  const core::ValidatorRegistry decoded =
      node::ValidatorsDomainCodec::decode(encoded);

  require(node::ValidatorsDomainCodec::encode(decoded) == encoded,
          "validators decode must re-encode byte-identically");
  require(decoded.size() == 4,
          "validators round-trip must preserve every entry");
  require(decoded.jailedValidatorAddresses().size() == 1,
          "jailed status must round-trip");
  require(decoded.exitRequestedValidatorAddresses().size() == 1,
          "exit-requested status must round-trip");
  require(decoded.pendingValidatorAddresses().size() == 1,
          "pending-activation status must round-trip");
  require(decoded.activeValidatorAddresses().size() == 1,
          "active status must round-trip");

  require(node::ValidatorsDomainCodec::validateRoot(
              validators,
              node::ValidatorsDomainCodec::calculateRoot(validators)),
          "validators root must self-validate");

  bool threw = false;
  try {
    node::ValidatorsDomainCodec::decode(truncateHex(encoded, 4));
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "corrupted validators payload must fail to decode");
}

// ---------------------------------------------------------------------------
// slashing
// ---------------------------------------------------------------------------

consensus::ValidatorPenaltyLedger buildSlashingFixture() {
  // ValidatorPenaltyDecision::isValid() requires penaltyId to equal the
  // content-derived hash computed internally by
  // ValidatorPenaltyDecision::create(evidence, policy, ...) (see
  // ValidatorPenaltyApplication.cpp's computePenaltyId, not exposed
  // publicly), so fixtures must go through that factory rather than the raw
  // constructor with an arbitrary id.
  consensus::ValidatorPenaltyLedger ledger;
  const consensus::ValidatorPenaltyPolicy policy(3000, 5000, 20, false);

  const consensus::SlashingEvidenceRecord warningEvidence(
      "evidence-warning", consensus::SlashingEvidenceType::INVALID_SIGNATURE,
      deterministicValidatorAddress("codec-slashing-warned"), "payload-hash-1",
      consensus::SlashingEvidenceSeverity::WARNING, kTimestamp);
  const consensus::ValidatorPenaltyDecision warningDecision =
      consensus::ValidatorPenaltyDecision::create(warningEvidence, policy,
                                                  kTimestamp);
  require(ledger.applyDecision(warningDecision).applied(),
          "slashing fixture warning decision must apply");

  const consensus::SlashingEvidenceRecord slashEvidence(
      "evidence-slash", consensus::SlashingEvidenceType::DOUBLE_VOTE,
      deterministicValidatorAddress("codec-slashing-slashed"),
      "payload-hash-2", consensus::SlashingEvidenceSeverity::SLASHABLE,
      kTimestamp + 1);
  const consensus::ValidatorPenaltyDecision slashDecision =
      consensus::ValidatorPenaltyDecision::create(slashEvidence, policy,
                                                  kTimestamp + 1);
  require(ledger.applyDecision(slashDecision).applied(),
          "slashing fixture slash decision must apply");

  return ledger;
}

void testSlashingDomainRoundTrip() {
  const consensus::ValidatorPenaltyLedger ledger = buildSlashingFixture();
  const std::vector<consensus::ValidatorPenaltyDecision> originalDecisions =
      ledger.allDecisions();
  require(originalDecisions.size() == 2,
          "slashing fixture must contain two decisions");

  const std::string encoded = node::SlashingDomainCodec::encode(ledger);
  const consensus::ValidatorPenaltyLedger decoded =
      node::SlashingDomainCodec::decode(encoded);

  require(node::SlashingDomainCodec::encode(decoded) == encoded,
          "slashing decode must re-encode byte-identically");
  require(decoded.size() == 2,
          "slashing round-trip must preserve every decision");

  for (const consensus::ValidatorPenaltyDecision &original :
       originalDecisions) {
    const consensus::ValidatorPenaltyDecision *found =
        decoded.decisionByPenaltyId(original.penaltyId());
    require(found != nullptr &&
                found->slashAmountRawUnits() == original.slashAmountRawUnits() &&
                found->jailEpochs() == original.jailEpochs() &&
                found->action() == original.action() &&
                found->validatorAddress() == original.validatorAddress(),
            "decoded decision fields must match the original decision");
  }

  require(node::SlashingDomainCodec::validateRoot(
              ledger, node::SlashingDomainCodec::calculateRoot(ledger)),
          "slashing root must self-validate");

  bool threw = false;
  try {
    node::SlashingDomainCodec::decode(truncateHex(encoded, 4));
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "corrupted slashing payload must fail to decode");
}

// ---------------------------------------------------------------------------
// full ProtocolExecutionState + validator_weights cross-validation
// ---------------------------------------------------------------------------

void testProtocolDomainCodecFullStateRoundTrip() {
  node::ProtocolExecutionState state;
  state.validators = buildValidatorsFixture();
  state.staking = buildStakingFixture();
  state.governance = buildGovernanceFixture();
  state.penaltyLedger = buildSlashingFixture();
  state.supply = utils::Amount::fromRawUnits(987654321);
  state.burns = {economics::BurnRecord(
      "full-state-burn", 5, 0, "addr-full",
      utils::Amount::fromRawUnits(42), "fee burn",
      economics::BurnType::FEE_BURN)};

  const std::map<std::string, std::string> domains =
      node::ProtocolDomainCodec::encodeState(state);
  require(domains.size() == 7,
          "encoded state must carry exactly the 7 known domains");

  const node::ProtocolExecutionState decoded =
      node::ProtocolDomainCodec::decodeState(domains);
  const std::map<std::string, std::string> reEncoded =
      node::ProtocolDomainCodec::encodeState(decoded);
  require(reEncoded == domains,
          "decoding then re-encoding the full protocol state must be "
          "byte-identical");

  // Tamper with validator_weights only: decodeState must reject it even
  // though every individual domain still decodes fine on its own.
  std::map<std::string, std::string> tampered = domains;
  tampered["validator_weights"] =
      node::ValidatorWeightsDomainCodec::calculateRoot(core::ValidatorRegistry());
  bool threw = false;
  try {
    node::ProtocolDomainCodec::decodeState(tampered);
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw,
          "decodeState must reject a validator_weights root that does not "
          "match the validators payload");

  std::map<std::string, std::string> missingDomain = domains;
  missingDomain.erase("staking");
  threw = false;
  try {
    node::ProtocolDomainCodec::decodeState(missingDomain);
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "decodeState must reject a missing domain");
}

} // namespace

int main() {
  try {
    testSupplyDomainRoundTrip();
    testBurnsDomainRoundTrip();
    testStakingDomainRoundTrip();
    testGovernanceDomainRoundTrip();
    testValidatorsDomainRoundTrip();
    testSlashingDomainRoundTrip();
    testProtocolDomainCodecFullStateRoundTrip();
    std::cout << "Protocol domain codec tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Protocol domain codec tests failed: " << error.what()
              << '\n';
    return 1;
  }
}
