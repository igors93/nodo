#include "archive/ArchivalProof.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "archive/ArchiveSignature.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "utils/SafeScalar.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace nodo::archive {

namespace {

constexpr const char *kSignaturePurpose = "ARCHIVE/PROOF";

ArchivalProofVerdict verdict(ArchivalProofStatus status, std::string reason,
                             std::string proofId = "") {
  ArchivalProofVerdict result;
  result.status = status;
  result.reason = std::move(reason);
  result.proofId = std::move(proofId);
  return result;
}

} // namespace

ArchivalProof::ArchivalProof()
    : m_challengeId(), m_providerId(), m_segmentIndex(0), m_round(0),
      m_submittedHeight(0), m_samples(), m_signature() {}

ArchivalProof::ArchivalProof(std::string challengeId, std::string providerId,
                             std::uint64_t segmentIndex, std::uint64_t round,
                             std::uint64_t submittedHeight,
                             std::vector<ArchivalSample> samples,
                             crypto::Signature signature)
    : m_challengeId(std::move(challengeId)),
      m_providerId(std::move(providerId)), m_segmentIndex(segmentIndex),
      m_round(round), m_submittedHeight(submittedHeight),
      m_samples(std::move(samples)), m_signature(std::move(signature)) {}

ArchivalProof ArchivalProof::build(const ArchivalChallenge &challenge,
                                   const ArchiveSegmentIndex &segment,
                                   const ArchivedBlockSource &source,
                                   std::uint64_t submittedHeight,
                                   const crypto::KeyPair &providerKey,
                                   std::int64_t signedAt) {
  const ArchivalChallengeFields &fields = challenge.fields();
  if (!challenge.isStructurallyValid() ||
      fields.segmentId != segment.commitment().segmentId() ||
      fields.providerId != providerKey.address().value() ||
      segment.leaves().size() != segment.commitment().pieceCount()) {
    throw std::invalid_argument("Proof inputs do not match the challenge.");
  }
  const std::vector<serialization::V1MerkleTree::InclusionProof> paths =
      serialization::V1MerkleTree::prove(ArchiveSegmentBuilder::LEAF_KIND,
                                         segment.leaves(), fields.sampleIndices);
  std::vector<ArchivalSample> samples;
  samples.reserve(fields.sampleIndices.size());
  for (std::size_t index = 0; index < fields.sampleIndices.size(); ++index) {
    ArchivalSample sample;
    sample.pieceIndex = fields.sampleIndices[index];
    sample.piece =
        ArchiveSegmentBuilder::readPiece(segment, sample.pieceIndex, source);
    sample.siblings = paths[index].siblings;
    samples.push_back(std::move(sample));
  }
  const ArchivalProof unsignedProof(challenge.challengeId(), fields.providerId,
                                    fields.segmentIndex, fields.round,
                                    submittedHeight, std::move(samples),
                                    crypto::Signature());
  return signedWith(unsignedProof, providerKey, signedAt);
}

ArchivalProof ArchivalProof::signedWith(const ArchivalProof &unsignedProof,
                                        const crypto::KeyPair &providerKey,
                                        std::int64_t signedAt) {
  ArchivalProof proof = unsignedProof;
  proof.m_signature = ArchiveSignature::sign(proof.encodeBody(),
                                             kSignaturePurpose, providerKey,
                                             signedAt);
  return proof;
}

const std::string &ArchivalProof::challengeId() const { return m_challengeId; }
const std::string &ArchivalProof::providerId() const { return m_providerId; }
std::uint64_t ArchivalProof::segmentIndex() const { return m_segmentIndex; }
std::uint64_t ArchivalProof::round() const { return m_round; }
std::uint64_t ArchivalProof::submittedHeight() const {
  return m_submittedHeight;
}
const std::vector<ArchivalSample> &ArchivalProof::samples() const {
  return m_samples;
}
const crypto::Signature &ArchivalProof::signature() const { return m_signature; }

