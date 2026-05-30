#include "p2p/PeerMessage.hpp"
#include "consensus/ForkChoice.hpp"
#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/ValidationWorkRecord.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::consensus::ChainForkSummary;
using nodo::core::Block;
using nodo::core::LedgerRecord;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;
using nodo::p2p::PeerInfo;
using nodo::p2p::PeerMessage;
using nodo::p2p::PeerMessageFactory;
using nodo::p2p::PeerMessageType;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

PeerInfo peer(
    const std::string& id,
    std::uint64_t height = 0
) {
    return PeerInfo(
        id,
        "127.0.0.1:9000",
        "nodo/0.1",
        height,
        kTimestamp
    );
}

Block genesisBlock() {
    return Block::createGenesisBlock(
        {
            LedgerRecord::fromValidationWorkRecord(
                ValidationWorkRecord(
                    "p2p-validator",
                    1,
                    ValidationWorkType::VALIDATE_BLOCK,
                    ValidationWorkResult::ACCEPTED,
                    "p2p-target",
                    "p2p-evidence",
                    1,
                    kTimestamp
                ),
                kTimestamp + 1
            )
        },
        kTimestamp + 2
    );
}

void testHandshakeMessageIsValidAndDeterministic() {
    const PeerInfo local =
        peer("peer-local");

    const PeerMessage message =
        PeerMessageFactory::handshake(
            local,
            "peer-remote",
            kTimestamp + 10
        );

    requireCondition(
        message.isValid(),
        "Handshake message should be valid."
    );

    requireCondition(
        message.messageId() ==
            PeerMessage::computeMessageId(
                message.type(),
                message.fromPeerId(),
                message.toPeerId(),
                message.payload(),
                message.createdAt()
            ),
        "Message id should be deterministic."
    );
}

void testChainSummaryMessage() {
    const ChainForkSummary summary(
        2,
        1,
        "latest-block-hash"
    );

    const PeerMessage message =
        PeerMessageFactory::chainSummary(
            peer("peer-a", 1),
            "peer-b",
            summary,
            kTimestamp + 20
        );

    requireCondition(
        message.type() == PeerMessageType::CHAIN_SUMMARY &&
        message.isValid(),
        "Chain summary message should be valid."
    );
}

void testBlockAnnouncementAllowsSerializedBlockPayload() {
    const Block block =
        genesisBlock();

    const PeerMessage message =
        PeerMessageFactory::blockAnnouncement(
            peer("peer-block"),
            "peer-target",
            block,
            kTimestamp + 30
        );

    requireCondition(
        message.type() == PeerMessageType::BLOCK_ANNOUNCEMENT &&
        message.isValid(),
        "Block announcement with serialized block payload should be valid."
    );

    requireCondition(
        message.payload().find(block.hash()) != std::string::npos,
        "Block announcement payload should contain block hash."
    );
}

void testExpiredMessage() {
    const PeerMessage message =
        PeerMessageFactory::handshake(
            peer("peer-expire"),
            "peer-target",
            kTimestamp + 40
        );

    requireCondition(
        !message.expired(kTimestamp + 41),
        "Message should not expire immediately."
    );

    requireCondition(
        message.expired(kTimestamp + 1000),
        "Message should expire after TTL."
    );
}

void testInvalidPeerRejected() {
    bool rejected = false;

    try {
        (void)PeerMessageFactory::handshake(
            PeerInfo(
                "bad peer id",
                "127.0.0.1:9000",
                "nodo/0.1",
                0,
                kTimestamp
            ),
            "peer-target",
            kTimestamp + 50
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Invalid peer id should be rejected."
    );
}

} // namespace

int main() {
    try {
        testHandshakeMessageIsValidAndDeterministic();
        testChainSummaryMessage();
        testBlockAnnouncementAllowsSerializedBlockPayload();
        testExpiredMessage();
        testInvalidPeerRejected();

        std::cout << "Nodo peer message tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo peer message tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
