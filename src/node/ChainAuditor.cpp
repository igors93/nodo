#include "node/ChainAuditor.hpp"

#include "node/AuditAssignment.hpp"
#include "node/DataAvailabilityAuditValidator.hpp"
#include "node/ProtectionRewards.hpp"
#include "crypto/KeyEncryptionPolicy.hpp"
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
#include <set>
#include <utility>

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

        // Reward evidence audit: every protection reward settlement must carry
        // verifiable work evidence. Extraordinary rewards are not permitted in
        // the current localnet configuration (no extraordinary minting path exists).
        for (const auto& artifact : load.loadedArtifacts()) {
            const RewardEvidenceAuditResult rewardAudit =
                ProtectionRewards::auditSettlementEvidence(
                    artifact.protectionRewardSettlements()
                );
            if (!rewardAudit.isPassed()) {
                return ChainAuditResult::failed(
                    "chain audit: reward evidence audit failed at block " +
                    std::to_string(artifact.block().index()) + ": " +
                    rewardAudit.reason()
                );
            }

            // Artifact digest stability check: verify that the artifact's
            // recomputed digest is non-empty and internally consistent.
            // A corrupted artifact would produce an empty or mismatched digest.
            if (artifact.isValid()) {
                const std::string recomputed = artifact.artifactDigest();
                if (recomputed.empty()) {
                    return ChainAuditResult::failed(
                        "chain audit: artifact at block " +
                        std::to_string(artifact.block().index()) +
                        " produced an empty digest"
                    );
                }
                // If a second recomputation differs, the artifact is unstable.
                if (artifact.artifactDigest() != recomputed) {
                    return ChainAuditResult::failed(
                        "chain audit: artifact digest is non-deterministic at block " +
                        std::to_string(artifact.block().index())
                    );
                }
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

        // Per-record integrity verification across all loaded artifacts.
        //
        // These checks catch tampering that aggregate-supply audits miss:
        // duplicate slash evidence, counts inconsistent with summaries, and
        // structurally invalid reward or lifecycle records.

        // A. Cross-artifact slash evidence deduplication.
        // The same (validatorAddress, blockHeight) pair must not appear in more
        // than one artifact — that would mean the same misbehaviour was used to
        // slash twice.
        {
            std::set<std::pair<std::string, std::uint64_t>> seenSlashEvidence;
            for (const auto& artifact : load.loadedArtifacts()) {
                for (const auto& record : artifact.slashingEvidenceRecords()) {
                    const auto key = std::make_pair(
                        record.validatorAddress(),
                        record.blockHeight()
                    );
                    if (!seenSlashEvidence.insert(key).second) {
                        return ChainAuditResult::failed(
                            "chain audit: duplicate slash evidence for validator " +
                            record.validatorAddress() + " at height " +
                            std::to_string(record.blockHeight()) +
                            " detected in artifact at block " +
                            std::to_string(artifact.block().index()) +
                            ". The same misbehaviour record must not appear in "
                            "more than one finalized artifact."
                        );
                    }
                }
            }
        }

        // B. Intra-artifact slash evidence summary consistency.
        // The summary's evidenceCount must equal the number of actual records and
        // slashableEvidenceCount must not exceed evidenceCount.
        for (const auto& artifact : load.loadedArtifacts()) {
            const auto& summary = artifact.slashingEvidenceSummary();
            if (!summary.active()) {
                continue;
            }

            const auto actualEvidenceCount =
                static_cast<std::uint64_t>(artifact.slashingEvidenceRecords().size());

            if (summary.evidenceCount() != actualEvidenceCount) {
                return ChainAuditResult::failed(
                    "chain audit: slash evidence summary count mismatch at block " +
                    std::to_string(artifact.block().index()) +
                    ": summary declares " + std::to_string(summary.evidenceCount()) +
                    " evidence records but " + std::to_string(actualEvidenceCount) +
                    " are present in the artifact."
                );
            }

            if (summary.slashableEvidenceCount() > summary.evidenceCount()) {
                return ChainAuditResult::failed(
                    "chain audit: slash evidence summary at block " +
                    std::to_string(artifact.block().index()) +
                    " claims more slashable records (" +
                    std::to_string(summary.slashableEvidenceCount()) +
                    ") than total evidence records (" +
                    std::to_string(summary.evidenceCount()) + ")."
                );
            }
        }

        // C. Intra-artifact validator lifecycle summary consistency.
        // active + jailed + slashed counts in the summary must equal the total
        // number of ValidatorLifecycleRecord entries in the same artifact.
        for (const auto& artifact : load.loadedArtifacts()) {
            const auto& lifecycleSummary = artifact.validatorLifecycleSummary();
            if (!lifecycleSummary.active()) {
                continue;
            }

            const std::uint64_t summaryTotal =
                lifecycleSummary.activeValidatorCount() +
                lifecycleSummary.jailedValidatorCount() +
                lifecycleSummary.slashedValidatorCount();

            const auto recordCount =
                static_cast<std::uint64_t>(artifact.validatorLifecycleRecords().size());

            if (summaryTotal != recordCount) {
                return ChainAuditResult::failed(
                    "chain audit: validator lifecycle summary count mismatch at block " +
                    std::to_string(artifact.block().index()) +
                    ": active(" +
                    std::to_string(lifecycleSummary.activeValidatorCount()) +
                    ") + jailed(" +
                    std::to_string(lifecycleSummary.jailedValidatorCount()) +
                    ") + slashed(" +
                    std::to_string(lifecycleSummary.slashedValidatorCount()) +
                    ") = " + std::to_string(summaryTotal) +
                    " but " + std::to_string(recordCount) +
                    " lifecycle records are present."
                );
            }

            // Each lifecycle record must be individually valid.
            for (const auto& rec : artifact.validatorLifecycleRecords()) {
                if (!rec.isValid()) {
                    return ChainAuditResult::failed(
                        "chain audit: invalid validator lifecycle record for " +
                        rec.validatorAddress() + " at block " +
                        std::to_string(artifact.block().index()) + "."
                    );
                }
            }
        }

        // D. Intra-artifact reward distribution validity.
        // Every RewardDistribution in each artifact must pass its own isValid()
        // guard.  A structurally invalid distribution indicates tampering or a
        // serialisation bug.
        for (const auto& artifact : load.loadedArtifacts()) {
            for (const auto& dist : artifact.rewardDistributions()) {
                if (!dist.isValid()) {
                    return ChainAuditResult::failed(
                        "chain audit: structurally invalid reward distribution "
                        "for validator " + dist.validatorAddress() +
                        " at block " + std::to_string(artifact.block().index()) +
                        ". Reward distributions must satisfy their own validity "
                        "contract after deserialization."
                    );
                }
            }
        }

        // E. Intra-artifact locked stake position validity.
        // Every LockedStakePosition embedded in a finalized artifact must
        // satisfy its own validity contract. A structurally invalid position
        // indicates tampering or a serialisation bug in the stake lock
        // lifecycle.
        for (const auto& artifact : load.loadedArtifacts()) {
            for (const auto& position : artifact.lockedStakePositions()) {
                if (!position.isValid()) {
                    return ChainAuditResult::failed(
                        "chain audit: structurally invalid locked stake position "
                        "for owner " + position.ownerAddress() +
                        " at block " + std::to_string(artifact.block().index()) +
                        ". Locked stake positions must satisfy their own "
                        "validity contract after deserialization."
                    );
                }
            }
        }

        // F. Intra-artifact cryptographic slashing evidence and summary
        // consistency. Mirrors the double-vote/proposer-equivocation slash
        // evidence checks above (B), but for the cryptographic double-vote
        // evidence path and its penalty summary.
        for (const auto& artifact : load.loadedArtifacts()) {
            for (const auto& record : artifact.cryptographicSlashingEvidenceRecords()) {
                if (!record.isValid()) {
                    return ChainAuditResult::failed(
                        "chain audit: structurally invalid cryptographic "
                        "slashing evidence for validator " +
                        record.validatorAddress() + " at block " +
                        std::to_string(artifact.block().index()) + "."
                    );
                }
            }

            const auto& cryptoSummary = artifact.cryptographicSlashingSummary();
            if (!cryptoSummary.active()) {
                continue;
            }

            const auto actualCryptoEvidenceCount = static_cast<std::uint64_t>(
                artifact.cryptographicSlashingEvidenceRecords().size()
            );

            if (cryptoSummary.evidenceCount() != actualCryptoEvidenceCount) {
                return ChainAuditResult::failed(
                    "chain audit: cryptographic slashing summary count "
                    "mismatch at block " +
                    std::to_string(artifact.block().index()) +
                    ": summary declares " +
                    std::to_string(cryptoSummary.evidenceCount()) +
                    " evidence records but " +
                    std::to_string(actualCryptoEvidenceCount) +
                    " are present in the artifact."
                );
            }

            if (cryptoSummary.slashableEvidenceCount() > cryptoSummary.evidenceCount()) {
                return ChainAuditResult::failed(
                    "chain audit: cryptographic slashing summary at block " +
                    std::to_string(artifact.block().index()) +
                    " claims more slashable records (" +
                    std::to_string(cryptoSummary.slashableEvidenceCount()) +
                    ") than total evidence records (" +
                    std::to_string(cryptoSummary.evidenceCount()) + ")."
                );
            }
        }

        // G. Intra-artifact stake penalty record validity.
        // Every StakePenaltyRecord must be individually valid; this is the
        // auditable trail proving a slash was applied to a specific stake
        // lock rather than fabricated after the fact.
        for (const auto& artifact : load.loadedArtifacts()) {
            for (const auto& penalty : artifact.stakePenaltyRecords()) {
                if (!penalty.isValid()) {
                    return ChainAuditResult::failed(
                        "chain audit: structurally invalid stake penalty "
                        "record for validator " + penalty.validatorAddress() +
                        " at block " +
                        std::to_string(artifact.block().index()) + "."
                    );
                }
            }
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

            // Official networks must not accept treasury reports that are missing
            // the spend records digest. A report without a digest can only be
            // verified at total level, which hides changes to individual recipient
            // addresses, proposal IDs, or payment amounts behind a matching sum.
            if (crypto::KeyEncryptionPolicy::isOfficialNetwork(manifest.networkName()) &&
                persistedTreasuryReport.spendRecordsDigest().empty()) {
                return ChainAuditResult::failed(
                    "chain audit: treasury report on official network '" +
                    manifest.networkName() +
                    "' is missing the spend records digest. "
                    "The report must be regenerated with record-level verification "
                    "before this node can participate on an official network."
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
