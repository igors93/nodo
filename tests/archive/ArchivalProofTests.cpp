#include "ArchiveTestSupport.hpp"

#include "archive/ArchivalScoreLedger.hpp"
#include "archive/ArchiveAssignment.hpp"

#include <algorithm>
#include <iostream>

using nodo::test::require;
using nodo::test::SyntheticHistory;
using namespace nodo;
using namespace nodo::archive;

namespace {

ArchivalProof prove(const SyntheticHistory &history, const ArchivalChallenge &challenge,
                    const std::string &keyName,
                    std::set<std::uint64_t> missing = {},
                    std::uint64_t submittedHeight = 0) {
  return ArchivalProof::build(
      challenge, history.segment(challenge.fields().segmentIndex),
      history.source(std::move(missing)),
      submittedHeight == 0 ? challenge.fields().issueHeight : submittedHeight,
      SyntheticHistory::key(keyName), SyntheticHistory::kSignedAt);
}

void requireStatus(const ArchivalProofVerdict &verdict, ArchivalProofStatus status,
                   const std::string &what) {
  require(verdict.status == status,
          what + ": got " + archivalProofStatusToString(verdict.status) + " (" +
              verdict.reason + ")");
}

void testChallengeDeterminism() {
  const SyntheticHistory history(16);
  ArchiveProviderRegistry registry(SyntheticHistory::kChainId);
  const std::string a = history.addProvider(registry, "a", "operator-a");
  const std::string b = history.addProvider(registry, "b", "operator-b");
  const std::uint64_t round = history.firstRoundFor(1);
  const ArchivalChallenge first = history.challenge(a, 1, round);
  const ArchivalChallenge again = history.challenge(a, 1, round);
  require(first.encode() == again.encode() &&
              first.challengeId() == again.challengeId(),
          "the same finalized inputs give the same challenge");
  const auto &samples = first.fields().sampleIndices;
  require(samples.size() == history.parameters().archiveChallengeSamples(),
          "challenge asks for k samples");
  require(std::set<std::uint64_t>(samples.begin(), samples.end()).size() ==
                  samples.size() &&
              std::all_of(samples.begin(), samples.end(),
                          [&](std::uint64_t index) {
                            return index < first.fields().pieceCount;
                          }),
          "samples are distinct pieces of the segment");
  require(history.challenge(b, 1, round).fields().sampleIndices != samples ||
              history.challenge(b, 1, round).challengeId() != first.challengeId(),
          "each provider gets its own challenge");
  require(history.challenge(a, 1, round + 1).challengeId() != first.challengeId(),
          "each round gets a fresh challenge");

  const ArchivalChallenge decoded = ArchivalChallenge::decode(first.encode());
  require(decoded.challengeId() == first.challengeId(), "challenge round-trips");

  // A challenger cannot pick easy pieces: samples must be the derived ones.
  ArchivalChallengeFields forged = first.fields();
  forged.sampleIndices = {0, 1, 2, 3};
  require(forged.sampleIndices == samples ||
              !ArchivalChallenge(forged).isStructurallyValid(),
          "chosen samples are rejected");

  // A segment cannot be challenged before it is sealed.
  bool early = false;
  try {
    (void)history.challenge(a, 3, 0);
  } catch (const std::invalid_argument &) {
    early = true;
  }
  require(early, "an unsealed segment is never challenged");

  require(ArchivalChallenge::sampleIndices(std::string(64, 'a'), 3, 8) ==
              std::vector<std::uint64_t>({0, 1, 2}),
          "small segments are sampled completely");
}

void testProofValidation() {
  const SyntheticHistory history(16);
  ArchiveProviderRegistry registry(SyntheticHistory::kChainId);
  const std::string providerId = history.addProvider(registry, "a", "operator-a");
  const std::string otherProvider =
      history.addProvider(registry, "b", "operator-b");
  const ArchiveSegmentCommitment segment = history.segment(1).commitment();
  const ArchivalChallenge challenge =
      history.challenge(providerId, 1, history.firstRoundFor(1));
  const ArchivalReplayGuard guard;
  const auto &params = history.parameters();

  const ArchivalProof honest = prove(history, challenge, "a");
  requireStatus(ArchivalProofVerifier::verify(params, challenge, segment, registry,
                                              honest, guard, honest.submittedHeight()),
                ArchivalProofStatus::ACCEPTED, "honest proof");
  require(ArchivalProof::decode(honest.encode(), params.maxArchivalProofBytes())
                  .proofId() == honest.proofId(),
          "proof round-trips");
  require(honest.encode().size() < 64 * 1024,
          "verification data is tiny compared with the segment");

  // Wrong data signed by the provider: fraud.
  std::vector<ArchivalSample> samples = honest.samples();
  samples[1].piece[0] ^= 0x01;
  const ArchivalProof wrongData = ArchivalProof::signedWith(
      ArchivalProof(honest.challengeId(), honest.providerId(),
                    honest.segmentIndex(), honest.round(),
                    honest.submittedHeight(), samples, crypto::Signature()),
      SyntheticHistory::key("a"), SyntheticHistory::kSignedAt);
  const ArchivalProofVerdict fraud = ArchivalProofVerifier::verify(
      params, challenge, segment, registry, wrongData, guard,
      wrongData.submittedHeight());
  requireStatus(fraud, ArchivalProofStatus::WRONG_DATA, "tampered piece");
  require(fraud.isFraud(), "a signed wrong answer is fraud");
  const ArchivalFaultEvidence evidence(challenge, wrongData,
                                      wrongData.submittedHeight());
  require(evidence.verify(params, segment, registry,
                          wrongData.submittedHeight()),
          "fraud evidence is independently verifiable");
  require(!ArchivalFaultEvidence(challenge, honest, honest.submittedHeight())
               .verify(params, segment, registry, honest.submittedHeight()),
          "an honest proof is never evidence");
  require(!evidence.verify(params, segment, registry,
                           challenge.fields().deadlineHeight + 1),
          "fraud evidence cannot be reused at a different inclusion height");

  // Swapped sample order: wrong pieces.
  samples = honest.samples();
  std::swap(samples[0], samples[1]);
  requireStatus(ArchivalProofVerifier::verify(
                    params, challenge, segment, registry,
                    ArchivalProof::signedWith(
                        ArchivalProof(honest.challengeId(), honest.providerId(),
                                      honest.segmentIndex(), honest.round(),
                                      honest.submittedHeight(), samples,
                                      crypto::Signature()),
                        SyntheticHistory::key("a"), SyntheticHistory::kSignedAt),
                    guard, honest.submittedHeight()),
                ArchivalProofStatus::WRONG_SAMPLES, "unchallenged pieces");

  // Signed by someone else: not attributable, so not fraud.
  const ArchivalProof stolen = ArchivalProof::signedWith(
      honest, SyntheticHistory::key("b"), SyntheticHistory::kSignedAt);
  const ArchivalProofVerdict badSignature = ArchivalProofVerifier::verify(
      params, challenge, segment, registry, stolen, guard,
      stolen.submittedHeight());
  requireStatus(badSignature, ArchivalProofStatus::BAD_SIGNATURE,
                "foreign signature");
  require(!badSignature.isFraud(), "an unattributable message is never fraud");

  // Late proof.
  requireStatus(ArchivalProofVerifier::verify(
                    params, challenge, segment, registry,
                    prove(history, challenge, "a", {},
                          challenge.fields().deadlineHeight + 1),
                    guard, challenge.fields().deadlineHeight + 1),
                ArchivalProofStatus::EXPIRED, "after the deadline");
  requireStatus(ArchivalProofVerifier::verify(
                    params, challenge, segment, registry, honest, guard,
                    challenge.fields().deadlineHeight + 1),
                ArchivalProofStatus::EXPIRED,
                "a late finalized proof cannot backdate its signed height");

  // Proof for another provider's challenge.
  const ArchivalChallenge other =
      history.challenge(otherProvider, 1, history.firstRoundFor(1));
  requireStatus(ArchivalProofVerifier::verify(params, other, segment, registry,
                                              honest, guard, honest.submittedHeight()),
                ArchivalProofStatus::WRONG_CHALLENGE, "someone else's challenge");

  // Unknown and inactive providers.
  ArchiveProviderRegistry empty(SyntheticHistory::kChainId);
  requireStatus(ArchivalProofVerifier::verify(params, challenge, segment, empty,
                                              honest, guard, honest.submittedHeight()),
                ArchivalProofStatus::PROVIDER_UNKNOWN, "unregistered");
  ArchiveProviderRegistry removed = registry;
  removed.remove(providerId, 9, "test");
  requireStatus(ArchivalProofVerifier::verify(params, challenge, segment, removed,
                                              honest, guard, honest.submittedHeight()),
                ArchivalProofStatus::PROVIDER_INACTIVE, "removed provider");

  // Oversized proof.
  config::HistoryParameterValues tight = params.values();
  tight.maxArchivalProofBytes = 1;
  requireStatus(ArchivalProofVerifier::verify(config::HistoryParameters(tight),
                                              challenge, segment, registry,
                                              honest, guard, honest.submittedHeight()),
                ArchivalProofStatus::OVERSIZED, "proof above the cap");
}

void testReplayIsRejected() {
  const SyntheticHistory history(16);
  ArchiveProviderRegistry registry(SyntheticHistory::kChainId);
  const std::string providerId = history.addProvider(registry, "a", "operator-a");
  const auto segments = history.commitments(4);
  ArchivalScoreLedger ledger(history.parameters());
  const std::uint64_t round = history.firstRoundFor(1);
  const ArchivalChallenge challenge = history.challenge(providerId, 1, round);
  require(ledger.issue(challenge) && !ledger.issue(challenge),
          "a challenge is issued once");
  const ArchivalProof proof = prove(history, challenge, "a");
  requireStatus(ledger.submit(proof, segments, registry, proof.submittedHeight()),
                ArchivalProofStatus::ACCEPTED, "first submission");
  requireStatus(ledger.submit(proof, segments, registry, proof.submittedHeight()),
                ArchivalProofStatus::DUPLICATE, "replayed submission");

  // Last round's proof cannot answer this round.
  const ArchivalChallenge next = history.challenge(providerId, 1, round + 1);
  require(ledger.issue(next), "next round issued");
  const ArchivalProofVerdict replay =
      ledger.submit(proof, segments, registry, proof.submittedHeight());
  require(replay.status == ArchivalProofStatus::DUPLICATE &&
              ledger.outcome(next.challengeId()) == ChallengeOutcome::PENDING,
          "an old proof never satisfies a new challenge");
  require(ledger.passedCount() == 1, "only one challenge passed");
}

void testMissingDataAndFraudOutcomes() {
  const SyntheticHistory history(16);
  ArchiveProviderRegistry registry(SyntheticHistory::kChainId);
  const std::string honest = history.addProvider(registry, "honest", "operator-1");
  const std::string lazy = history.addProvider(registry, "lazy", "operator-2");
  const std::string cheat = history.addProvider(registry, "cheat", "operator-3");
  const auto segments = history.commitments(4);
  const auto &params = history.parameters();
  ArchivalScoreLedger ledger(params);

  std::uint64_t epochLast = 0;
  for (std::uint32_t epoch = 1; epoch <= params.archiveRemovalFailedEpochs(); ++epoch) {
    const std::uint64_t round = history.firstRoundFor(1) + epoch - 1;
    const ArchivalChallenge forHonest = history.challenge(honest, 1, round);
    const ArchivalChallenge forLazy = history.challenge(lazy, 1, round);
    ledger.issue(forHonest);
    ledger.issue(forLazy);
    requireStatus(ledger.submit(prove(history, forHonest, "honest"), segments,
                                registry, forHonest.fields().issueHeight),
                  ArchivalProofStatus::ACCEPTED, "honest provider");
    // The lazy provider dropped block 6: it cannot build a proof at all.
    bool cannotProve = false;
    try {
      (void)prove(history, forLazy, "lazy", {5, 6, 7, 8});
    } catch (const std::runtime_error &) {
      cannotProve = true;
    }
    require(cannotProve, "a provider without the data cannot answer");
    if (epoch == 1) {
      const ArchivalChallenge forCheat = history.challenge(cheat, 1, round);
      ledger.issue(forCheat);
      ArchivalProof real = prove(history, forCheat, "cheat");
      std::vector<ArchivalSample> samples = real.samples();
      samples.front().piece.back() ^= 0x20;
      const ArchivalProof forged = ArchivalProof::signedWith(
          ArchivalProof(real.challengeId(), real.providerId(), real.segmentIndex(),
                        real.round(), real.submittedHeight(), samples,
                        crypto::Signature()),
          SyntheticHistory::key("cheat"), SyntheticHistory::kSignedAt);
      require(ledger.submit(forged, segments, registry,
                            forged.submittedHeight()).isFraud(),
              "a forged piece is recorded as fraud");
    }
    const std::uint64_t first = params.challengeIssueHeight(round);
    epochLast = first;
    ledger.expire(params.challengeDeadlineHeight(round) + 1);
    require(ledger.outcome(forLazy.challengeId()) == ChallengeOutcome::MISSED,
            "an unanswered challenge expires as MISSED");
    const ArchivalEpochSummary summary =
        ledger.closeEpoch(epoch, first, epochLast, registry);
    const auto streak = registry.find(honest)->successfulEpochStreak;
    const auto failures = registry.find(lazy)->consecutiveFailedEpochs;
    const std::string closedDigest = ledger.digest();
    require(ledger.closeEpoch(epoch, first, epochLast, registry).digest() ==
                summary.digest() &&
                ledger.digest() == closedDigest &&
                registry.find(honest)->successfulEpochStreak == streak &&
                registry.find(lazy)->consecutiveFailedEpochs == failures,
            "closing the same epoch again cannot apply score changes twice");
    for (const ArchivalProviderEpochResult &result : summary.providers) {
      if (result.providerId == honest) {
        require(!result.failedEpoch && result.availabilityBasisPoints == 10000,
                "the honest provider is fully available");
      }
      if (result.providerId == lazy) {
        require(result.failedEpoch && result.availabilityBasisPoints == 0,
                "the lazy provider fails the epoch");
        require(result.removed == (epoch == params.archiveRemovalFailedEpochs()),
                "persistent failure removes the provider only at the limit");
      }
      if (result.providerId == cheat) {
        require(result.fraud && result.removed, "fraud removes immediately");
      }
    }
    if (epoch == 1) {
      require(summary.faultEvidenceIds.size() == 1,
              "only the signed wrong answer produces evidence");
      require(summary.provenReplicas(1, params.archiveMinAvailabilityBasisPoints()) ==
                  1,
              "only the honest operator counts as a proven replica");
    }
  }
  require(registry.find(lazy)->status == ArchiveProviderStatus::REMOVED &&
              registry.find(cheat)->status == ArchiveProviderStatus::REMOVED &&
              registry.find(honest)->status == ArchiveProviderStatus::ACTIVE,
          "removal follows the rules");
  require(ledger.faultEvidence().size() == 1,
          "a missed challenge is never penalty evidence");
  require(ledger.reliabilityBasisPoints(registry.find(honest)->successfulEpochStreak) >
              ledger.reliabilityBasisPoints(0),
          "consistent providers gain reliability");
}

void testEpochSummaryDigestCommitsAllAccounting() {
  ArchivalEpochSummary summary;
  summary.epoch = 1;
  summary.firstHeight = 1;
  summary.lastHeight = 4;
  summary.providers.push_back({"provider", "operator", 1, 1, 10000,
                               false, false, 10000, false});
  summary.challenges = 1;
  summary.passed = 1;
  const std::string original = summary.digest();
  summary.providers.front().operatorId = "another-operator";
  require(summary.digest() != original,
          "epoch digest commits the economic operator identity");
  summary.providers.front().operatorId = "operator";
  summary.providers.front().issued = 2;
  require(summary.digest() != original,
          "epoch digest commits provider challenge accounting");
  summary.providers.front().issued = 1;
  ++summary.challenges;
  require(summary.digest() != original,
          "epoch digest commits aggregate challenge accounting");
}

void testAssignmentAndSybilResistance() {
  const SyntheticHistory history(24);
  const auto &params = history.parameters();
  ArchiveProviderRegistry registry(SyntheticHistory::kChainId);
  // One operator spinning up many identities.
  for (int index = 0; index < 10; ++index) {
    history.addProvider(registry, "sybil-" + std::to_string(index), "operator-sybil");
  }
  history.addProvider(registry, "independent-1", "operator-1");
  history.addProvider(registry, "independent-2", "operator-2");
  const std::string small =
      history.addProvider(registry, "small", "operator-3", /*slots=*/1);
  const auto segments = history.commitments(6);
  const std::vector<ArchiveSlot> slots =
      ArchiveAssignmentPlanner::plan(params, segments, registry);
  const std::vector<ArchiveSlot> replanned =
      ArchiveAssignmentPlanner::plan(params, segments, registry);
  require(replanned.size() == slots.size() &&
              std::equal(slots.begin(), slots.end(), replanned.begin(),
                         [](const ArchiveSlot &left, const ArchiveSlot &right) {
                           return left.segmentIndex == right.segmentIndex &&
                                  left.replica == right.replica &&
                                  left.providerId == right.providerId;
                         }),
          "planning is a pure function");
  std::map<std::uint64_t, std::set<std::string>> operatorsBySegment;
  std::map<std::uint64_t, std::size_t> slotsBySegment;
  for (const ArchiveSlot &slot : slots) {
    const std::string op =
        registry.find(slot.providerId)->registration.fields().operatorId;
    require(operatorsBySegment[slot.segmentIndex].insert(op).second,
            "an operator never holds two replicas of one segment");
    ++slotsBySegment[slot.segmentIndex];
  }
  for (const auto &segment : segments) {
    require(slotsBySegment[segment.segmentIndex()] ==
                params.archiveReplicationTarget(),
            "every segment gets the target replication");
  }
  require(ArchiveAssignmentPlanner::slotsFor(slots, small).size() <= 1,
          "slots are bounded by bond");

  // Stability: a newcomer moves at most the slots it wins.
  const std::string newcomer =
      history.addProvider(registry, "newcomer", "operator-4");
  const std::vector<ArchiveSlot> after =
      ArchiveAssignmentPlanner::plan(params, segments, registry);
  std::size_t moved = 0;
  for (std::size_t index = 0; index < std::min(slots.size(), after.size()); ++index) {
    if (slots[index].providerId != after[index].providerId) {
      ++moved;
    }
  }
  require(after.size() == slots.size() && moved * 2 <= slots.size(),
          "rendezvous assignment keeps most slots when a provider joins");
  require(!ArchiveAssignmentPlanner::slotsFor(after, newcomer).empty() ||
              moved == 0,
          "only the newcomer's wins move data");

  // Replication report counts proven operators, not identities.
  ArchivalEpochSummary summary;
  for (const ArchiveSlot &slot : slots) {
    ArchivalSlotTally tally;
    tally.providerId = slot.providerId;
    tally.operatorId =
        registry.find(slot.providerId)->registration.fields().operatorId;
    tally.segmentIndex = slot.segmentIndex;
    tally.issued = 1;
    tally.passed = slot.segmentIndex == 0 ? 0 : 1;
    summary.slots.push_back(tally);
  }
  ArchivalSlotTally unassigned;
  unassigned.providerId = "unassigned";
  unassigned.operatorId = "unassigned-operator";
  unassigned.segmentIndex = 0;
  unassigned.issued = 1;
  unassigned.passed = 1;
  summary.slots.push_back(unassigned);
  const ArchiveReplicationReport report =
      ArchiveReplicationReport::build(params, segments, slots, &summary);
  require(report.segments.front().provenReplicas == 0 &&
              report.segments.front().underReplicated &&
              report.segments.front().critical,
          "a segment nobody proved is critical");
  require(report.segments.back().provenReplicas == params.archiveReplicationTarget() &&
              !report.segments.back().underReplicated,
          "a fully proven segment is healthy");
  require(report.minProvenReplicas == 0 &&
              report.underReplicatedSegments == std::vector<std::uint64_t>({0}),
          "under-replicated segments are listed");
  std::uint64_t historyBytes = 0;
  for (const auto &segment : segments) {
    historyBytes += segment.totalBytes();
  }
  require(report.historyBytes == historyBytes &&
              report.targetReplicaBytes ==
                  historyBytes * params.archiveReplicationTarget(),
          "byte accounting covers the whole history");
}

void testRegistrationRules() {
  const SyntheticHistory history(4);
  ArchiveProviderRegistry registry(SyntheticHistory::kChainId);
  ArchiveProviderRegistrationFields fields;
  fields.chainId = SyntheticHistory::kChainId;
  fields.operatorId = "operator";
  fields.bondRawUnits = history.parameters().archiveMinBondPerSlotRawUnits() - 1;
  fields.declaredCapacityBytes = 1 << 20;
  fields.registeredHeight = 1;
  require(registry.registerProvider(
              ArchiveProviderRegistration::sign(fields, SyntheticHistory::key("p"),
                                                SyntheticHistory::kSignedAt),
              history.parameters()) == ArchiveRegistrationStatus::INSUFFICIENT_BOND,
          "a bond below one slot is refused");
  fields.bondRawUnits = history.parameters().archiveMinBondPerSlotRawUnits();
  ArchiveProviderRegistration registration = ArchiveProviderRegistration::sign(
      fields, SyntheticHistory::key("p"), SyntheticHistory::kSignedAt);
  ArchiveProviderRegistrationFields stolen = registration.fields();
  stolen.operatorId = "someone-else";
  require(registry.registerProvider(
              ArchiveProviderRegistration(stolen, registration.signature()),
              history.parameters()) == ArchiveRegistrationStatus::BAD_SIGNATURE,
          "an altered registration fails its signature");
  require(registry.registerProvider(registration, history.parameters()) ==
              ArchiveRegistrationStatus::REGISTERED,
          "a bonded, signed registration is accepted");
  require(registry.registerProvider(registration, history.parameters()) ==
              ArchiveRegistrationStatus::DUPLICATE_PROVIDER,
          "a key registers once");
  require(registry.find(registration.fields().providerId)->status ==
              ArchiveProviderStatus::PENDING,
          "providers start pending until an unknown beacon fixes their key");
  require(ArchiveProviderRegistration::decode(registration.encode())
                  .registrationId() == registration.registrationId(),
          "registration round-trips");
}

} // namespace

int main() {
  try {
    testChallengeDeterminism();
    testProofValidation();
    testReplayIsRejected();
    testMissingDataAndFraudOutcomes();
    testEpochSummaryDigestCommitsAllAccounting();
    testAssignmentAndSybilResistance();
    testRegistrationRules();
    std::cout << "Archival proof tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Archival proof tests failed: " << error.what() << "\n";
    return 1;
  }
}
