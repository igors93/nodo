#include "p2p/PeerMessage.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::p2p::PeerInfo;
using nodo::p2p::PeerMessage;
using nodo::p2p::PeerMessageFactory;
using nodo::p2p::PeerMessageType;

constexpr std::int64_t kTs = 1700000000;

PeerInfo peer(const std::string& id) {
    return PeerInfo(id, "127.0.0.1:9000", "nodo/0.1", 0, kTs);
}

void testSerializeDoesNotExposeRawPayloadWithDelimiters() {
    const PeerMessage msg = PeerMessageFactory::error(
        peer("peer-a"), "peer-b",
        "reason;with=delimiters{and}brackets",
        kTs + 1
    );

    const std::string serialized = msg.serialize();

    assert(serialized.find("reason;with=delimiters") == std::string::npos
        && "serialize() must not embed raw payload with structural delimiters");

    assert(serialized.find("payloadHex=") != std::string::npos
        && "serialize() must use payloadHex= field");
}

void testMessageIdChangesWhenPayloadChanges() {
    const std::string id1 = PeerMessage::computeMessageId(
        PeerMessageType::ERROR, "a", "b", "payload-one", kTs
    );
    const std::string id2 = PeerMessage::computeMessageId(
        PeerMessageType::ERROR, "a", "b", "payload-two", kTs
    );
    assert(id1 != id2 && "Different payloads must produce different messageIds");
}

void testMessageIdStableForSameInput() {
    const std::string id1 = PeerMessage::computeMessageId(
        PeerMessageType::ERROR, "a", "b", "payload", kTs
    );
    const std::string id2 = PeerMessage::computeMessageId(
        PeerMessageType::ERROR, "a", "b", "payload", kTs
    );
    assert(id1 == id2 && "Same inputs must produce same messageId");
}

void testPayloadWithDelimitersIsStillValid() {
    const PeerMessage msg = PeerMessageFactory::error(
        peer("peer-a"), "peer-b",
        "reason;with=delimiters",
        kTs + 5
    );
    assert(msg.isValid() && "Payload with delimiters must still produce a valid message");
}

void testPayloadAccessorReturnsRawPayload() {
    const std::string reason = "raw-reason-with-no-delimiters";
    const PeerMessage msg = PeerMessageFactory::error(
        peer("peer-a"), "peer-b",
        reason,
        kTs + 10
    );
    assert(msg.payload().find(reason) != std::string::npos
        && "payload() accessor must contain the original reason");
}

} // namespace

int main() {
    testSerializeDoesNotExposeRawPayloadWithDelimiters();
    testMessageIdChangesWhenPayloadChanges();
    testMessageIdStableForSameInput();
    testPayloadWithDelimitersIsStillValid();
    testPayloadAccessorReturnsRawPayload();
    return 0;
}
