#include "consensus/ProposalJustification.hpp"

#include <stdexcept>
#include <utility>

namespace nodo::consensus {

namespace {
constexpr const char *kNoneText = "ProposalJustification{type=NONE}";
// No '\n' anywhere in this format: SignedBlockProposalMessage::deserialize
// locates the header/block-payload boundary via the FIRST '\n' in the whole
// serialized message, and this text is embedded inline as the value of that
// outer message's own "justification" field. QuorumCertificate::serialize()
// is itself single-line and brace-balanced, so wrapping it between a fixed
// prefix and exactly one trailing '}' keeps the whole value single-line and
// lets deserialize() recover it with plain prefix/suffix slicing (no
// brace-depth scanning needed) — SignedBlockProposalMessage's own top-level
// field splitter is brace-depth aware, so this nested value is safely
// treated as one field even though it contains further ';'/'{'/'}'.
constexpr const char *kUnlockPrefix =
    "ProposalJustification{type=UNLOCK_QC;certificate=";
} // namespace

std::string proposalJustificationTypeToString(ProposalJustificationType type) {
  switch (type) {
  case ProposalJustificationType::NONE:
    return "NONE";
  case ProposalJustificationType::UNLOCK_QUORUM_CERTIFICATE:
    return "UNLOCK_QC";
  }
  return "NONE";
}

ProposalJustification::ProposalJustification()
    : m_type(ProposalJustificationType::NONE), m_certificate() {}

ProposalJustification ProposalJustification::none() {
  return ProposalJustification();
}

ProposalJustification
ProposalJustification::unlockQuorumCertificate(QuorumCertificate certificate) {
  // Deliberately does not validate: QuorumCertificate itself separates
  // construction from validity (isStructurallyValid()/verify() are queries,
  // not constructor checks), and permitsUnlock() below is the actual
  // enforcement point. A malicious proposer is not bound by this factory
  // anyway — the real adversarial input path is deserialize(), which already
  // goes through QuorumCertificate::deserialize()'s own strict validation
  // before ever reaching here.
  ProposalJustification justification;
  justification.m_type = ProposalJustificationType::UNLOCK_QUORUM_CERTIFICATE;
  justification.m_certificate = std::move(certificate);
  return justification;
}

ProposalJustificationType ProposalJustification::type() const { return m_type; }

bool ProposalJustification::hasUnlockCertificate() const {
  return m_type == ProposalJustificationType::UNLOCK_QUORUM_CERTIFICATE;
}

const QuorumCertificate &ProposalJustification::unlockCertificate() const {
  if (!hasUnlockCertificate()) {
    throw std::logic_error(
        "ProposalJustification has no unlock quorum certificate.");
  }
  return m_certificate;
}

bool ProposalJustification::isValid() const {
  switch (m_type) {
  case ProposalJustificationType::NONE:
    return true;
  case ProposalJustificationType::UNLOCK_QUORUM_CERTIFICATE:
    return m_certificate.isStructurallyValid();
  }
  return false;
}

std::string ProposalJustification::serialize() const {
  if (m_type == ProposalJustificationType::NONE) {
    return kNoneText;
  }
  return std::string(kUnlockPrefix) + m_certificate.serialize() + "}";
}

ProposalJustification
ProposalJustification::deserialize(const std::string &serialized) {
  if (serialized == kNoneText) {
    return ProposalJustification::none();
  }

  const std::string prefix(kUnlockPrefix);
  if (serialized.size() <= prefix.size() + 1 ||
      serialized.compare(0, prefix.size(), prefix) != 0 ||
      serialized.back() != '}') {
    throw std::invalid_argument("Malformed ProposalJustification.");
  }

  // The certificate blob is everything after the prefix up to (but not
  // including) the one closing brace this class itself appended in
  // serialize() — QuorumCertificate::serialize() already ends with its own
  // '}', so stripping exactly one trailing character isolates it exactly,
  // with no brace-depth scanning required.
  const std::string certificateText =
      serialized.substr(prefix.size(), serialized.size() - prefix.size() - 1);
  QuorumCertificate certificate =
      QuorumCertificate::deserialize(certificateText);
  return ProposalJustification::unlockQuorumCertificate(
      std::move(certificate));
}

bool ProposalJustification::permitsUnlock(
    const ProposalJustification &justification, std::uint64_t lockedRound,
    const std::string &candidateBlockHash, std::uint64_t round,
    const core::ValidatorRegistry &validators, const crypto::CryptoPolicy &policy,
    const crypto::SignatureProvider &provider, std::string *reason) {
  if (round <= lockedRound) {
    if (reason != nullptr)
      *reason =
          "justification round does not exceed the current lock's round";
    return false;
  }

  if (!justification.hasUnlockCertificate()) {
    if (reason != nullptr)
      *reason = "no unlock quorum certificate was provided";
    return false;
  }

  const QuorumCertificate &certificate = justification.unlockCertificate();

  if (!certificate.isStructurallyValid()) {
    if (reason != nullptr)
      *reason = "unlock quorum certificate is structurally invalid";
    return false;
  }

  if (!certificate.verify(validators, policy, provider)) {
    if (reason != nullptr)
      *reason = "unlock quorum certificate failed cryptographic verification";
    return false;
  }

  if (certificate.round() < lockedRound) {
    if (reason != nullptr)
      *reason = "unlock quorum certificate round is older than the current "
                "lock";
    return false;
  }

  if (certificate.blockHash() != candidateBlockHash) {
    if (reason != nullptr)
      *reason = "unlock quorum certificate does not certify the candidate "
                "block";
    return false;
  }

  if (reason != nullptr)
    *reason = "permitted";
  return true;
}

} // namespace nodo::consensus
