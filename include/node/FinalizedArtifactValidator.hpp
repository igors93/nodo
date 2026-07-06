#ifndef NODO_NODE_FINALIZED_ARTIFACT_VALIDATOR_HPP
#define NODO_NODE_FINALIZED_ARTIFACT_VALIDATOR_HPP

#include "node/ArtifactValidationResult.hpp"
#include "node/FinalizedArtifactValidationContext.hpp"
#include "node/FinalizedBlockArtifact.hpp"

namespace nodo::node {

class FinalizedArtifactValidator {
public:
  static ArtifactValidationResult
  validate(FinalizedArtifactValidationContext &context,
           const FinalizedBlockArtifact &artifact);
};

} // namespace nodo::node

#endif
