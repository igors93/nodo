#include "core/ProtectionBlockProposal.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

ProtectionBlockProposal::ProtectionBlockProposal(
    Block block,
    economics::EpochRewardLedgerBuildResult ledgerBuildResult,
    std::uint64_t chainSizeBeforeProposal,
    std::string expectedPreviousHash
)
    : m_block(std::move(block)),
      m_ledgerBuildResult(std::move(ledgerBuildResult)),
      m_chainSizeBeforeProposal(chainSizeBeforeProposal),
      m_expectedPreviousHash(std::move(expectedPreviousHash)) {}

const Block& ProtectionBlockProposal::block() const {
    return m_block;
}

const economics::EpochRewardLedgerBuildResult& ProtectionBlockProposal::ledgerBuildResult() const {
    return m_ledgerBuildResult;
}

std::uint64_t ProtectionBlockProposal::chainSizeBeforeProposal() const {
    return m_chainSizeBeforeProposal;
}

const std::string& ProtectionBlockProposal::expectedPreviousHash() const {
    return m_expectedPreviousHash;
}

bool ProtectionBlockProposal::isValidForBlockchain(
    const Blockchain& blockchain
) const {
    if (blockchain.empty()) {
        return false;
    }

    if (!blockchain.isValid()) {
        return false;
    }

    if (!m_block.isValid()) {
        return false;
    }

    if (!m_ledgerBuildResult.isValid()) {
        return false;
    }

    if (m_expectedPreviousHash.empty()) {
        return false;
    }

    if (m_chainSizeBeforeProposal != blockchain.size()) {
        return false;
    }

    if (m_block.index() != blockchain.size()) {
        return false;
    }

    if (m_expectedPreviousHash != blockchain.latestBlock().hash()) {
        return false;
    }

    if (m_block.previousHash() != blockchain.latestBlock().hash()) {
        return false;
    }

    if (m_block.records().size() != m_ledgerBuildResult.records().size()) {
        return false;
    }

    for (std::size_t index = 0; index < m_block.records().size(); ++index) {
        if (m_block.records()[index].serialize() !=
            m_ledgerBuildResult.records()[index].serialize()) {
            return false;
        }
    }

    return blockchain.canAppendBlock(m_block);
}

void ProtectionBlockProposal::appendToBlockchain(
    Blockchain& blockchain
) const {
    if (!isValidForBlockchain(blockchain)) {
        throw std::invalid_argument("Invalid protection block proposal rejected.");
    }

    blockchain.addBlock(m_block);
}

std::string ProtectionBlockProposal::serialize() const {
    std::ostringstream oss;

    oss << "ProtectionBlockProposal{"
        << "chainSizeBeforeProposal=" << m_chainSizeBeforeProposal
        << ";expectedPreviousHash=" << m_expectedPreviousHash
        << ";blockIndex=" << m_block.index()
        << ";blockHash=" << m_block.hash()
        << ";ledgerBuildResult=" << m_ledgerBuildResult.serialize()
        << "}";

    return oss.str();
}

ProtectionBlockProposal ProtectionBlockBuilder::buildRewardBlockProposal(
    const Blockchain& blockchain,
    const economics::EpochRewardDistribution& distribution,
    std::int64_t timestamp
) {
    if (blockchain.empty()) {
        throw std::invalid_argument("Cannot build reward block proposal for empty blockchain.");
    }

    if (!blockchain.isValid()) {
        throw std::invalid_argument("Cannot build reward block proposal for invalid blockchain.");
    }

    if (!distribution.isValid()) {
        throw std::invalid_argument("Invalid reward distribution rejected by block builder.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("Reward block proposal timestamp must be positive.");
    }

    const economics::EpochRewardLedgerBuildResult ledgerResult =
        economics::EpochRewardLedgerBuilder::buildLedgerRecords(
            distribution,
            timestamp
        );

    const Block block(
        blockchain.size(),
        blockchain.latestBlock().hash(),
        ledgerResult.records(),
        timestamp
    );

    ProtectionBlockProposal proposal(
        block,
        ledgerResult,
        blockchain.size(),
        blockchain.latestBlock().hash()
    );

    if (!proposal.isValidForBlockchain(blockchain)) {
        throw std::logic_error("ProtectionBlockBuilder produced an invalid proposal.");
    }

    return proposal;
}

} // namespace nodo::core
