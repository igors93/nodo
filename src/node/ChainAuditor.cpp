#include "node/ChainAuditor.hpp"

#include "crypto/ProtocolCryptoContext.hpp"
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
