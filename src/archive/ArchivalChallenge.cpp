#include "archive/ArchivalChallenge.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "serialization/V1EncodingPrimitives.hpp"
#include "utils/SafeScalar.hpp"

#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace nodo::archive {

namespace {

// Each draw is rejected with probability below 1/2, so 64 attempts per
// sample fail only with negligible probability; the bound keeps the loop
// finite for adversarial inputs.
constexpr std::uint32_t kMaxAttemptsPerSample = 64;

std::uint64_t drawU64(const std::string &seedHex, std::uint32_t sample,
                      std::uint32_t attempt) {
  serialization::CanonicalWriter writer;
  writer.writeString(seedHex);
  writer.writeUInt32(sample);
  writer.writeUInt32(attempt);
  const auto digest =
      serialization::V1EncodingPrimitives::hash("ARCHIVE/SAMPLE", writer.bytes());
  std::uint64_t value = 0;
  for (int index = 0; index < 8; ++index) {
    value = (value << 8) | digest[static_cast<std::size_t>(index)];
  }
  return value;
}

} // namespace

ArchivalChallenge::ArchivalChallenge() : m_fields() {}

ArchivalChallenge::ArchivalChallenge(ArchivalChallengeFields fields)
    : m_fields(std::move(fields)) {}

const ArchivalChallengeFields &ArchivalChallenge::fields() const {
  return m_fields;
}

std::string ArchivalChallenge::seed() const {
  serialization::CanonicalWriter writer;
  writer.writeString(m_fields.chainId);
  writer.writeUInt64(m_fields.round);
  writer.writeString(m_fields.seedBlockHash);
  writer.writeString(m_fields.providerId);
  writer.writeUInt64(m_fields.segmentIndex);
  writer.writeString(m_fields.segmentId);
  return hashHex("ARCHIVE/CHALLENGE-SEED", writer.bytes());
}

std::vector<std::uint64_t>
ArchivalChallenge::sampleIndices(const std::string &seedHex,
                                 std::uint64_t pieceCount,
                                 std::uint32_t samples) {
  if (pieceCount == 0 || samples == 0 || !isDigestHex(seedHex)) {
    throw std::invalid_argument("Challenge sampling needs pieces and a seed.");
  }
  std::vector<std::uint64_t> indices;
  if (samples >= pieceCount) {
    for (std::uint64_t index = 0; index < pieceCount; ++index) {
      indices.push_back(index);
    }
    return indices;
  }
  // Largest multiple of pieceCount below 2^64: accepting only draws below it
  // removes modulo bias.
  const unsigned __int128 range = static_cast<unsigned __int128>(1) << 64;
  const unsigned __int128 limit = (range / pieceCount) * pieceCount;
  std::set<std::uint64_t> chosen;
  for (std::uint32_t sample = 0; indices.size() < samples; ++sample) {
    bool placed = false;
    for (std::uint32_t attempt = 0; attempt < kMaxAttemptsPerSample; ++attempt) {
      const std::uint64_t draw = drawU64(seedHex, sample, attempt);
      if (draw >= limit) {
        continue;
      }
      const std::uint64_t index = draw % pieceCount;
      if (chosen.insert(index).second) {
        indices.push_back(index);
        placed = true;
        break;
      }
    }
    if (!placed) {
      throw std::runtime_error("Challenge sampling exhausted its attempts.");
    }
  }
  return indices;
}

ArchivalChallenge ArchivalChallenge::derive(
    const config::HistoryParameters &parameters, const std::string &providerId,
    const ArchiveSegmentCommitment &segment, std::uint64_t round,
    const std::string &seedBlockHash) {
  if (!segment.matchesParameters(parameters)) {
    throw std::invalid_argument("Challenge segment does not match parameters.");
  }
  ArchivalChallengeFields fields;
  fields.chainId = segment.chainId();
  fields.providerId = providerId;
  fields.segmentIndex = segment.segmentIndex();
  fields.segmentId = segment.segmentId();
  fields.pieceCount = segment.pieceCount();
  fields.round = round;
  fields.seedHeight = parameters.challengeSeedHeight(round);
  fields.seedBlockHash = seedBlockHash;
  fields.issueHeight = parameters.challengeIssueHeight(round);
  fields.deadlineHeight = parameters.challengeDeadlineHeight(round);
  // A segment can only be challenged once it is sealed and its commitment
  // finalized, i.e. no earlier than the round after its last height.
  if (fields.seedHeight < segment.lastHeight()) {
    throw std::invalid_argument("Segment is not sealed before this round.");
  }
  ArchivalChallenge challenge(fields);
  fields.sampleIndices = sampleIndices(challenge.seed(), fields.pieceCount,
                                       parameters.archiveChallengeSamples());
  ArchivalChallenge result(std::move(fields));
  if (!result.isStructurallyValid()) {
    throw std::logic_error("Derived archival challenge is invalid.");
  }
  return result;
}

