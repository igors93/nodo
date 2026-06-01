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
     *
     * Returns accepted if the gate accepts; rejected with a reason otherwise.
     */
    static ArtifactValidationResult validateSupplyDelta(
        const economics::MonetaryPolicy& policy,
        const economics::SupplyDelta& delta,
        const std::vector<economics::MintAuthorization>& authorizations
    );

    /*
     * Check that SupplyDelta.burnedAmount >= FeeBurnRecord.burnAmount.
     *
     * The legacy FeeBurnRecord represents only the fee burn portion. The
     * SupplyDelta may include additional burns (e.g., slash burns) so it
     * must be >= the fee burn, not necessarily equal.
     */
    static ArtifactValidationResult validateSupplyDeltaConsistencyWithFeeBurn(
        const economics::SupplyDelta& delta,
        const FeeBurnRecord& feeBurnRecord
    );

    /*
     * Check that SupplyDelta.mintedAmount is consistent with SupplyExpansionRecord.
     *
     * If supply expansion claims no minting, the SupplyDelta must also be
     * mint-free. This prevents the two monetary truth sources from contradicting
     * each other.
     */
    static ArtifactValidationResult validateSupplyDeltaConsistencyWithSupplyExpansion(
        const economics::SupplyDelta& delta,
        const SupplyExpansionRecord& supplyExpansionRecord
    );
};

} // namespace nodo::node

#endif
