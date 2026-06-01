#include "node/ChainAuditor.hpp"

#include "crypto/ProtocolCryptoContext.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "node/FinalizedSupplyAudit.hpp"
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
    if (!finalizedDeltas.empty()) {
        utils::Amount genesisSupply;
        try {
            genesisSupply = MonetaryFirewall::genesisSupply(runtime.config().genesisConfig());
        } catch (const std::exception& e) {
            return ChainAuditResult::failed(
                std::string("chain audit: cannot determine genesis supply: ") + e.what()
            );
        }

        const economics::MonetaryPolicy policy =
            economics::MonetaryPolicy::localnetDefault(
                runtime.config().genesisConfig().networkParameters().chainId(),
                genesisSupply
            );

        const FinalizedSupplyAuditResult supplyAudit =
            FinalizedSupplyAudit::auditDeltas(policy, finalizedDeltas);

        if (!supplyAudit.passed()) {
            return ChainAuditResult::failed(
                "chain audit: monetary supply continuity failure: " +
                supplyAudit.reason()
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
