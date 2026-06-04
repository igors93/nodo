#include "p2p/FinalizedArtifactSyncService.hpp"

#include "node/FinalizedBlockArtifactCodec.hpp"

#include <exception>
#include <sstream>
#include <utility>

namespace nodo::p2p {

// ---------------------------------------------------------------------------
// Free helpers
// ---------------------------------------------------------------------------

std::string finalizedArtifactSyncRejectionReasonToString(
    FinalizedArtifactSyncRejectionReason reason
) {
    switch (reason) {
        case FinalizedArtifactSyncRejectionReason::NONE:
            return "NONE";
        case FinalizedArtifactSyncRejectionReason::MALFORMED_ANNOUNCEMENT:
            return "MALFORMED_ANNOUNCEMENT";
        case FinalizedArtifactSyncRejectionReason::MALFORMED_REQUEST:
            return "MALFORMED_REQUEST";
        case FinalizedArtifactSyncRejectionReason::MALFORMED_RESPONSE:
            return "MALFORMED_RESPONSE";
        case FinalizedArtifactSyncRejectionReason::NETWORK_MISMATCH:
            return "NETWORK_MISMATCH";
        case FinalizedArtifactSyncRejectionReason::CHAIN_MISMATCH:
            return "CHAIN_MISMATCH";
        case FinalizedArtifactSyncRejectionReason::GENESIS_MISMATCH:
            return "GENESIS_MISMATCH";
        case FinalizedArtifactSyncRejectionReason::DIGEST_MISMATCH:
            return "DIGEST_MISMATCH";
        case FinalizedArtifactSyncRejectionReason::DECODE_FAILED:
            return "DECODE_FAILED";
        case FinalizedArtifactSyncRejectionReason::IMPORT_FAILED:
            return "IMPORT_FAILED";
        default:
            return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// FinalizedArtifactSyncResult
// ---------------------------------------------------------------------------

FinalizedArtifactSyncResult::FinalizedArtifactSyncResult()
    : m_accepted(false),
      m_rejectionReason(FinalizedArtifactSyncRejectionReason::MALFORMED_RESPONSE),
      m_detail("Uninitialized sync result."),
      m_importResult() {}

FinalizedArtifactSyncResult FinalizedArtifactSyncResult::accepted(
    node::FinalizedArtifactImportResult importResult
) {
    FinalizedArtifactSyncResult r;
    r.m_accepted = true;
    r.m_rejectionReason = FinalizedArtifactSyncRejectionReason::NONE;
    r.m_detail.clear();
    r.m_importResult = std::move(importResult);
    return r;
}

FinalizedArtifactSyncResult FinalizedArtifactSyncResult::rejected(
    FinalizedArtifactSyncRejectionReason reason,
    std::string detail
) {
    FinalizedArtifactSyncResult r;
    r.m_accepted = false;
    r.m_rejectionReason = reason;
    r.m_detail = std::move(detail);
    return r;
}

bool FinalizedArtifactSyncResult::accepted() const { return m_accepted; }

FinalizedArtifactSyncRejectionReason
FinalizedArtifactSyncResult::rejectionReason() const {
    return m_rejectionReason;
}

const std::string& FinalizedArtifactSyncResult::detail() const {
    return m_detail;
}

const node::FinalizedArtifactImportResult&
FinalizedArtifactSyncResult::importResult() const {
    return m_importResult;
}

std::string FinalizedArtifactSyncResult::serialize() const {
    std::ostringstream oss;
    oss << "FinalizedArtifactSyncResult{"
        << "accepted=" << (m_accepted ? "true" : "false")
        << ";reason=" << finalizedArtifactSyncRejectionReasonToString(m_rejectionReason)
        << ";detail=" << m_detail
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// FinalizedArtifactPayloadParser
// ---------------------------------------------------------------------------

bool FinalizedArtifactPayloadParser::validateAnnouncement(
    const FinalizedArtifactAnnouncement& announcement,
    const std::string& localNetworkId,
    const std::string& localChainId,
    const std::string& localGenesisId,
    std::string& outRejectionReason
) {
    if (!announcement.isValid()) {
        outRejectionReason =
            "Finalized artifact announcement is structurally invalid: "
            "one or more required fields are empty or zero.";
        return false;
    }

    if (announcement.networkId() != localNetworkId) {
        outRejectionReason =
            "Announcement network id '" + announcement.networkId() +
            "' does not match local network '" + localNetworkId + "'.";
        return false;
    }

    if (announcement.chainId() != localChainId) {
        outRejectionReason =
            "Announcement chain id '" + announcement.chainId() +
            "' does not match local chain '" + localChainId + "'.";
        return false;
    }

    if (!localGenesisId.empty() &&
        announcement.genesisId() != localGenesisId) {
        outRejectionReason =
            "Announcement genesis id '" + announcement.genesisId() +
            "' does not match local genesis '" + localGenesisId + "'.";
        return false;
    }

    outRejectionReason.clear();
    return true;
}

bool FinalizedArtifactPayloadParser::validateRequest(
    const FinalizedArtifactRequest& request,
    const std::string& localNetworkId,
    const std::string& localChainId,
    const std::string& localGenesisId,
    std::string& outRejectionReason
) {
    if (!request.isValid()) {
        outRejectionReason =
            "Finalized artifact request is structurally invalid: "
            "one or more required fields are empty or zero.";
        return false;
    }

    if (request.networkId() != localNetworkId) {
        outRejectionReason =
            "Request network id '" + request.networkId() +
            "' does not match local network '" + localNetworkId + "'.";
        return false;
    }

    if (request.chainId() != localChainId) {
        outRejectionReason =
            "Request chain id '" + request.chainId() +
            "' does not match local chain '" + localChainId + "'.";
        return false;
    }

    if (!localGenesisId.empty() &&
        request.genesisId() != localGenesisId) {
        outRejectionReason =
            "Request genesis id '" + request.genesisId() +
            "' does not match local genesis '" + localGenesisId + "'.";
        return false;
    }

    outRejectionReason.clear();
    return true;
}

bool FinalizedArtifactPayloadParser::validateResponse(
    const FinalizedArtifactResponse& response,
    const std::string& localNetworkId,
    const std::string& localChainId,
    const std::string& localGenesisId,
    const std::string& expectedDigest,
    std::string& outRejectionReason
) {
    // A response without an artifact cannot be imported regardless of structural validity.
    if (!response.hasArtifact()) {
        outRejectionReason =
            "Finalized artifact response carries no artifact (status: " +
            finalizedArtifactResponseStatusToString(response.status()) + "). "
            "Only responses with ARTIFACT_FOUND status carry importable content.";
        return false;
    }

    if (!response.isValid()) {
        outRejectionReason =
            "Finalized artifact response is structurally invalid: "
            "one or more required fields are empty.";
        return false;
    }

    if (response.networkId() != localNetworkId) {
        outRejectionReason =
            "Response network id '" + response.networkId() +
            "' does not match local network '" + localNetworkId + "'.";
        return false;
    }

    if (response.chainId() != localChainId) {
        outRejectionReason =
            "Response chain id '" + response.chainId() +
            "' does not match local chain '" + localChainId + "'.";
        return false;
    }

    if (!localGenesisId.empty() &&
        response.genesisId() != localGenesisId) {
        outRejectionReason =
            "Response genesis id '" + response.genesisId() +
            "' does not match local genesis '" + localGenesisId + "'.";
        return false;
    }

    // Digest must match the expected value from the corresponding request/announcement.
    if (!expectedDigest.empty() &&
        response.artifactDigest() != expectedDigest) {
        outRejectionReason =
            "Response artifact digest '" + response.artifactDigest() +
            "' does not match expected digest '" + expectedDigest +
            "' at height " + std::to_string(response.height()) + ".";
        return false;
    }

    outRejectionReason.clear();
    return true;
}

// ---------------------------------------------------------------------------
// FinalizedArtifactSyncService
// ---------------------------------------------------------------------------

FinalizedArtifactSyncResult FinalizedArtifactSyncService::processResponse(
    const FinalizedArtifactResponse& response,
    const std::string& localNetworkId,
    const std::string& localChainId,
    const std::string& localGenesisId,
    const std::string& expectedDigest,
    const node::NodeDataDirectoryConfig& targetDir,
    node::NodeRuntime& runtime,
    const config::GenesisConfig& genesisConfig,
    std::int64_t importedAt
) {
    std::string rejectionReason;

    if (!FinalizedArtifactPayloadParser::validateResponse(
            response,
            localNetworkId,
            localChainId,
            localGenesisId,
            expectedDigest,
            rejectionReason)) {
        return FinalizedArtifactSyncResult::rejected(
            FinalizedArtifactSyncRejectionReason::MALFORMED_RESPONSE,
            rejectionReason
        );
    }

    // Decode the raw artifact content through the canonical codec.
    // Raw content from P2P is never written to disk without a successful decode.
    node::FinalizedBlockArtifact artifact;
    try {
        artifact = node::FinalizedBlockArtifactCodec::decodeBlockArtifactFileContents(
            response.rawArtifactContents()
        );
    } catch (const std::exception& e) {
        return FinalizedArtifactSyncResult::rejected(
            FinalizedArtifactSyncRejectionReason::DECODE_FAILED,
            std::string("Failed to decode artifact content from peer response: ") + e.what()
        );
    }

    // Route the decoded candidate through the official import validation pipeline.
    const node::FinalizedArtifactImportResult importResult =
        node::LocalArtifactImportService::importArtifact(
            targetDir,
            runtime,
            genesisConfig,
            artifact,
            response.rawArtifactContents(),
            importedAt
        );

    if (!importResult.accepted()) {
        return FinalizedArtifactSyncResult::rejected(
            FinalizedArtifactSyncRejectionReason::IMPORT_FAILED,
            "Import pipeline rejected artifact from peer at height " +
            std::to_string(response.height()) + ": " +
            node::artifactImportRejectionReasonToString(importResult.rejectionReason()) +
            " — " + importResult.detail()
        );
    }

    return FinalizedArtifactSyncResult::accepted(importResult);
}

} // namespace nodo::p2p
