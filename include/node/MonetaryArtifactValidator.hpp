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
     * Check that the current fee burn model matches SupplyDelta exactly.
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

    /*
     * Check that the legacy monetary firewall ledger mirrors the canonical
     * SupplyDelta supply transition.
     */
    static ArtifactValidationResult validateSupplyDeltaConsistencyWithMonetaryFirewallAudit(
        const economics::SupplyDelta& delta,
        const MonetaryFirewallAudit& monetaryFirewallAudit
    );
};

} // namespace nodo::node

#endif
