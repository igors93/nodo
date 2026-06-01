#ifndef NODO_NODE_STATE_ARTIFACT_VALIDATOR_HPP
#define NODO_NODE_STATE_ARTIFACT_VALIDATOR_HPP

#include "node/ArtifactValidationResult.hpp"
#include "node/FinalizedArtifactValidationContext.hpp"
#include "node/FinalizedBlockArtifact.hpp"

namespace nodo::node {

class StateArtifactValidator {
public:
    static ArtifactValidationResult validate(
        FinalizedArtifactValidationContext& context,
        const FinalizedBlockArtifact& artifact
    );
};

} // namespace nodo::node

#endif
