#ifndef NODO_NODE_GOVERNANCE_ARTIFACT_VALIDATOR_HPP
#define NODO_NODE_GOVERNANCE_ARTIFACT_VALIDATOR_HPP

#include "node/ArtifactValidationResult.hpp"
#include "node/FinalizedArtifactValidationContext.hpp"
#include "node/FinalizedBlockArtifact.hpp"

namespace nodo::node {

class GovernanceArtifactValidator {
public:
    static ArtifactValidationResult validate(
        FinalizedArtifactValidationContext& context,
        const FinalizedBlockArtifact& artifact
    );
};

} // namespace nodo::node

#endif