std::vector<unsigned char> ArchivalProof::encodeBody() const {
  serialization::CanonicalWriter writer;
  writer.writeString(SCHEMA);
  writer.writeString(m_challengeId);
  writer.writeString(m_providerId);
  writer.writeUInt64(m_segmentIndex);
  writer.writeUInt64(m_round);
  writer.writeUInt64(m_submittedHeight);
  writer.writeUInt32(static_cast<std::uint32_t>(m_samples.size()));
  for (const ArchivalSample &sample : m_samples) {
    writer.writeUInt64(sample.pieceIndex);
    writer.writeBytes(sample.piece);
    writer.writeUInt32(static_cast<std::uint32_t>(sample.siblings.size()));
    for (const auto &sibling : sample.siblings) {
      writer.writeBytes(std::vector<unsigned char>(sibling.begin(), sibling.end()));
    }
  }
  return writer.bytes();
}

std::string ArchivalProof::proofId() const {
  // Piece bytes can exceed the single-hash input limit, so the id hashes a
  // digest of each sample.
  serialization::CanonicalWriter writer;
  writer.writeString(m_challengeId);
  writer.writeString(m_providerId);
  writer.writeUInt64(m_segmentIndex);
  writer.writeUInt64(m_round);
  writer.writeUInt64(m_submittedHeight);
  writer.writeUInt32(static_cast<std::uint32_t>(m_samples.size()));
  for (const ArchivalSample &sample : m_samples) {
    writer.writeUInt64(sample.pieceIndex);
    writer.writeString(hashHex("ARCHIVE/PIECE", sample.piece));
  }
  return hashHex("ARCHIVE/PROOF-ID", writer.bytes());
}

std::vector<unsigned char> ArchivalProof::encode() const {
  serialization::CanonicalWriter writer;
  writer.writeBytes(encodeBody());
  ArchiveSignature::write(writer, m_signature);
  return writer.bytes();
}

ArchivalProof ArchivalProof::decode(const std::vector<unsigned char> &bytes,
                                    std::uint64_t maxBytes) {
  if (bytes.empty() || bytes.size() > maxBytes) {
    throw std::invalid_argument("Archival proof size is out of range.");
  }
  serialization::CanonicalReader outer(bytes, static_cast<std::size_t>(maxBytes));
  const std::vector<unsigned char> body = outer.readBytes();
  const crypto::Signature signature = ArchiveSignature::read(outer);
  outer.requireFullyConsumed();

  serialization::CanonicalReader reader(body, static_cast<std::size_t>(maxBytes));
  if (reader.readString() != SCHEMA) {
    throw std::invalid_argument("Unknown archival proof schema.");
  }
  std::string challengeId = reader.readString();
  std::string providerId = reader.readString();
  const std::uint64_t segmentIndex = reader.readUInt64();
  const std::uint64_t round = reader.readUInt64();
  const std::uint64_t submittedHeight = reader.readUInt64();
  const std::uint32_t count = reader.readUInt32();
  if (count == 0 || count > config::HistoryParameters::kMaxChallengeSamples) {
    throw std::invalid_argument("Archival proof sample count is out of range.");
  }
  std::vector<ArchivalSample> samples;
  for (std::uint32_t index = 0; index < count; ++index) {
    ArchivalSample sample;
    sample.pieceIndex = reader.readUInt64();
    sample.piece = reader.readBytes();
    const std::uint32_t siblings = reader.readUInt32();
    if (siblings > serialization::V1MerkleTree::kMaxProofSiblings) {
      throw std::invalid_argument("Archival proof path is too long.");
    }
    for (std::uint32_t level = 0; level < siblings; ++level) {
      const std::vector<unsigned char> raw = reader.readBytes();
      serialization::V1MerkleTree::Digest digest{};
      if (raw.size() != digest.size()) {
        throw std::invalid_argument("Archival proof sibling is not a digest.");
      }
      std::copy(raw.begin(), raw.end(), digest.begin());
      sample.siblings.push_back(digest);
    }
    samples.push_back(std::move(sample));
  }
  reader.requireFullyConsumed();
  ArchivalProof proof(std::move(challengeId), std::move(providerId),
                      segmentIndex, round, submittedHeight, std::move(samples),
                      signature);
  if (proof.encode() != bytes) {
    throw std::invalid_argument("Archival proof encoding is not canonical.");
  }
  return proof;
}

