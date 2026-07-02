#include "node/ArtifactValidationResult.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

std::string
artifactValidationFailureKindToString(ArtifactValidationFailureKind kind) {
  switch (kind) {
  case ArtifactValidationFailureKind::NONE:
    return "NONE";
  case ArtifactValidationFailureKind::INVALID_ARTIFACT:
    return "INVALID_ARTIFACT";
  case ArtifactValidationFailureKind::APPEND_FAILED:
    return "APPEND_FAILED";
  default:
    return "INVALID_ARTIFACT";
  }
}

ArtifactValidationResult::ArtifactValidationResult()
    : m_accepted(false), m_reason("Uninitialized artifact validation result."),
      m_failureKind(ArtifactValidationFailureKind::INVALID_ARTIFACT) {}

ArtifactValidationResult ArtifactValidationResult::acceptedResult() {
  ArtifactValidationResult result;
  result.m_accepted = true;
  result.m_reason = "";
  result.m_failureKind = ArtifactValidationFailureKind::NONE;
  return result;
}

ArtifactValidationResult
ArtifactValidationResult::rejected(std::string reason) {
  ArtifactValidationResult result;
  result.m_accepted = false;
  result.m_reason = std::move(reason);
  result.m_failureKind = ArtifactValidationFailureKind::INVALID_ARTIFACT;
  return result;
}

ArtifactValidationResult
ArtifactValidationResult::appendRejected(std::string reason) {
  ArtifactValidationResult result;
  result.m_accepted = false;
  result.m_reason = std::move(reason);
  result.m_failureKind = ArtifactValidationFailureKind::APPEND_FAILED;
  return result;
}

bool ArtifactValidationResult::accepted() const { return m_accepted; }

const std::string &ArtifactValidationResult::reason() const { return m_reason; }

ArtifactValidationFailureKind ArtifactValidationResult::failureKind() const {
  return m_failureKind;
}

bool ArtifactValidationResult::appendFailed() const {
  return m_failureKind == ArtifactValidationFailureKind::APPEND_FAILED;
}

std::string ArtifactValidationResult::serialize() const {
  std::ostringstream oss;

  oss << "ArtifactValidationResult{"
      << "accepted=" << (m_accepted ? "true" : "false")
      << ";failureKind=" << artifactValidationFailureKindToString(m_failureKind)
      << ";reason=" << m_reason << "}";

  return oss.str();
}

} // namespace nodo::node
