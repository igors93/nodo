#ifndef NODO_NODE_LOCAL_ARTIFACT_IMPORT_SERVICE_HPP
#define NODO_NODE_LOCAL_ARTIFACT_IMPORT_SERVICE_HPP

#include "config/NetworkParameters.hpp"
#include "node/FinalizedBlockArtifact.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"

#include <filesystem>
#include <string>

namespace nodo::node {

enum class ArtifactImportRejectionReason {
    NONE,
    INVALID_CONFIG,
    DIRECTORY_NOT_INITIALIZED,
    GENESIS_MISMATCH,
    DECODE_FAILED,
    INVALID_ARTIFACT,
    HEIGHT_CONTINUITY_MISMATCH,
    PREVIOUS_HASH_MISMATCH,
    ARTIFACT_DIGEST_EMPTY,
    ARTIFACT_DIGEST_UNSTABLE,
    SUPPLY_CONTINUITY_BREAK,
    ARTIFACT_VALIDATION_FAILED,
    FINALITY_VALIDATION_FAILED,
    REWARD_EVIDENCE_MISSING,
    TREASURY_DIGEST_MISMATCH,
    CONFLICTING_ARTIFACT,
    PERSIST_FAILED
};

std::string artifactImportRejectionReasonToString(ArtifactImportRejectionReason reason);

class FinalizedArtifactImportResult {
public:
    FinalizedArtifactImportResult();

    static FinalizedArtifactImportResult accepted(NodeRuntimeManifest manifest);
    static FinalizedArtifactImportResult rejected(
        ArtifactImportRejectionReason reason,
        std::string detail
    );

    bool accepted() const;
    ArtifactImportRejectionReason rejectionReason() const;
    const std::string& detail() const;
    const NodeRuntimeManifest& manifest() const;

    std::string serialize() const;

private:
    bool m_accepted;
    ArtifactImportRejectionReason m_rejectionReason;
    std::string m_detail;
    NodeRuntimeManifest m_manifest;
};

/*
 * LocalArtifactImportService validates and imports a finalized artifact
 * produced by another local node into the target node's data directory.
 *
 * Security principle:
 * An artifact is never written to disk before passing full protocol
 * validation. If any validation fails the runtime state is not modified.
 * The import validates height continuity, previous hash continuity,
 * artifact digest stability, supply delta continuity, reward evidence,
 * and the full FinalizedArtifactValidator pipeline.
 */
class LocalArtifactImportService {
public:
    // Import a finalized artifact from a source file on another local node.
    // Reads the file, decodes it, validates it, and if valid persists it
    // in the target node's data directory. The runtime is updated only
    // after the file is written successfully.
    static FinalizedArtifactImportResult importArtifactFromFile(
        const NodeDataDirectoryConfig& targetDir,
        NodeRuntime& runtime,
        const config::GenesisConfig& genesisConfig,
        const std::filesystem::path& sourceArtifactPath,
        std::int64_t importedAt
    );

    // Import a pre-decoded artifact. rawArtifactContents must be the
    // byte-identical encoding that will be stored on disk (as produced by
    // FinalizedBlockStore on the source node). Used when receiving via P2P.
    static FinalizedArtifactImportResult importArtifact(
        const NodeDataDirectoryConfig& targetDir,
        NodeRuntime& runtime,
        const config::GenesisConfig& genesisConfig,
        const FinalizedBlockArtifact& artifact,
        const std::string& rawArtifactContents,
        std::int64_t importedAt
    );
};

} // namespace nodo::node

#endif
