#include "node/ChainAuditor.hpp"

#include "node/AuditAssignment.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "economics/EpochMonetaryReport.hpp"
#include "economics/EpochTreasuryReport.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "node/EpochMonetaryReportStore.hpp"
#include "node/EpochTreasuryReportStore.hpp"
#include "node/EpochTreasuryReportVerifier.hpp"
#include "node/FinalizedSupplyAudit.hpp"
#include "node/FinalizedTreasuryAudit.hpp"
#include "node/MonetaryAuditDiagnostic.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/MonetaryReportVerifier.hpp"
#include "node/ProtocolInvariantChecker.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "node/RuntimeStateVerifier.hpp"

#include <filesystem>

namespace nodo::node {

namespace {

// Internal helper used by both public entry points. requireReport controls
// whether a missing monetaryReportPath is fatal when finalized deltas exist.
ChainAuditResult auditImpl(
    const RuntimeStateLoadResult& load,
    const std::filesystem::path& monetaryReportPath,
    const std::filesystem::path& treasuryReportPath,
    bool requireReport
) {
    if (!load.loaded()) {
        return ChainAuditResult::failed(load.reason());
    }

    const NodeRuntimeManifest& manifest = load.manifest();
    const NodeRuntime& runtime          = load.runtime();

    const RuntimeStateVerificationResult runtimeVerification =
        RuntimeStateVerifier::verifyManifestMatchesRuntime(manifest, runtime);

    if (!runtimeVerification.verified()) {
        return ChainAuditResult::failed(runtimeVerification.reason());
    }

    const ProtocolInvariantCheckResult invariantCheck =
        ProtocolInvariantChecker::checkRuntimeAgainstManifest(runtime, manifest);

    if (!invariantCheck.passed()) {
        return ChainAuditResult::failed(invariantCheck.reason());
    }

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(manifest.networkName());

    if (!cryptoContext.isValid()) {
        return ChainAuditResult::failed(
            "invalid crypto context for network " + manifest.networkName() +
            ": " + cryptoContext.rejectionReason()
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
        return ChainAuditResult::failed(
            "manifest validatorCount does not match runtime validator registry"
        );
    }

    const auto& finalizedDeltas = runtime.supplyState().finalizedDeltas();

    economics::MonetaryPolicy policy;
    utils::Amount genesisSupply;

    if (!finalizedDeltas.empty()) {
        try {
            genesisSupply = MonetaryFirewall::genesisSupply(
                runtime.config().genesisConfig()
            );
        } catch (const std::exception& e) {
            return ChainAuditResult::failed(
                std::string("chain audit: cannot determine genesis supply: ") + e.what()
            );
        }

        policy = economics::MonetaryPolicy::localnetDefault(
            runtime.config().genesisConfig().networkParameters().chainId(),
            genesisSupply
        );

        // Supply continuity audit: verify finalized SupplyDeltas form a valid chain.
        const FinalizedSupplyAuditResult supplyAudit =
            FinalizedSupplyAudit::auditDeltas(policy, finalizedDeltas);

        if (!supplyAudit.passed()) {
            const MonetaryAuditDiagnostic diag =
                MonetaryAuditDiagnostic::supplyContinuityFailure(
                    supplyAudit.reason(),
                    supplyAudit.failedBlockHeight(),
                    supplyAudit.expectedSupply(),
                    supplyAudit.actualSupply(),
                    supplyAudit.finalSupply()
                );
            return ChainAuditResult::failed(
                "chain audit: monetary supply continuity failure: " +
                supplyAudit.reason() + " | " + diag.serialize()
            );
        }

        // Epoch monetary report verification: rebuild the report from finalized
        // deltas. This proves the delta sequence is self-consistent.
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

        // Report path is required when finalized deltas exist.
        if (monetaryReportPath.empty()) {
            if (requireReport) {
                const MonetaryAuditDiagnostic diag =
                    MonetaryAuditDiagnostic::reportMissing(
                        "monetary report verification is required but no report path was provided",
                        0
                    );
                return ChainAuditResult::failed(
                    "chain audit: monetary report path required when finalized deltas exist"
                    " | " + diag.serialize()
                );
            }
            // Dev-mode: skip report verification when path is empty.
        } else {
            if (!std::filesystem::exists(monetaryReportPath)) {
                const MonetaryAuditDiagnostic diag =
                    MonetaryAuditDiagnostic::reportMissing(
                        "expected persisted monetary report not found at: " +
                        monetaryReportPath.string(),
                        0
                    );
                return ChainAuditResult::failed(
                    "chain audit: persisted monetary report missing: " +
                    monetaryReportPath.string() + " | " + diag.serialize()
                );
            }

            economics::EpochMonetaryReport persistedReport;
            try {
                persistedReport = EpochMonetaryReportStore::read(
                    monetaryReportPath, policy
                );
            } catch (const std::exception& e) {
                const MonetaryAuditDiagnostic diag =
                    MonetaryAuditDiagnostic::reportMissing(
                        std::string("failed to decode persisted monetary report: ") + e.what(),
                        0
                    );
                return ChainAuditResult::failed(
                    std::string("chain audit: persisted monetary report decode failed: ") +
                    e.what() + " | " + diag.serialize()
                );
            }

            const MonetaryReportVerificationResult verif =
                MonetaryReportVerifier::verify(persistedReport, rebuiltReport);

            if (!verif.matched()) {
                return ChainAuditResult::failed(
                    "chain audit: persisted monetary report does not match rebuilt report: " +
                    verif.reason() + " | " + verif.serialize()
                );
            }
        }

        // Treasury section audit: validate spend records in all loaded artifacts.
        const FinalizedTreasuryAuditResult treasuryAudit =
            FinalizedTreasuryAudit::auditArtifacts(0, load.loadedArtifacts());

        if (!treasuryAudit.passed()) {
            return ChainAuditResult::failed(
                "chain audit: treasury section audit failed: " + treasuryAudit.reason()
            );
        }

        // Treasury report verification.
        if (!treasuryReportPath.empty()) {
            if (!std::filesystem::exists(treasuryReportPath)) {
                return ChainAuditResult::failed(
                    "chain audit: expected persisted treasury report not found at: " +
                    treasuryReportPath.string()
                );
            }

            economics::EpochTreasuryReport persistedTreasuryReport;
            try {
                persistedTreasuryReport = EpochTreasuryReportStore::read(treasuryReportPath);
            } catch (const std::exception& e) {
                return ChainAuditResult::failed(
                    std::string("chain audit: treasury report decode failed: ") + e.what()
                );
            }

            const EpochTreasuryVerificationResult treasuryVerif =
                EpochTreasuryReportVerifier::verify(
                    persistedTreasuryReport,
                    treasuryAudit.rebuiltReport()
                );

            if (!treasuryVerif.matched()) {
                return ChainAuditResult::failed(
                    "chain audit: persisted treasury report does not match rebuilt report: " +
                    treasuryVerif.reason()
                );
            }
        }
    }

    ChainAuditResult result = ChainAuditResult::passed(
        manifest.networkName(),
        cryptoContext.networkProfile(),
        manifest.latestBlockHeight(),
        manifest.latestBlockHash(),
        manifest.latestStateRoot(),
        load.loadedBlockCount(),
        load.loadedMempoolTransactionCount(),
        manifest.validatorCount()
    );

    // Compute a deterministic audit assignment for the latest artifact.
    // Seed = previous block hash (already-finalized data) to prevent manipulation.
    // An empty or insufficient validator set produces no assignment (not an error).
    const auto& artifacts = load.loadedArtifacts();
    if (!artifacts.empty()) {
        const FinalizedBlockArtifact& latest = artifacts.back();
        const std::string seedDigest = manifest.latestBlockHash();
        const std::vector<std::string> validatorAddresses =
            runtime.validatorRegistry().activeValidatorAddresses();

        if (!seedDigest.empty() && !validatorAddresses.empty()) {
            const std::string assignmentId =
                "chain-audit-assignment:" + seedDigest;
            const AuditAssignment assignment =
                AuditAssignmentCalculator::buildAssignment(
                    assignmentId,
                    manifest.latestBlockHeight(),
                    0,
                    AuditAssignmentTargetType::BLOCK_ARTIFACT,
                    latest.artifactDigest(),
                    seedDigest,
                    validatorAddresses
                );
            if (assignment.isValid()) {
                result.setAuditAssignment(std::move(assignment));
            }
        }
    }

    return result;
}

} // namespace

ChainAuditResult ChainAuditor::auditLoadedRuntime(
    const RuntimeStateLoadResult& load,
    const std::filesystem::path& monetaryReportPath,
    const std::filesystem::path& treasuryReportPath
) {
    return auditImpl(load, monetaryReportPath, treasuryReportPath, true);
}

ChainAuditResult ChainAuditor::auditLoadedRuntimeDevMode(
    const RuntimeStateLoadResult& load
) {
    return auditImpl(load, {}, {}, false);
}

} // namespace nodo::node
