#include "p2p/LocalNodeSync.hpp"
#include "consensus/ForkChoice.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/ValidationWorkRecord.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::consensus::BlockFinalizationRegistry;
using nodo::consensus::ChainForkSummary;
using nodo::consensus::FinalizedCheckpoint;
using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::LedgerRecord;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;
using nodo::p2p::LocalNodeSynchronizer;
using nodo::p2p::LocalSyncDecision;
using nodo::p2p::LocalSyncRejectReason;
using nodo::p2p::PeerInfo;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ValidationWorkRecord work(const std::string& evidence) {
    return ValidationWorkRecord(
        "sync-validator",
        1,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "sync-target-" + evidence,
        evidence,
        1,
        kTimestamp
    );
}

Blockchain genesisChain() {
    const Block genesis =
        Block::createGenesisBlock(
            {
                LedgerRecord::fromValidationWorkRecord(
                    work("genesis"),
                    kTimestamp + 1
                )
            },
            kTimestamp + 2
        );

    Blockchain chain;
    chain.addGenesisBlock(genesis);
    return chain;
}

PeerInfo peer(const std::string& id, std::uint64_t height) {
    return PeerInfo(
        id,
        "127.0.0.1:9001",
        "nodo/0.1",
        height,
        kTimestamp
    );
}

void testRequestsBlocksFromLongerPeer() {
    const Blockchain localChain =
        genesisChain();

    const BlockFinalizationRegistry localFinality;

    const ChainForkSummary remoteSummary(
        3,
        2,
        "remote-latest-hash"
    );

    const auto plan =
        LocalNodeSynchronizer::evaluatePeerSummary(
            peer("local", 0),
            peer("remote", 2),
            localChain,
            localFinality,
            remoteSummary,
            kTimestamp + 10
        );

    requireCondition(
        plan.decision() == LocalSyncDecision::REQUEST_BLOCKS &&
        plan.shouldRequestBlocks(),
        "Longer remote peer should trigger block request."
    );

    requireCondition(
        plan.fromHeight() == 1U &&
        plan.toHeight() == 2U,
        "Sync request should start after genesis when no finality exists."
    );
}

void testNoActionForSameHeightPeer() {
    const Blockchain localChain =
        genesisChain();

    const BlockFinalizationRegistry localFinality;

    const ChainForkSummary remoteSummary(
        1,
        0,
        localChain.latestBlock().hash()
    );

    const auto plan =
        LocalNodeSynchronizer::evaluatePeerSummary(
            peer("local", 0),
            peer("remote", 0),
            localChain,
            localFinality,
            remoteSummary,
            kTimestamp + 20
        );

    requireCondition(
        plan.decision() == LocalSyncDecision::NO_ACTION,
        "Peer with same summary should not trigger sync."
    );
}

void testRejectsInvalidRemoteSummary() {
    const Blockchain localChain =
        genesisChain();

    const BlockFinalizationRegistry localFinality;

    const ChainForkSummary invalidRemoteSummary;

    const auto plan =
        LocalNodeSynchronizer::evaluatePeerSummary(
            peer("local", 0),
            peer("remote", 0),
            localChain,
            localFinality,
            invalidRemoteSummary,
            kTimestamp + 30
        );

    requireCondition(
        plan.rejectReason() == LocalSyncRejectReason::INVALID_REMOTE_SUMMARY,
        "Invalid remote summary should be rejected."
    );
}

void testRejectsRemoteCheckpointAheadOfRemoteTip() {
    const Blockchain localChain =
        genesisChain();

    const BlockFinalizationRegistry localFinality;

    const ChainForkSummary regressingRemoteSummary(
        1,
        0,
        localChain.latestBlock().hash(),
        FinalizedCheckpoint(
            1,
            "remote-finalized-hash",
            localChain.latestBlock().hash(),
            1,
            kTimestamp + 35
        )
    );

    const auto plan =
        LocalNodeSynchronizer::evaluatePeerSummary(
            peer("local", 0),
            peer("remote", 0),
            localChain,
            localFinality,
            regressingRemoteSummary,
            kTimestamp + 36
        );

    requireCondition(
        plan.rejectReason() == LocalSyncRejectReason::INVALID_REMOTE_SUMMARY,
        "Remote sync checkpoint ahead of remote tip should be rejected."
    );
}

void testRejectsInvalidPeerMetadata() {
    const Blockchain localChain =
        genesisChain();

    const BlockFinalizationRegistry localFinality;

    const ChainForkSummary remoteSummary(
        2,
        1,
        "remote-latest-hash"
    );

    const auto plan =
        LocalNodeSynchronizer::evaluatePeerSummary(
            PeerInfo(
                "local peer bad",
                "127.0.0.1:9001",
                "nodo/0.1",
                0,
                kTimestamp
            ),
            peer("remote", 1),
            localChain,
            localFinality,
            remoteSummary,
            kTimestamp + 40
        );

    requireCondition(
        plan.rejectReason() == LocalSyncRejectReason::INVALID_PEER,
        "Invalid peer metadata should be rejected."
    );
}

} // namespace

int main() {
    try {
        testRequestsBlocksFromLongerPeer();
        testNoActionForSameHeightPeer();
        testRejectsInvalidRemoteSummary();
        testRejectsRemoteCheckpointAheadOfRemoteTip();
        testRejectsInvalidPeerMetadata();

        std::cout << "Nodo local node sync tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo local node sync tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
