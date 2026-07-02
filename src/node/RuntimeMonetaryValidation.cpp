#include "node/RuntimeMonetaryValidation.hpp"

#include "economics/BurnRecord.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/SupplyDeltaBuilder.hpp"
#include "core/Transaction.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <sstream>
#include <optional>
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
    std::uint64_t deltaEpoch = blockHeight == 0
        ? 0
        : ValidatorLifecycle::epochIndexForBlock(blockHeight);

    std::vector<economics::GenesisRewardRecord> rewardRecords;
    std::optional<economics::ProtectionEpoch> protectionEpoch;
    try {
        for (const core::LedgerRecord& record : candidateBlock.records()) {
            if (record.type() == core::LedgerRecordType::PROTECTION_EPOCH) {
                if (protectionEpoch.has_value()) {
                    throw std::invalid_argument("duplicate ProtectionEpoch record");
                }
                protectionEpoch = economics::ProtectionEpoch::deserialize(record.payload());
            } else if (record.type() == core::LedgerRecordType::GENESIS_REWARD) {
                rewardRecords.push_back(
                    economics::GenesisRewardRecord::deserialize(record.payload())
                );
            }
        }
        if (!rewardRecords.empty()) {
            if (!protectionEpoch.has_value() ||
                !protectionEpoch->hasCanonicalSettlementMetadata()) {
                throw std::invalid_argument("GenesisReward requires canonical epoch metadata");
            }
            deltaEpoch = protectionEpoch->epochId();
        }
    } catch (const std::exception& error) {
        return RuntimeMonetaryValidationResult::contextUnavailable(
            std::string("RuntimeMonetaryValidation: invalid epoch reward bundle: ") + error.what()
        );
    }

    std::vector<economics::BurnRecord> burnRecords;
    if (feeBurnAmount.isPositive()) {
        burnRecords.emplace_back(
            "fee-burn-block-" + std::to_string(blockHeight), blockHeight,
            deltaEpoch, "nodo_fee_pool", feeBurnAmount, "fee burn",
            economics::BurnType::FEE_BURN
        );
    }
    try {
        for (const core::LedgerRecord& record : candidateBlock.records()) {
            if (record.type() != core::LedgerRecordType::TRANSACTION) continue;
            const core::Transaction tx = core::Transaction::deserialize(record.payload());
            if (tx.type() != core::TransactionType::BURN) continue;
            burnRecords.emplace_back(
                tx.id(), blockHeight, deltaEpoch, tx.fromAddress(), tx.amount(),
                "voluntary burn", economics::BurnType::VOLUNTARY_BURN
            );
        }
    } catch (const std::exception& error) {
        return RuntimeMonetaryValidationResult::contextUnavailable(
            std::string("RuntimeMonetaryValidation: invalid burn transaction: ") + error.what()
        );
    }

    std::vector<economics::MintRecord> mintRecords;
    std::vector<economics::MintAuthorization> authorizations;
    if (!rewardRecords.empty()) {
        const std::string authorizationId =
            "epoch-reward-" + std::to_string(deltaEpoch) + "-" +
            protectionEpoch->evidenceBlockHash();
        for (const auto& reward : rewardRecords) {
            if (reward.epoch() != deltaEpoch ||
                reward.policyVersion() != protectionEpoch->policyVersion() ||
                reward.acceptedBlockHash() != protectionEpoch->evidenceBlockHash() ||
                reward.timestamp() != candidateBlock.timestamp()) {
                return RuntimeMonetaryValidationResult::contextUnavailable(
                    "RuntimeMonetaryValidation: GenesisReward metadata mismatch."
                );
            }
            mintRecords.emplace_back(
                reward.deterministicId(), authorizationId,
                reward.validatorAddress(), reward.amount(),
                economics::MintReason::NETWORK_DEFENSE_REWARD,
                deltaEpoch, blockHeight, blockHash, reward.timestamp()
            );
        }
        authorizations.emplace_back(
            authorizationId, policy.policyVersion(), deltaEpoch, deltaEpoch,
            protectionEpoch->securityEmission(),
            "canonical epoch protection rewards",
            protectionEpoch->policyVersion()
        );
    }

    // Build the SupplyDelta for this candidate block.
    economics::SupplyDelta delta;

    if (burnRecords.empty() && mintRecords.empty()) {
        // No monetary effects — no-op delta is valid.
        delta = economics::SupplyDelta::noOp(
            blockHeight, blockHash, deltaEpoch, supplyBefore
        );
    } else {
        try {
            delta = economics::SupplyDeltaBuilder::build(
                blockHeight,
                blockHash,
                deltaEpoch,
                supplyBefore,
                mintRecords,
                burnRecords
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

    const economics::MonetaryValidationGateResult gateResult =
        economics::MonetaryValidationGate::validate(policy, delta, authorizations);

    if (!gateResult.isAccepted()) {
        return RuntimeMonetaryValidationResult::rejectedByGate(delta, gateResult);
    }

    return RuntimeMonetaryValidationResult::accepted(delta, gateResult);
}

RuntimeMonetaryValidationResult RuntimeMonetaryValidation::validateFirstBlockCandidate(
    const config::GenesisConfig& genesisConfig,
    const core::Block& candidateBlock,
    utils::Amount feeBurnAmount
) {
    // Restricted form: only valid for block.index() == 1 (first block after genesis).
    // Rejecting any other height prevents accidental use for multi-block chains where
    // the cumulative supply differs from genesis supply.
    if (candidateBlock.index() != 1) {
        return RuntimeMonetaryValidationResult::contextUnavailable(
            "RuntimeMonetaryValidation::validateFirstBlockCandidate: "
            "only valid for block index 1, got " +
            std::to_string(candidateBlock.index()) + "."
        );
    }

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
