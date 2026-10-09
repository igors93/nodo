#include "../../common/TestFramework.hpp"

#include "node/history/HistorySyncMessages.hpp"

#include <iostream>
#include <random>

using nodo::test::require;
using namespace nodo::node;

namespace {

const std::string kId(64, 'a');

std::vector<HistorySyncMessage> samples() {
  std::vector<HistorySyncMessage> messages;
  messages.push_back(CheckpointRequest{1, 0});
  messages.push_back(CheckpointResponse{2, {1, 2, 3}});
  messages.push_back(SnapshotManifestRequest{3, kId});
  messages.push_back(SnapshotManifestResponse{4, kId, 16, kId, 4096, 1024});
  messages.push_back(SnapshotChunkRequest{5, kId, 0});
  messages.push_back(SnapshotChunkResponse{6, 0, 4, {9, 9}, {kId, kId}});
  messages.push_back(HistoryRangeRequest{7, 9, 4});
  messages.push_back(ArchiveIndexRequest{8, 0, 16});
  messages.push_back(ArchiveIndexResponse{9, {{1}, {2}}});
  messages.push_back(ArchiveProviderAnnouncement{{7, 7}});
  messages.push_back(ArchiveSegmentsAnnouncement{
      "nodo1provider", {{0, 3}, {5, 9}}, 1900000000, {1, 2}});
  messages.push_back(ArchivalChallengeMessage{{4}});
  messages.push_back(ArchivalProofMessage{{5}});
  messages.push_back(ReplicationStatusRequest{10, 0, 8});
  messages.push_back(ReplicationStatusResponse{11, {{0, 3, 2}, {1, 3, 3}}});
  return messages;
}

bool rejects(const std::vector<unsigned char> &bytes) {
  try {
    (void)HistorySyncCodec::decode(bytes);
    return false;
  } catch (const std::exception &) {
    return true;
  }
}

bool encodeRejects(const HistorySyncMessage &message) {
  try {
    (void)HistorySyncCodec::encode(message);
    return false;
  } catch (const std::exception &) {
    return true;
  }
}

void testRoundTrip() {
  std::uint16_t tag = 1;
  for (const HistorySyncMessage &message : samples()) {
    require(static_cast<std::uint16_t>(HistorySyncCodec::typeOf(message)) == tag++,
            "variant order mirrors the wire tags");
    const auto bytes = HistorySyncCodec::encode(message);
    require(HistorySyncCodec::encode(HistorySyncCodec::decode(bytes)) == bytes,
            historySyncMessageTypeToString(HistorySyncCodec::typeOf(message)) +
                " round-trips");
    auto trailing = bytes;
    trailing.push_back(0);
    require(rejects(trailing), "trailing bytes are rejected");
    require(rejects(std::vector<unsigned char>(bytes.begin(), bytes.end() - 1)),
            "truncation is rejected");
  }
}

void testLimits() {
  require(rejects({}) && rejects({0, 0}) && rejects({0, 99}),
          "empty and unknown messages are rejected");
  require(rejects(std::vector<unsigned char>(HistorySyncLimits::kMaxMessageBytes + 1, 1)),
          "oversized messages are rejected before parsing");
  require(encodeRejects(HistoryRangeRequest{1, 9, 5}) &&
              encodeRejects(HistoryRangeRequest{1, 0, 1}) &&
              encodeRejects(HistoryRangeRequest{1, 9, 0}),
          "range requests are 1-4 blocks after genesis");
  require(encodeRejects(CheckpointRequest{0, 1}), "request ids are non-zero");
  require(encodeRejects(SnapshotManifestRequest{1, "../../etc/passwd"}),
          "ids are hashes, never paths");
  require(encodeRejects(ArchiveIndexRequest{1, 0, 0}) &&
              encodeRejects(ArchiveIndexRequest{
                  1, 0, HistorySyncLimits::kMaxIndexEntries + 1}),
          "index requests are bounded");
  require(encodeRejects(SnapshotChunkResponse{
              1, 0, 1,
              std::vector<unsigned char>(HistorySyncLimits::kMaxChunkBytes + 1, 1),
              {}}),
          "chunks above 256 KiB are rejected");
  require(encodeRejects(ArchiveSegmentsAnnouncement{
              "nodo1provider", {{5, 9}, {7, 12}}, 1900000000, {1}}),
          "overlapping segment ranges are rejected");
  require(encodeRejects(ReplicationStatusResponse{1, {{3, 1, 1}, {3, 1, 1}}}),
          "replication entries must ascend");
  require(encodeRejects(CheckpointResponse{
              1, std::vector<unsigned char>(
                     HistorySyncLimits::kMaxCheckpointBytes + 1, 1)}),
          "oversized embedded checkpoints are rejected");

  // A count field that claims more entries than allowed is rejected before
  // any allocation for them happens.
  auto bytes = HistorySyncCodec::encode(ArchiveIndexResponse{9, {{1}}});
  bytes[2 + 8] = 0xff;
  require(rejects(bytes), "a forged list length is rejected");
}

void testFuzzLikeInputsNeverCrash() {
  std::mt19937 random(1234);
  for (const HistorySyncMessage &message : samples()) {
    const auto bytes = HistorySyncCodec::encode(message);
    for (int round = 0; round < 200; ++round) {
      auto mutated = bytes;
      mutated[random() % mutated.size()] ^= static_cast<unsigned char>(1u << (random() % 8));
      try {
        (void)HistorySyncCodec::decode(mutated);
      } catch (const std::exception &) {
      }
    }
  }
}

void testRequestGuard() {
  HistoryRequestGuard::Limits limits;
  limits.maxOutstandingPerPeer = 2;
  limits.maxServedBytesPerWindow = 1000;
  limits.maxTrackedPeers = 2;
  HistoryRequestGuard guard(limits);
  const auto first =
      guard.openRequest("peer-a", HistorySyncMessageType::CHECKPOINT_REQUEST, 100);
  require(first.has_value(), "a request opens");
  require(!guard.openRequest("peer-a", HistorySyncMessageType::ARCHIVAL_PROOF, 100),
          "only request types open requests");
  require(!guard.acceptResponse("peer-b", *first,
                                HistorySyncMessageType::CHECKPOINT_RESPONSE, 101),
          "a response from another peer is unsolicited");
  require(!guard.acceptResponse("peer-a", *first,
                                HistorySyncMessageType::SNAPSHOT_CHUNK_RESPONSE, 101),
          "a response of the wrong type is unsolicited");
  require(guard.acceptResponse("peer-a", *first,
                               HistorySyncMessageType::CHECKPOINT_RESPONSE, 101),
          "the matching response is accepted");
  require(!guard.acceptResponse("peer-a", *first,
                                HistorySyncMessageType::CHECKPOINT_RESPONSE, 102),
          "a response is accepted once");

  guard.openRequest("peer-a", HistorySyncMessageType::SNAPSHOT_CHUNK_REQUEST, 200);
  const auto late =
      guard.openRequest("peer-a", HistorySyncMessageType::SNAPSHOT_CHUNK_REQUEST, 200);
  require(late.has_value() &&
              !guard.openRequest("peer-a",
                                 HistorySyncMessageType::SNAPSHOT_CHUNK_REQUEST, 200),
          "outstanding requests per peer are capped");
  require(!guard.acceptResponse("peer-a", *late,
                                HistorySyncMessageType::SNAPSHOT_CHUNK_RESPONSE,
                                200 + limits.requestTimeoutSeconds + 1),
          "late responses are rejected");
  require(guard.expire(200 + limits.requestTimeoutSeconds + 1) >= 1 &&
              guard.outstanding("peer-a") == 0,
          "expired requests are released");

  require(guard.admitServe("peer-c", 600, 300) &&
              !guard.admitServe("peer-c", 600, 301),
          "serving is bounded per window (anti-amplification)");
  require(guard.admitServe("peer-c", 600, 300 + limits.windowSeconds),
          "the serving budget refills after the window");
  require(!guard.admitServe("peer-d", 1, 300) || !guard.admitServe("peer-e", 1, 300),
          "tracked peers are capped");
}

} // namespace

int main() {
  try {
    testRoundTrip();
    testLimits();
    testFuzzLikeInputsNeverCrash();
    testRequestGuard();
    std::cout << "History sync message tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "History sync message tests failed: " << error.what() << "\n";
    return 1;
  }
}
