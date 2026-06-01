#include "node/ChainAuditor.hpp"

#include "crypto/ProtocolCryptoContext.hpp"
#include "economics/EpochMonetaryReport.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "node/FinalizedSupplyAudit.hpp"
#include "node/MonetaryAuditDiagnostic.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/ProtocolInvariantChecker.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "node/RuntimeStateVerifier.hpp"

namespace nodo::node {

ChainAuditResult ChainAuditor::auditLoadedRuntime(
    const RuntimeStateLoadResult& load
) {
    if (!load.loaded()) {
        return ChainAuditResult::failed(
            load.reason()
        );
    }

    const NodeRuntimeManifest& manifest =
        load.manifest();

    const NodeRuntime& runtime =
        load.runtime();

    const RuntimeStateVerificationResult runtimeVerification =
        RuntimeStateVerifier::verifyManifestMatchesRuntime(
            manifest,
            runtime
        );

    if (!runtimeVerification.verified()) {
        return ChainAuditResult::failed(
            runtimeVerification.reason()
        );
    }

    const ProtocolInvariantCheckResult invariantCheck =
        ProtocolInvariantChecker::checkRuntimeAgainstManifest(
            runtime,
            manifest
        );

    if (!invariantCheck.passed()) {
        return ChainAuditResult::failed(
            invariantCheck.reason()
        );
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            manifest.networkName()
        );

    if (!cryptoContext.isValid()) {
        return ChainAuditResult::failed(
            "invalid crypto context for network "
            + manifest.networkName()
            + ": "
            + cryptoContext.rejectionReason()
        );
    }

    if (!runtime.mempool().isValid(
            cryptoContext.policy(),
            crypto::SecurityContext::USER_TRANSACTION
        )) {
        return ChainAuditResult::failed("rebuilt mempool is invalid");
    }

    if (!runtime.validatorRegistry().isValid()) {
        return ChainAuditResult::failed("validator registry is invalid");
    }

    if (runtime.validatorRegistry().activeCount() != manifest.validatorCount()) {
        return ChainAuditResult::failed("manifest validatorCount does not match runtime validator registry");
    }

    // Supply continuity audit: verify finalized SupplyDeltas form a valid chain.
    const auto& finalizedDeltas = runtime.supplyState().finalizedDeltas();

    economics::MonetaryPolicy policy;
    utils::Amount genesisSupply;

    if (!finalizedDeltas.empty()) {
        try {
            genesisSupply = MonetaryFirewall::genesisSupply(runtime.config().genesisConfig());
        } catch (const std::exception& e) {
            return ChainAuditResult::failed(
                std::string("chain audit: cannot determine genesis supply: ") + e.what()
            );
        }

        policy = economics::MonetaryPolicy::localnetDefault(
            runtime.config().genesisConfig().networkParameters().chainId(),
            genesisSupply
        );

        const FinalizedSupplyAuditResult supplyAudit =
            FinalizedSupplyAudit::auditDeltas(policy, finalizedDeltas);

        if (!supplyAudit.passed()) {
            const MonetaryAuditDiagnostic diag =
                MonetaryAuditDiagnostic::supplyContinuityFailure(
                    supplyAudit.reason(),
                    supplyAudit.failedBlockHeight(),
                    utils::Amount::fromRawUnits(0),
                    utils::Amount::fromRawUnits(0),
                    supplyAudit.finalSupply()
                );
            return ChainAuditResult::failed(
                "chain audit: monetary supply continuity failure: " +
                supplyAudit.reason() + " | " + diag.serialize()
            );
        }

        // Epoch monetary report verification: rebuild the report from finalized
        // deltas. This proves the delta sequence is self-consistent and that a
        // correct report can be derived deterministically from the chain.
        // For epoch 0 (current simple behavior), the full delta sequence is used.
        const economics::EpochMonetaryReport rebuiltReport =
            economics::EpochMonetaryReport::fromDeltas(
                policy,
                0,
                finalizedDeltas.front().blockHeight(),
                finalizedDeltas.back().blockHeight(),
                finalizedDeltas
            );

        if (!rebuiltReport.isValid()) {
            const MonetaryAuditDiagnostic diag =
                MonetaryAuditDiagnostic::reportMissing(
                    "cannot rebuild epoch monetary report from finalized deltas: " +
                    rebuiltReport.rejectionReason(),
                    0
                );
            return ChainAuditResult::failed(
                "chain audit: epoch monetary report rebuild failed: " +
                rebuiltReport.rejectionReason() + " | " + diag.serialize()
            );
        }
    }

    return ChainAuditResult::passed(
        manifest.networkName(),
        cryptoContext.networkProfile(),
        manifest.latestBlockHeight(),
        manifest.latestBlockHash(),
        manifest.latestStateRoot(),
        load.loadedBlockCount(),
        load.loadedMempoolTransactionCount(),
        manifest.validatorCount()
    );
}

} // namespace nodo::node
