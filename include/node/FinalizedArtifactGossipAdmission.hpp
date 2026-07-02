#ifndef NODO_NODE_FINALIZED_ARTIFACT_GOSSIP_ADMISSION_HPP
#define NODO_NODE_FINALIZED_ARTIFACT_GOSSIP_ADMISSION_HPP

#include <string>

namespace nodo::crypto {
class CryptoPolicy;
class SignatureProvider;
}

namespace nodo::p2p {
class NetworkEnvelope;
}

namespace nodo::node {

class FinalizedBlockRecordStore;
class NodeRuntime;

enum class FinalizedArtifactGossipAdmissionStatus {
    ACCEPTED,
    DUPLICATE,
    WRONG_MESSAGE_TYPE,
    NETWORK_CONTEXT_MISMATCH,
    EMPTY_PAYLOAD,
    MALFORMED_PAYLOAD,
    INVALID_RECORD,
    BLOCK_UNAVAILABLE,
    BLOCK_MISMATCH,
    VALIDATOR_SET_UNAVAILABLE,
    INVALID_QUORUM_CERTIFICATE,
    CONFLICTING_FINALIZATION,
    PERSISTENCE_FAILED,
    REGISTRATION_FAILED
};

class FinalizedArtifactGossipAdmissionResult {
public:
    static FinalizedArtifactGossipAdmissionResult accepted();
    static FinalizedArtifactGossipAdmissionResult duplicate();
    static FinalizedArtifactGossipAdmissionResult rejected(
        FinalizedArtifactGossipAdmissionStatus status,
        std::string reason
    );

    FinalizedArtifactGossipAdmissionStatus status() const;
    const std::string& reason() const;

    bool acceptedRecord() const;
    bool duplicateRecord() const;
    bool fatalConsistencyError() const;

private:
    FinalizedArtifactGossipAdmissionStatus m_status =
        FinalizedArtifactGossipAdmissionStatus::INVALID_RECORD;
    std::string m_reason;
};

/*
 * Validates one FINALIZED_BLOCK_ARTIFACT after it has traversed the gossip
 * receive path. The admission boundary deliberately owns no transport state:
 * it verifies the envelope against the runtime network context, checks the
 * exact canonical block and historical validator set, persists the QC, and
 * only then mutates the in-memory finalization registry.
 */
class FinalizedArtifactGossipAdmission {
public:
    static FinalizedArtifactGossipAdmissionResult admit(
        const p2p::NetworkEnvelope& envelope,
        NodeRuntime& runtime,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        const FinalizedBlockRecordStore& store
    );
};

} // namespace nodo::node

#endif
