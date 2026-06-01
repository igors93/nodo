#ifndef NODO_NODE_MONETARY_ARTIFACT_VALIDATOR_HPP
#define NODO_NODE_MONETARY_ARTIFACT_VALIDATOR_HPP

#include "economics/MonetaryPolicy.hpp"
#include "economics/MonetaryValidationGate.hpp"
#include "economics/MintAuthorization.hpp"
#include "economics/SupplyDelta.hpp"
#include "node/ArtifactValidationResult.hpp"
#include "node/FinalizedArtifactValidationContext.hpp"
#include "node/FinalizedBlockArtifact.hpp"

#include <vector>

namespace nodo::node {

class MonetaryArtifactValidator {
public:
    /*
     * Full artifact monetary validation (inflation-layer audit + fee economics).
     */
    static ArtifactValidationResult validate(
        FinalizedArtifactValidationContext& context,
        const FinalizedBlockArtifact& artifact
    );

    /*
     * Validate a SupplyDelta using the economics::MonetaryValidationGate.
     *
     * This is the authorization-layer check for finalized artifacts.
     * Task 05 will call this when FinalizedBlockArtifact carries a SupplyDelta.
     *
     * Returns accepted if the gate accepts; rejected with a reason otherwise.
     */
    static ArtifactValidationResult validateSupplyDelta(
        const economics::MonetaryPolicy& policy,
        const economics::SupplyDelta& delta,
        const std::vector<economics::MintAuthorization>& authorizations
    );
};

} // namespace nodo::node

#endif