std::string archivalProofStatusToString(ArchivalProofStatus status) {
  switch (status) {
  case ArchivalProofStatus::ACCEPTED:
    return "ACCEPTED";
  case ArchivalProofStatus::OVERSIZED:
    return "OVERSIZED";
  case ArchivalProofStatus::MALFORMED:
    return "MALFORMED";
  case ArchivalProofStatus::PROVIDER_UNKNOWN:
    return "PROVIDER_UNKNOWN";
  case ArchivalProofStatus::PROVIDER_INACTIVE:
    return "PROVIDER_INACTIVE";
  case ArchivalProofStatus::WRONG_CHALLENGE:
    return "WRONG_CHALLENGE";
  case ArchivalProofStatus::BAD_SIGNATURE:
    return "BAD_SIGNATURE";
  case ArchivalProofStatus::EXPIRED:
    return "EXPIRED";
  case ArchivalProofStatus::DUPLICATE:
    return "DUPLICATE";
  case ArchivalProofStatus::WRONG_SAMPLES:
    return "WRONG_SAMPLES";
  case ArchivalProofStatus::WRONG_DATA:
    return "WRONG_DATA";
  }
  return "MALFORMED";
}

bool ArchivalReplayGuard::answered(const std::string &challengeId) const {
  return m_answered.count(challengeId) != 0;
}

void ArchivalReplayGuard::record(const std::string &challengeId,
                                 const std::string &proofId) {
  m_answered.emplace(challengeId, proofId);
}

std::size_t ArchivalReplayGuard::size() const { return m_answered.size(); }

ArchivalProofVerdict ArchivalProofVerifier::verify(
    const config::HistoryParameters &parameters,
    const ArchivalChallenge &challenge, const ArchiveSegmentCommitment &segment,
    const ArchiveProviderRegistry &registry, const ArchivalProof &proof,
    const ArchivalReplayGuard &guard, std::uint64_t finalizedHeight) {
  const ArchiveProviderRecord *provider = registry.find(proof.providerId());
  if (provider == nullptr) {
    return verdict(ArchivalProofStatus::PROVIDER_UNKNOWN,
                   "provider is not registered");
  }
  if (provider->status != ArchiveProviderStatus::ACTIVE) {
    return verdict(ArchivalProofStatus::PROVIDER_INACTIVE,
                   "provider is not active");
  }
  return verifyForRegistration(parameters, challenge, segment,
                               provider->registration, proof, guard,
                               finalizedHeight);
}

