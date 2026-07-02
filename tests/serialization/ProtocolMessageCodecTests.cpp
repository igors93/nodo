#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "node/ChainSyncMessages.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "serialization/ProtocolMessageCodec.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

const std::string HASH_A =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const std::string HASH_B =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

nodo::core::LedgerRecord ledgerRecord(const std::string &suffix,
                                      std::int64_t timestamp) {
  return nodo::core::LedgerRecord::fromValidationWorkRecord(
      nodo::economics::ValidationWorkRecord(
          "codec-validator", 1,
          nodo::economics::ValidationWorkType::VALIDATE_BLOCK,
          nodo::economics::ValidationWorkResult::ACCEPTED,
          "codec-target-" + suffix, "codec-evidence-" + suffix, 1, timestamp),
      timestamp + 1);
}

} // namespace

int main() {
  nodo::node::ChainStatusMessage status("nodo-localnet", "nodo-localnet-1",
                                        "nodo/0.1", 12, HASH_A, 10, HASH_B);

  const auto statusBytes =
      nodo::serialization::ProtocolMessageCodec::encodeChainStatusMessage(
          status);
  const auto decodedStatus =
      nodo::serialization::ProtocolMessageCodec::decodeChainStatusMessage(
          statusBytes);

  assert(decodedStatus.isValid());
  assert(decodedStatus.latestHeight() == status.latestHeight());
  assert(decodedStatus.finalizedHeight() == status.finalizedHeight());
  assert(nodo::serialization::ProtocolMessageCodec::hashChainStatusMessage(
             status) ==
         nodo::serialization::ProtocolMessageCodec::hashChainStatusMessage(
             decodedStatus));

  nodo::node::BlockLocator locator(5, 100, {HASH_A, HASH_B});

  const auto locatorBytes =
      nodo::serialization::ProtocolMessageCodec::encodeBlockLocator(locator);
  const auto decodedLocator =
      nodo::serialization::ProtocolMessageCodec::decodeBlockLocator(
          locatorBytes);

  assert(decodedLocator.isValid());
  assert(decodedLocator.fromHeight() == locator.fromHeight());
  assert(decodedLocator.knownAncestorHashes().size() == 2);

  nodo::node::NetworkBlockSyncRequest request("node-a", locator, 1700000000);

  const auto requestBytes =
      nodo::serialization::ProtocolMessageCodec::encodeNetworkBlockSyncRequest(
          request);
  const auto decodedRequest =
      nodo::serialization::ProtocolMessageCodec::decodeNetworkBlockSyncRequest(
          requestBytes);

  assert(decodedRequest.isValid());
  assert(decodedRequest.requesterNodeId() == "node-a");
  assert(decodedRequest.locator().maxBlocks() == 100);

  const nodo::core::Block genesis = nodo::core::Block::createGenesisBlock(
      {ledgerRecord("genesis", 1700000001)}, 1700000003);
  const nodo::core::Block child(
      1, genesis.hash(), {ledgerRecord("child", 1700000004)}, 1700000006);

  const std::vector<nodo::core::Block> blocks = {genesis, child};
  const auto blockListBytes =
      nodo::serialization::ProtocolMessageCodec::encodeBlockList(blocks);
  const auto decodedBlocks =
      nodo::serialization::ProtocolMessageCodec::decodeBlockList(
          blockListBytes);

  assert(decodedBlocks.size() == 2);
  assert(decodedBlocks[0].hash() == genesis.hash());
  assert(decodedBlocks[1].previousHash() == genesis.hash());
  assert(
      nodo::serialization::ProtocolMessageCodec::hashBlockList(blocks) ==
      nodo::serialization::ProtocolMessageCodec::hashBlockList(decodedBlocks));

  nodo::node::RoundAdvanceMessage roundAdvance(13, 2, "validator-a",
                                               1700000010);
  const auto roundAdvanceBytes =
      nodo::serialization::ProtocolMessageCodec::encodeRoundAdvanceMessage(
          roundAdvance);
  const auto decodedRoundAdvance =
      nodo::serialization::ProtocolMessageCodec::decodeRoundAdvanceMessage(
          roundAdvanceBytes);

  assert(decodedRoundAdvance.isValid());
  assert(decodedRoundAdvance.height() == 13);
  assert(decodedRoundAdvance.round() == 2);
  assert(nodo::serialization::ProtocolMessageCodec::hashRoundAdvanceMessage(
             roundAdvance) ==
         nodo::serialization::ProtocolMessageCodec::hashRoundAdvanceMessage(
             decodedRoundAdvance));

  nodo::p2p::NetworkEnvelope envelope(
      "nodo-localnet", "nodo-localnet-1", "nodo/0.1",
      nodo::p2p::NetworkMessageType::SLASHING_EVIDENCE_RESPONSE, "node-a",
      1700000000, 60, status.serialize());

  const auto envelopeBytes =
      nodo::serialization::ProtocolMessageCodec::encodeNetworkEnvelope(
          envelope);
  const auto decodedEnvelope =
      nodo::serialization::ProtocolMessageCodec::decodeNetworkEnvelope(
          envelopeBytes);

  assert(decodedEnvelope.isStructurallyValid(1024 * 1024));
  assert(decodedEnvelope.messageId() == envelope.messageId());
  assert(decodedEnvelope.payloadHash() == envelope.payloadHash());
  assert(nodo::serialization::ProtocolMessageCodec::hashNetworkEnvelope(
             envelope) ==
         nodo::serialization::ProtocolMessageCodec::hashNetworkEnvelope(
             decodedEnvelope));

  return 0;
}
