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

} // namespace

int main() {
    nodo::node::ChainStatusMessage status(
        "nodo-localnet",
        "nodo-localnet-1",
        "nodo/0.1",
        12,
        HASH_A,
        10,
        HASH_B
    );

    const auto statusBytes =
        nodo::serialization::ProtocolMessageCodec::encodeChainStatusMessage(
            status
        );
    const auto decodedStatus =
        nodo::serialization::ProtocolMessageCodec::decodeChainStatusMessage(
            statusBytes
        );

    assert(decodedStatus.isValid());
    assert(decodedStatus.latestHeight() == status.latestHeight());
    assert(decodedStatus.finalizedHeight() == status.finalizedHeight());
    assert(
        nodo::serialization::ProtocolMessageCodec::hashChainStatusMessage(status) ==
        nodo::serialization::ProtocolMessageCodec::hashChainStatusMessage(decodedStatus)
    );

    nodo::node::BlockLocator locator(
        5,
        100,
        {HASH_A, HASH_B}
    );

    const auto locatorBytes =
        nodo::serialization::ProtocolMessageCodec::encodeBlockLocator(locator);
    const auto decodedLocator =
        nodo::serialization::ProtocolMessageCodec::decodeBlockLocator(locatorBytes);

    assert(decodedLocator.isValid());
    assert(decodedLocator.fromHeight() == locator.fromHeight());
    assert(decodedLocator.knownAncestorHashes().size() == 2);

    nodo::node::NetworkBlockSyncRequest request(
        "node-a",
        locator,
        1700000000
    );

    const auto requestBytes =
        nodo::serialization::ProtocolMessageCodec::encodeNetworkBlockSyncRequest(
            request
        );
    const auto decodedRequest =
        nodo::serialization::ProtocolMessageCodec::decodeNetworkBlockSyncRequest(
            requestBytes
        );

    assert(decodedRequest.isValid());
    assert(decodedRequest.requesterNodeId() == "node-a");
    assert(decodedRequest.locator().maxBlocks() == 100);

    nodo::p2p::NetworkEnvelope envelope(
        "nodo-localnet",
        "nodo-localnet-1",
        "nodo/0.1",
        nodo::p2p::NetworkMessageType::CHAIN_STATUS,
        "node-a",
        1700000000,
        60,
        status.serialize()
    );

    const auto envelopeBytes =
        nodo::serialization::ProtocolMessageCodec::encodeNetworkEnvelope(
            envelope
        );
    const auto decodedEnvelope =
        nodo::serialization::ProtocolMessageCodec::decodeNetworkEnvelope(
            envelopeBytes
        );

    assert(decodedEnvelope.isStructurallyValid(1024 * 1024));
    assert(decodedEnvelope.messageId() == envelope.messageId());
    assert(decodedEnvelope.payloadHash() == envelope.payloadHash());
    assert(
        nodo::serialization::ProtocolMessageCodec::hashNetworkEnvelope(envelope) ==
        nodo::serialization::ProtocolMessageCodec::hashNetworkEnvelope(decodedEnvelope)
    );

    return 0;
}
