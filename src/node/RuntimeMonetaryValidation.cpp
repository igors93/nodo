#include "node/RuntimeMonetaryValidation.hpp"

#include "economics/BurnRecord.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDeltaBuilder.hpp"
#include "node/MonetaryFirewall.hpp"

#include <sstream>
#include <utility>
#include <vector>

namespace nodo::node {

std::string runtimeMonetaryValidationStatusToString(
    RuntimeMonetaryValidationStatus status
) {
    switch (status) {
        case RuntimeMonetaryValidationStatus::ACCEPTED:
            return "ACCEPTED";
        case RuntimeMonetaryValidationStatus::MONETARY_CONTEXT_UNAVAILABLE:
            return "MONETARY_CONTEXT_UNAVAILABLE";
        case RuntimeMonetaryValidationStatus::REJECTED_BY_GATE:
            return "REJECTED_BY_GATE";
        default:
            return "UNKNOWN";
    }
}

RuntimeMonetaryValidationResult::RuntimeMonetaryValidationResult()
    : m_accepted(false),
      m_status(RuntimeMonetaryValidationStatus::MONETARY_CONTEXT_UNAVAILABLE),
      m_reason("") {}

RuntimeMonetaryValidationResult RuntimeMonetaryValidationResult::accepted(
    economics::SupplyDelta supplyDelta,
    economics::MonetaryValidationGateResult gateResult
) {
    RuntimeMonetaryValidationResult r;
    r.m_accepted = true;
    r.m_status = RuntimeMonetaryValidationStatus::ACCEPTED;
    r.m_supplyDelta = std::move(supplyDelta);
    r.m_gateResult = std::move(gateResult);
    return r;
}

RuntimeMonetaryValidationResult RuntimeMonetaryValidationResult::contextUnavailable(
    std::string reason
) {
    RuntimeMonetaryValidationResult r;
    r.m_accepted = false;
    r.m_status = RuntimeMonetaryValidationStatus::MONETARY_CONTEXT_UNAVAILABLE;
    r.m_reason = std::move(reason);
    return r;
}

RuntimeMonetaryValidationResult RuntimeMonetaryValidationResult::rejectedByGate(
    economics::SupplyDelta supplyDelta,
    economics::MonetaryValidationGateResult gateResult
) {
    RuntimeMonetaryValidationResult r;
    r.m_accepted = false;
    r.m_status = RuntimeMonetaryValidationStatus::REJECTED_BY_GATE;
    r.m_reason = gateResult.reason();
    r.m_supplyDelta = std::move(supplyDelta);
    r.m_gateResult = std::move(gateResult);
    return r;
}

bool RuntimeMonetaryValidationResult::isAccepted() const { return m_accepted; }
RuntimeMonetaryValidationStatus RuntimeMonetaryValidationResult::status() const { return m_status; }
const std::string& RuntimeMonetaryValidationResult::reason() const { return m_reason; }
const economics::SupplyDelta& RuntimeMonetaryValidationResult::supplyDelta() const { return m_supplyDelta; }
const economics::MonetaryValidationGateResult& RuntimeMonetaryValidationResult::gateResult() const { return m_gateResult; }

std::string RuntimeMonetaryValidationResult::serialize() const {
    std::ostringstream oss;
    oss << "RuntimeMonetaryValidationResult{"
        << "accepted=" << (m_accepted ? "1" : "0")
        << ";status=" << runtimeMonetaryValidationStatusToString(m_status)
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

RuntimeMonetaryValidationResult RuntimeMonetaryValidation::validateCandidate(
    const config::GenesisConfig& genesisConfig,
    const core::Block& candidateBlock,
    utils::Amount feeBurnAmount,
    utils::Amount supplyBefore
) {
    // Derive economics::MonetaryPolicy from the genesis config.
    utils::Amount genesisSupplyAmount;
    try {
        genesisSupplyAmount = MonetaryFirewall::genesisSupply(genesisConfig);
    } catch (const std::exception& e) {
        return RuntimeMonetaryValidationResult::contextUnavailable(
            std::string("RuntimeMonetaryValidation: cannot determine genesis supply: ") +
            e.what()
        );
    }

    const economics::MonetaryPolicy policy =
        economics::MonetaryPolicy::localnetDefault(
            genesisConfig.networkParameters().chainId(),
            genesisSupplyAmount
        );

    if (!policy.isValid()) {
        return RuntimeMonetaryValidationResult::contextUnavailable(
            "RuntimeMonetaryValidation: derived monetary policy is invalid: " +
            policy.rejectionReason()
        );
    }

    // Use the explicitly provided supplyBefore (cumulative from supply state).

    const std::uint64_t blockHeight = candidateBlock.index();
    const std::string& blockHash = candidateBlock.hash();
    constexpr std::uint64_t kCurrentEpoch = 0;

    // Build the SupplyDelta for this candidate block.
    economics::SupplyDelta delta;

    if (feeBurnAmount.isZero()) {
        // No monetary effects — no-op delta is valid.
        delta = economics::SupplyDelta::noOp(
            blockHeight, blockHash, kCurrentEpoch, supplyBefore
        );
    } else {
        // Build a burn record for the fee burn.
        const economics::BurnRecord feeBurn(
            "fee-burn-block-" + std::to_string(blockHeight),
            blockHeight,
            kCurrentEpoch,
            "nodo_fee_pool",
            feeBurnAmount,
            "fee burn",
            economics::BurnType::FEE_BURN
        );

        try {
            delta = economics::SupplyDeltaBuilder::build(
                blockHeight,
                blockHash,
                kCurrentEpoch,
                supplyBefore,
                {},           // no mint records in regular blocks
                {feeBurn}
            );
        } catch (const std::exception& e) {
            return RuntimeMonetaryValidationResult::contextUnavailable(
                std::string("RuntimeMonetaryValidation: SupplyDeltaBuilder error: ") +
                e.what()
            );
        }
    }

    if (!delta.isValid()) {
        return RuntimeMonetaryValidationResult::contextUnavailable(
            "RuntimeMonetaryValidation: derived SupplyDelta is invalid: " +
            delta.rejectionReason()
        );
    }

    // No mint authorizations for regular blocks (no minting).
    const std::vector<economics::MintAuthorization> noAuthorizations;

    const economics::MonetaryValidationGateResult gateResult =
        economics::MonetaryValidationGate::validate(policy, delta, noAuthorizations);

    if (!gateResult.isAccepted()) {
        return RuntimeMonetaryValidationResult::rejectedByGate(delta, gateResult);
    }

    return RuntimeMonetaryValidationResult::accepted(delta, gateResult);
}

RuntimeMonetaryValidationResult RuntimeMonetaryValidation::validateCandidate(
    const config::GenesisConfig& genesisConfig,
    const core::Block& candidateBlock,
    utils::Amount feeBurnAmount
) {
    // Convenience overload: uses genesis supply as supplyBefore.
    // Valid only for the first block or in unit tests.
    utils::Amount genesisSupply;
    try {
        genesisSupply = MonetaryFirewall::genesisSupply(genesisConfig);
    } catch (const std::exception& e) {
        return RuntimeMonetaryValidationResult::contextUnavailable(
            std::string("RuntimeMonetaryValidation: cannot determine genesis supply: ") +
            e.what()
        );
    }
    return validateCandidate(genesisConfig, candidateBlock, feeBurnAmount, genesisSupply);
}

} // namespace nodo::node
