#include "p2p/FinalizedArtifactSyncMessages.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::p2p::FinalizedArtifactAnnouncement;
using nodo::p2p::FinalizedArtifactRequest;
using nodo::p2p::FinalizedArtifactResponse;
using nodo::p2p::FinalizedArtifactResponseStatus;
using nodo::p2p::finalizedArtifactResponseStatusToString;

void testAnnouncementValidFields() {
    FinalizedArtifactAnnouncement ann(
        "node-a",
        "nodo-localnet",
        "nodo-localnet-1",
        "genesis-id-v1",
        42,
        "block-hash-42",
        "block-hash-41",
        "digest-abc123",
        1000
    );
    assert(ann.isValid());
    assert(ann.senderNodeId() == "node-a");
    assert(ann.networkId() == "nodo-localnet");
    assert(ann.chainId() == "nodo-localnet-1");
    assert(ann.genesisId() == "genesis-id-v1");
    assert(ann.height() == 42);
    assert(ann.blockHash() == "block-hash-42");
    assert(ann.previousBlockHash() == "block-hash-41");
    assert(ann.artifactDigest() == "digest-abc123");
    assert(ann.createdAt() == 1000);
    assert(!ann.serialize().empty());
}

void testAnnouncementInvalidIfMissingFields() {
    FinalizedArtifactAnnouncement ann;
    assert(!ann.isValid());
}

void testAnnouncementInvalidIfMissingGenesisId() {
    FinalizedArtifactAnnouncement ann(
        "node-a", "nodo-localnet", "nodo-localnet-1", "",
        1, "hash", "prev-hash", "digest", 1000
    );
    assert(!ann.isValid());
}

void testRequestValidFields() {
    FinalizedArtifactRequest req(
        "node-requester",
        "nodo-localnet",
        "nodo-localnet-1",
        "genesis-id-v1",
        42,
        "block-hash-42",
        "artifact-digest-42",
        2000
    );
    assert(req.isValid());
    assert(req.requesterNodeId() == "node-requester");
    assert(req.genesisId() == "genesis-id-v1");
    assert(req.height() == 42);
    assert(req.expectedBlockHash() == "block-hash-42");
    assert(req.expectedArtifactDigest() == "artifact-digest-42");
    assert(!req.serialize().empty());
}

void testRequestInvalidIfMissingFields() {
    FinalizedArtifactRequest req;
    assert(!req.isValid());
}

void testRequestInvalidIfMissingGenesisId() {
    FinalizedArtifactRequest req(
        "node-a", "net", "chain", "", 1, "hash", "digest", 1000
    );
    assert(!req.isValid());
}

void testResponseWithArtifact() {
    FinalizedArtifactResponse resp =
        FinalizedArtifactResponse::withArtifact(
            "node-responder",
            "nodo-localnet",
            "nodo-localnet-1",
            "genesis-id-v1",
            42,
            "block-hash-42",
            "artifact-digest-42",
            "RAW_ARTIFACT_DATA",
            3000
        );
    assert(resp.hasArtifact());
    assert(resp.status() == FinalizedArtifactResponseStatus::ARTIFACT_FOUND);
    assert(resp.isValid());
    assert(resp.genesisId() == "genesis-id-v1");
    assert(resp.rawArtifactContents() == "RAW_ARTIFACT_DATA");
    assert(!resp.serialize().empty());
}

void testResponseRejected() {
    FinalizedArtifactResponse resp =
        FinalizedArtifactResponse::rejected(
            "node-responder",
            FinalizedArtifactResponseStatus::GENESIS_MISMATCH,
            "Genesis id does not match local genesis.",
            3000
        );
    assert(!resp.hasArtifact());
    assert(resp.status() == FinalizedArtifactResponseStatus::GENESIS_MISMATCH);
    assert(resp.isValid());
    assert(!resp.serialize().empty());
}

void testStatusToString() {
    assert(finalizedArtifactResponseStatusToString(
        FinalizedArtifactResponseStatus::ARTIFACT_FOUND) == "ARTIFACT_FOUND");
    assert(finalizedArtifactResponseStatusToString(
        FinalizedArtifactResponseStatus::GENESIS_MISMATCH) == "GENESIS_MISMATCH");
    assert(finalizedArtifactResponseStatusToString(
        FinalizedArtifactResponseStatus::NETWORK_MISMATCH) == "NETWORK_MISMATCH");
}

void testAnnouncementSerializeContainsGenesisId() {
    FinalizedArtifactAnnouncement ann(
        "node", "net", "chain", "my-genesis-id", 1, "hash", "prev", "digest", 100
    );
    const std::string s = ann.serialize();
    assert(s.find("my-genesis-id") != std::string::npos);
}

void testRequestSerializeContainsGenesisId() {
    FinalizedArtifactRequest req(
        "req", "net", "chain", "my-genesis-id", 1, "hash", "digest", 100
    );
    const std::string s = req.serialize();
    assert(s.find("my-genesis-id") != std::string::npos);
}

} // namespace

int main() {
    testAnnouncementValidFields();
    testAnnouncementInvalidIfMissingFields();
    testAnnouncementInvalidIfMissingGenesisId();
    testRequestValidFields();
    testRequestInvalidIfMissingFields();
    testRequestInvalidIfMissingGenesisId();
    testResponseWithArtifact();
    testResponseRejected();
    testStatusToString();
    testAnnouncementSerializeContainsGenesisId();
    testRequestSerializeContainsGenesisId();
    return 0;
}