std::vector<ArchivalChallenge> ArchivalChallenge::forRound(
    const config::HistoryParameters &parameters,
    const std::vector<ArchiveSlot> &slots,
    const std::vector<ArchiveSegmentCommitment> &segments, std::uint64_t round,
    const std::string &seedBlockHash) {
  std::vector<ArchivalChallenge> challenges;
  for (const ArchiveSlot &slot : slots) {
    if (slot.segmentIndex >= segments.size() ||
        segments[slot.segmentIndex].segmentIndex() != slot.segmentIndex) {
      throw std::invalid_argument("Slot references an unknown segment.");
    }
    const ArchiveSegmentCommitment &segment = segments[slot.segmentIndex];
    if (parameters.challengeSeedHeight(round) < segment.lastHeight()) {
      continue;
    }
    challenges.push_back(
        derive(parameters, slot.providerId, segment, round, seedBlockHash));
  }
  return challenges;
}

bool ArchivalChallenge::isStructurallyValid() const {
  const ArchivalChallengeFields &f = m_fields;
  if (!utils::isSafeIdentifier(f.chainId, 128, "_-.") ||
      !utils::isSafeIdentifier(f.providerId, 128, "_-.:") ||
      !isDigestHex(f.segmentId) || !isDigestHex(f.seedBlockHash) ||
      f.pieceCount == 0 ||
      f.pieceCount > serialization::V1MerkleTree::kMaxLeaves ||
      f.seedHeight == std::numeric_limits<std::uint64_t>::max() ||
      f.issueHeight != f.seedHeight + 1 || f.deadlineHeight <= f.issueHeight ||
      f.sampleIndices.empty() ||
      f.sampleIndices.size() > config::HistoryParameters::kMaxChallengeSamples) {
    return false;
  }
  try {
    return f.sampleIndices ==
           sampleIndices(seed(), f.pieceCount,
                         static_cast<std::uint32_t>(f.sampleIndices.size()));
  } catch (const std::exception &) {
    return false;
  }
}

std::string ArchivalChallenge::challengeId() const {
  return hashHex("ARCHIVE/CHALLENGE", encode());
}

std::vector<unsigned char> ArchivalChallenge::encode() const {
  const ArchivalChallengeFields &f = m_fields;
  serialization::CanonicalWriter writer;
  writer.writeString(SCHEMA);
  writer.writeString(f.chainId);
  writer.writeString(f.providerId);
  writer.writeUInt64(f.segmentIndex);
  writer.writeString(f.segmentId);
  writer.writeUInt64(f.pieceCount);
  writer.writeUInt64(f.round);
  writer.writeUInt64(f.seedHeight);
  writer.writeString(f.seedBlockHash);
  writer.writeUInt64(f.issueHeight);
  writer.writeUInt64(f.deadlineHeight);
  writer.writeUInt32(static_cast<std::uint32_t>(f.sampleIndices.size()));
  for (const std::uint64_t index : f.sampleIndices) {
    writer.writeUInt64(index);
  }
  return writer.bytes();
}

ArchivalChallenge
ArchivalChallenge::decode(const std::vector<unsigned char> &bytes) {
  if (bytes.empty() || bytes.size() > kMaxEncodedBytes) {
    throw std::invalid_argument("Challenge size is out of range.");
  }
  serialization::CanonicalReader reader(bytes, 256);
  if (reader.readString() != SCHEMA) {
    throw std::invalid_argument("Unknown archival challenge schema.");
  }
  ArchivalChallengeFields f;
  f.chainId = reader.readString();
  f.providerId = reader.readString();
  f.segmentIndex = reader.readUInt64();
  f.segmentId = reader.readString();
  f.pieceCount = reader.readUInt64();
  f.round = reader.readUInt64();
  f.seedHeight = reader.readUInt64();
  f.seedBlockHash = reader.readString();
  f.issueHeight = reader.readUInt64();
  f.deadlineHeight = reader.readUInt64();
  const std::uint32_t count = reader.readUInt32();
  if (count == 0 || count > config::HistoryParameters::kMaxChallengeSamples) {
    throw std::invalid_argument("Challenge sample count is out of range.");
  }
  for (std::uint32_t index = 0; index < count; ++index) {
    f.sampleIndices.push_back(reader.readUInt64());
  }
  reader.requireFullyConsumed();
  ArchivalChallenge challenge(std::move(f));
  if (!challenge.isStructurallyValid()) {
    throw std::invalid_argument("Archival challenge is not derivable.");
  }
  return challenge;
}

} // namespace nodo::archive
