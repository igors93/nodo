#ifndef NODO_NODE_FINALITY_ARTIFACT_VALIDATOR_HPP
#define NODO_NODE_FINALITY_ARTIFACT_VALIDATOR_HPP

#include "node/ArtifactValidationResult.hpp"
#include "node/FinalizedArtifactValidationContext.hpp"
#include "node/FinalizedBlockArtifact.hpp"

namespace nodo::node {

class FinalityArtifactValidator {
public:
  static ArtifactValidationResult
  validate(FinalizedArtifactValidationContext &context,
           const FinalizedBlockArtifact &artifact);

  static ArtifactValidationResult
  applyFinalization(FinalizedArtifactValidationContext &context,
                    const FinalizedBlockArtifact &artifact);
};

} // namespace nodo::node

#endif
