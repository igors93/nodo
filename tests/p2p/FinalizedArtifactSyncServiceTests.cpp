// Tests for FinalizedArtifactSyncService and FinalizedArtifactPayloadParser.
// Covers: malformed announcement rejected, malformed request rejected,
// malformed response rejected, genesis mismatch rejected,
// network mismatch rejected, digest mismatch rejected.

#include "p2p/FinalizedArtifactSyncMessages.hpp"
#include "p2p/FinalizedArtifactSyncService.hpp"

#include <cassert>
#include <iostream>
#include <string>

namespace {

constexpr std::int64_t kTs = 1900400000;

const std::string kNetworkId = "nodo-localnet";
const std::string kChainId = "nodo-localnet-1";
const std::string kGenesisId = "genesis-abc";
const std::string kSenderNode = "node-sender";

// ---- Announcement tests ----

void testValidAnnouncementAccepted() {
  nodo::p2p::FinalizedArtifactAnnouncement ann(
      kSenderNode, kNetworkId, kChainId, kGenesisId, 5, "hash-5", "hash-4",
      "digest-5", kTs);
  assert(ann.isValid());

  std::string reason;
  const bool ok =
      nodo::p2p::FinalizedArtifactPayloadParser::validateAnnouncement(
          ann, kNetworkId, kChainId, kGenesisId, reason);
  assert(ok && reason.empty());
}

void testMalformedAnnouncementRejected() {
  // Empty sender node id makes it invalid.
  nodo::p2p::FinalizedArtifactAnnouncement ann("", kNetworkId, kChainId,
                                               kGenesisId, 5, "hash-5",
                                               "hash-4", "digest-5", kTs);
  assert(!ann.isValid());

  std::string reason;
  const bool ok =
      nodo::p2p::FinalizedArtifactPayloadParser::validateAnnouncement(
          ann, kNetworkId, kChainId, kGenesisId, reason);
  assert(!ok);
  assert(!reason.empty());
}

void testAnnouncementNetworkMismatchRejected() {
  nodo::p2p::FinalizedArtifactAnnouncement ann(
      kSenderNode, "wrong-network", kChainId, kGenesisId, 5, "hash-5", "hash-4",
      "digest-5", kTs);

  std::string reason;
  const bool ok =
      nodo::p2p::FinalizedArtifactPayloadParser::validateAnnouncement(
          ann, kNetworkId, kChainId, kGenesisId, reason);
  assert(!ok);
  assert(reason.find("network") != std::string::npos ||
         reason.find("Network") != std::string::npos);
}

void testAnnouncementGenesisMismatchRejected() {
  nodo::p2p::FinalizedArtifactAnnouncement ann(
      kSenderNode, kNetworkId, kChainId, "wrong-genesis", 5, "hash-5", "hash-4",
      "digest-5", kTs);

  std::string reason;
  const bool ok =
      nodo::p2p::FinalizedArtifactPayloadParser::validateAnnouncement(
          ann, kNetworkId, kChainId, kGenesisId, reason);
  assert(!ok);
  assert(reason.find("genesis") != std::string::npos ||
         reason.find("Genesis") != std::string::npos);
}

// ---- Request tests ----

void testValidRequestAccepted() {
  nodo::p2p::FinalizedArtifactRequest req("node-requester", kNetworkId,
                                          kChainId, kGenesisId, 5, "hash-5",
                                          "digest-5", kTs);
  assert(req.isValid());

  std::string reason;
  const bool ok = nodo::p2p::FinalizedArtifactPayloadParser::validateRequest(
      req, kNetworkId, kChainId, kGenesisId, reason);
  assert(ok && reason.empty());
}

void testMalformedRequestRejected() {
  // Empty requester node id makes it invalid.
  nodo::p2p::FinalizedArtifactRequest req("", kNetworkId, kChainId, kGenesisId,
                                          5, "hash-5", "digest-5", kTs);
  assert(!req.isValid());

  std::string reason;
  const bool ok = nodo::p2p::FinalizedArtifactPayloadParser::validateRequest(
      req, kNetworkId, kChainId, kGenesisId, reason);
  assert(!ok);
}

void testRequestChainMismatchRejected() {
  nodo::p2p::FinalizedArtifactRequest req("node-requester", kNetworkId,
                                          "wrong-chain", kGenesisId, 5,
                                          "hash-5", "digest-5", kTs);

  std::string reason;
  const bool ok = nodo::p2p::FinalizedArtifactPayloadParser::validateRequest(
      req, kNetworkId, kChainId, kGenesisId, reason);
  assert(!ok);
  assert(reason.find("chain") != std::string::npos ||
         reason.find("Chain") != std::string::npos);
}

// ---- Response tests ----

void testMalformedResponseRejected() {
  // A response with no artifact (rejected by the sender) must be rejected by
  // the validator because it has no artifact content to import.
  nodo::p2p::FinalizedArtifactResponse resp =
      nodo::p2p::FinalizedArtifactResponse::rejected(
          kSenderNode,
          nodo::p2p::FinalizedArtifactResponseStatus::ARTIFACT_NOT_FOUND,
          "not found", kTs);

  // A rejected response has no artifact. The payload parser must reject it
  // because validateResponse() requires hasArtifact() to be true.
  assert(!resp.hasArtifact());

  std::string reason;
  const bool ok = nodo::p2p::FinalizedArtifactPayloadParser::validateResponse(
      resp, kNetworkId, kChainId, kGenesisId, "digest-5", reason);
  assert(!ok);
  assert(!reason.empty());
}

void testResponseGenesisMismatchRejected() {
  nodo::p2p::FinalizedArtifactResponse resp =
      nodo::p2p::FinalizedArtifactResponse::withArtifact(
          kSenderNode, kNetworkId, kChainId, "wrong-genesis", 5, "hash-5",
          "digest-5", "raw-content-xyz", kTs);
  assert(resp.isValid());

  std::string reason;
  const bool ok = nodo::p2p::FinalizedArtifactPayloadParser::validateResponse(
      resp, kNetworkId, kChainId, kGenesisId, "digest-5", reason);
  assert(!ok);
  assert(reason.find("genesis") != std::string::npos ||
         reason.find("Genesis") != std::string::npos);
}

void testResponseDigestMismatchRejected() {
  nodo::p2p::FinalizedArtifactResponse resp =
      nodo::p2p::FinalizedArtifactResponse::withArtifact(
          kSenderNode, kNetworkId, kChainId, kGenesisId, 5, "hash-5",
          "digest-5", "raw-content-xyz", kTs);
  assert(resp.isValid());

  std::string reason;
  // Pass a different expected digest — must be rejected.
  const bool ok = nodo::p2p::FinalizedArtifactPayloadParser::validateResponse(
      resp, kNetworkId, kChainId, kGenesisId, "different-digest", reason);
  assert(!ok);
  assert(reason.find("digest") != std::string::npos ||
         reason.find("Digest") != std::string::npos);
}

void testResponseNetworkMismatchRejected() {
  nodo::p2p::FinalizedArtifactResponse resp =
      nodo::p2p::FinalizedArtifactResponse::withArtifact(
          kSenderNode, "wrong-network", kChainId, kGenesisId, 5, "hash-5",
          "digest-5", "raw-content-xyz", kTs);

  std::string reason;
  const bool ok = nodo::p2p::FinalizedArtifactPayloadParser::validateResponse(
      resp, kNetworkId, kChainId, kGenesisId, "digest-5", reason);
  assert(!ok);
}

void testResponseValidStructurePasses() {
  nodo::p2p::FinalizedArtifactResponse resp =
      nodo::p2p::FinalizedArtifactResponse::withArtifact(
          kSenderNode, kNetworkId, kChainId, kGenesisId, 5, "hash-5",
          "digest-5", "raw-content-xyz", kTs);
  assert(resp.isValid());

  std::string reason;
  const bool ok = nodo::p2p::FinalizedArtifactPayloadParser::validateResponse(
      resp, kNetworkId, kChainId, kGenesisId, "digest-5", reason);
  assert(ok && reason.empty());
}

// ---- SyncResult tests ----

void testSyncResultSerializesCorrectly() {
  const auto rejected = nodo::p2p::FinalizedArtifactSyncResult::rejected(
      nodo::p2p::FinalizedArtifactSyncRejectionReason::GENESIS_MISMATCH,
      "test detail");
  assert(!rejected.accepted());
  assert(!rejected.serialize().empty());
  assert(rejected.serialize().find("GENESIS_MISMATCH") != std::string::npos);
}

} // namespace

int main() {
  try {
    testValidAnnouncementAccepted();
    testMalformedAnnouncementRejected();
    testAnnouncementNetworkMismatchRejected();
    testAnnouncementGenesisMismatchRejected();
    testValidRequestAccepted();
    testMalformedRequestRejected();
    testRequestChainMismatchRejected();
    testMalformedResponseRejected();
    testResponseGenesisMismatchRejected();
    testResponseDigestMismatchRejected();
    testResponseNetworkMismatchRejected();
    testResponseValidStructurePasses();
    testSyncResultSerializesCorrectly();

    std::cout << "FinalizedArtifactSyncService tests passed.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FinalizedArtifactSyncService tests failed: " << e.what()
              << "\n";
    return 1;
  }
}
