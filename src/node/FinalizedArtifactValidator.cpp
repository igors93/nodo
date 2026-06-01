#include "node/FinalizedArtifactValidator.hpp"

#include "node/EconomicArtifactValidator.hpp"
#include "node/FinalityArtifactValidator.hpp"
#include "node/GovernanceArtifactValidator.hpp"
#include "node/MonetaryArtifactValidator.hpp"
#include "node/SlashingArtifactValidator.hpp"
#include "node/StateArtifactValidator.hpp"
#include "node/ValidatorLifecycleArtifactValidator.hpp"

#include <array>

namespace nodo::node {

namespace {

using ValidatorFunction = ArtifactValidationResult (*)(
    FinalizedArtifactValidationContext&,
    const FinalizedBlockArtifact&
);

} // namespace

ArtifactValidationResult FinalizedArtifactValidator::validate(
    FinalizedArtifactValidationContext& context,
    const FinalizedBlockArtifact& artifact
) {
    static const std::array<ValidatorFunction, 7> validators = {
        &FinalityArtifactValidator::validate,
        &StateArtifactValidator::validate,
        &EconomicArtifactValidator::validate,
        &MonetaryArtifactValidator::validate,
        &SlashingArtifactValidator::validate,
        &GovernanceArtifactValidator::validate,
        &ValidatorLifecycleArtifactValidator::validate
    };

    for (const ValidatorFunction validator : validators) {
        const ArtifactValidationResult result =
            validator(
                context,
                artifact
            );

        if (!result.accepted()) {
            return result;
        }
    }

    return ArtifactValidationResult::acceptedResult();
}

} // namespace nodo::node

