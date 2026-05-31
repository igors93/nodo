#include "node/ChainAuditor.hpp"

#include "crypto/ProtocolCryptoContext.hpp"

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

    if (manifest.chainId().empty()) {
        return ChainAuditResult::failed("manifest chainId is empty");
    }

    if (manifest.networkName().empty()) {
        return ChainAuditResult::failed("manifest networkName is empty");
    }

    if (manifest.genesisConfigId().empty()) {
        return ChainAuditResult::failed("manifest genesisConfigId is empty");
    }

    const NodeRuntime& runtime =
        load.runtime();

    if (!runtime.isValid()) {
        return ChainAuditResult::failed("rebuilt runtime is invalid");
    }

    if (!runtime.blockchain().isValid()) {
        return ChainAuditResult::failed("rebuilt blockchain is invalid");
    }

    if (runtime.config().genesisConfig().networkParameters().chainId() != manifest.chainId()) {
        return ChainAuditResult::failed("manifest chainId does not match runtime network parameters");
    }

    if (runtime.config().genesisConfig().networkParameters().networkName() != manifest.networkName()) {
        return ChainAuditResult::failed("manifest networkName does not match runtime network parameters");
    }

    if (runtime.config().genesisConfig().deterministicId() != manifest.genesisConfigId()) {
        return ChainAuditResult::failed("manifest genesisConfigId does not match runtime genesis config");
    }

    if (runtime.blockchain().latestBlock().index() != manifest.latestBlockHeight()) {
        return ChainAuditResult::failed("manifest latestBlockHeight does not match rebuilt blockchain");
    }

    if (runtime.blockchain().latestBlock().hash() != manifest.latestBlockHash()) {
        return ChainAuditResult::failed("manifest latestBlockHash does not match rebuilt blockchain");
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
        load.loadedBlockCount(),
        load.loadedMempoolTransactionCount(),
        manifest.validatorCount()
    );
}

} // namespace nodo::node