ArchivalProofVerdict ArchivalProofVerifier::verifyForRegistration(
    const config::HistoryParameters &parameters,
    const ArchivalChallenge &challenge, const ArchiveSegmentCommitment &segment,
    const ArchiveProviderRegistration &registration, const ArchivalProof &proof,
    const ArchivalReplayGuard &guard, std::uint64_t finalizedHeight) {
  using Status = ArchivalProofStatus;
  std::vector<unsigned char> encoded;
  try {
    encoded = proof.encode();
  } catch (const std::exception &error) {
    return verdict(Status::MALFORMED, error.what());
  }
  if (!parameters.isValid() ||
      encoded.size() > parameters.maxArchivalProofBytes()) {
    return verdict(Status::OVERSIZED, "proof exceeds the archival proof cap");
  }
  const std::string proofId = proof.proofId();
  if (proof.samples().empty() || !isDigestHex(proof.challengeId()) ||
      proof.providerId() != registration.fields().providerId) {
    return verdict(Status::MALFORMED, "proof is incomplete", proofId);
  }

  const ArchivalChallengeFields &fields = challenge.fields();
  if (!challenge.isStructurallyValid() ||
      proof.challengeId() != challenge.challengeId() ||
      proof.providerId() != fields.providerId ||
      proof.segmentIndex() != fields.segmentIndex ||
      proof.round() != fields.round || !segment.matchesParameters(parameters) ||
      fields.segmentId != segment.segmentId() ||
      fields.pieceCount != segment.pieceCount()) {
    return verdict(Status::WRONG_CHALLENGE,
                   "proof does not answer this provider's challenge", proofId);
  }
  if (!ArchiveSignature::verify(proof.encodeBody(), kSignaturePurpose,
                                proof.signature(),
                                registration.fields().publicKeyHex)) {
    return verdict(Status::BAD_SIGNATURE, "provider signature is invalid",
                   proofId);
  }
  // The signed height is a claim. Only the finalized inclusion height supplied
  // by the caller can prove timeliness; requiring equality forbids backdating.
  if (proof.submittedHeight() != finalizedHeight ||
      finalizedHeight < fields.issueHeight ||
      finalizedHeight > fields.deadlineHeight) {
    return verdict(Status::EXPIRED, "proof is outside its challenge window",
                   proofId);
  }
  if (guard.answered(proof.challengeId())) {
    return verdict(Status::DUPLICATE, "challenge was already answered",
                   proofId);
  }

  // Everything below is signed by the provider: a failure is fraud.
  const auto root =
      serialization::V1MerkleTree::digestFromHex(segment.pieceRoot());
  if (!root || proof.samples().size() != fields.sampleIndices.size()) {
    return verdict(Status::WRONG_SAMPLES, "wrong number of samples", proofId);
  }
  for (std::size_t index = 0; index < proof.samples().size(); ++index) {
    const ArchivalSample &sample = proof.samples()[index];
    if (sample.pieceIndex != fields.sampleIndices[index]) {
      return verdict(Status::WRONG_SAMPLES,
                     "sample does not match the challenged piece", proofId);
    }
    const serialization::V1MerkleTree::InclusionProof path{
        sample.pieceIndex, segment.pieceCount(), sample.siblings};
    if (sample.piece.size() != segment.pieceLength(sample.pieceIndex) ||
        !serialization::V1MerkleTree::verify(ArchiveSegmentBuilder::LEAF_KIND,
                                             *root, path, sample.piece)) {
      return verdict(Status::WRONG_DATA,
                     "piece " + std::to_string(sample.pieceIndex) +
                         " does not match the segment commitment",
                     proofId);
    }
  }
  return verdict(Status::ACCEPTED, "", proofId);
}

ArchivalFaultEvidence::ArchivalFaultEvidence()
    : m_challenge(), m_proof(), m_finalizedHeight(0) {}

ArchivalFaultEvidence::ArchivalFaultEvidence(ArchivalChallenge challenge,
                                             ArchivalProof proof,
                                             std::uint64_t finalizedHeight)
    : m_challenge(std::move(challenge)), m_proof(std::move(proof)),
      m_finalizedHeight(finalizedHeight) {}

const ArchivalChallenge &ArchivalFaultEvidence::challenge() const {
  return m_challenge;
}

const ArchivalProof &ArchivalFaultEvidence::proof() const { return m_proof; }

std::uint64_t ArchivalFaultEvidence::finalizedHeight() const {
  return m_finalizedHeight;
}

std::string ArchivalFaultEvidence::evidenceId() const {
  serialization::CanonicalWriter writer;
  writer.writeString(m_challenge.challengeId());
  writer.writeString(m_proof.proofId());
  writer.writeUInt64(m_finalizedHeight);
  return hashHex("ARCHIVE/FAULT-EVIDENCE", writer.bytes());
}

bool ArchivalFaultEvidence::verify(const config::HistoryParameters &parameters,
                                   const ArchiveSegmentCommitment &segment,
                                   const ArchiveProviderRegistry &registry,
                                   std::uint64_t observedFinalizedHeight) const {
  // Evidence stays verifiable after the provider's removal, so it is checked
  // against the registration, not the provider's current status. A fresh
  // replay guard: evidence concerns the signed answer, not its recording.
  const ArchiveProviderRecord *provider = registry.find(m_proof.providerId());
  return provider != nullptr && m_finalizedHeight != 0 &&
         m_finalizedHeight == observedFinalizedHeight &&
         ArchivalProofVerifier::verifyForRegistration(
             parameters, m_challenge, segment, provider->registration, m_proof,
             ArchivalReplayGuard(), observedFinalizedHeight)
             .isFraud();
}

} // namespace nodo::archive
