#ifndef NODO_P2P_FINALIZED_ARTIFACT_SYNC_SERVICE_HPP
#define NODO_P2P_FINALIZED_ARTIFACT_SYNC_SERVICE_HPP

#include "config/GenesisRegistry.hpp"
#include "node/FinalizedBlockArtifact.hpp"
#include "node/LocalArtifactImportService.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "p2p/FinalizedArtifactSyncMessages.hpp"

#include <string>

namespace nodo::p2p {

enum class FinalizedArtifactSyncRejectionReason {
    NONE,
    MALFORMED_ANNOUNCEMENT,
    MALFORMED_REQUEST,
    MALFORMED_RESPONSE,
    NETWORK_MISMATCH,
    CHAIN_MISMATCH,
    GENESIS_MISMATCH,
    DIGEST_MISMATCH,
    DECODE_FAILED,
    IMPORT_FAILED
};

std::string finalizedArtifactSyncRejectionReasonToString(
    FinalizedArtifactSyncRejectionReason reason
);

/*
 * FinalizedArtifactSyncResult carries the outcome of a sync operation.
 */
class FinalizedArtifactSyncResult {
public:
    FinalizedArtifactSyncResult();

    static FinalizedArtifactSyncResult accepted(
        node::FinalizedArtifactImportResult importResult
    );

    static FinalizedArtifactSyncResult rejected(
        FinalizedArtifactSyncRejectionReason reason,
        std::string detail
    );

    bool accepted() const;
    FinalizedArtifactSyncRejectionReason rejectionReason() const;
    const std::string& detail() const;
    const node::FinalizedArtifactImportResult& importResult() const;

    std::string serialize() const;

private:
    bool m_accepted;
    FinalizedArtifactSyncRejectionReason m_rejectionReason;
    std::string m_detail;
    node::FinalizedArtifactImportResult m_importResult;
};

/*
 * FinalizedArtifactPayloadParser provides structural validation and field
 * extraction for the three finalized artifact sync message types.
 *
 * Security principle:
 * All parsing is structural. Fields are extracted and validated individually.
 * A missing, empty, or mismatched field is a hard rejection, not a warning.
 */
class FinalizedArtifactPayloadParser {
public:
    // Returns false if the announcement is structurally invalid.
    static bool validateAnnouncement(
        const FinalizedArtifactAnnouncement& announcement,
        const std::string& localNetworkId,
        const std::string& localChainId,
        const std::string& localGenesisId,
        std::string& outRejectionReason
    );

    // Returns false if the request is structurally invalid.
    static bool validateRequest(
        const FinalizedArtifactRequest& request,
        const std::string& localNetworkId,
        const std::string& localChainId,
        const std::string& localGenesisId,
        std::string& outRejectionReason
    );

    // Returns false if the response is structurally invalid or digest mismatches.
    static bool validateResponse(
        const FinalizedArtifactResponse& response,
        const std::string& localNetworkId,
        const std::string& localChainId,
        const std::string& localGenesisId,
        const std::string& expectedDigest,
        std::string& outRejectionReason
    );
};

/*
 * FinalizedArtifactSyncService connects finalized artifact P2P sync messages
 * to the official import validation pipeline.
 *
 * Security principle:
 * No artifact content received over P2P is accepted without passing the full
 * LocalArtifactImportService validation. Raw content is never written directly.
 * Rejected sync input is recorded via the rejection reason without mutating state.
 */
class FinalizedArtifactSyncService {
public:
    // Process a finalized artifact response received from a peer.
    // Validates the response structurally, checks network and genesis identity,
    // verifies the artifact digest, decodes the content through the canonical
    // codec, and routes the candidate to LocalArtifactImportService.
    //
    // Returns accepted() only when the artifact passes the full import pipeline.
    // A rejected result never mutates runtime or data directory state.
    static FinalizedArtifactSyncResult processResponse(
        const FinalizedArtifactResponse& response,
        const std::string& localNetworkId,
        const std::string& localChainId,
        const std::string& localGenesisId,
        const std::string& expectedDigest,
        const node::NodeDataDirectoryConfig& targetDir,
        node::NodeRuntime& runtime,
        const config::GenesisConfig& genesisConfig,
        std::int64_t importedAt
    );
};

} // namespace nodo::p2p

#endif
