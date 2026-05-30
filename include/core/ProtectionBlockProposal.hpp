#ifndef NODO_CORE_PROTECTION_BLOCK_PROPOSAL_HPP
#define NODO_CORE_PROTECTION_BLOCK_PROPOSAL_HPP

#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "economics/EpochRewardLedgerBuilder.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

/*
 * ProtectionBlockProposal represents a candidate block containing protection
 * economics records.
 *
 * It is deliberately separate from Blockchain::addBlock so the proposal can be
 * audited before mutation.
 */
class ProtectionBlockProposal {
public:
    ProtectionBlockProposal(
        Block block,
        economics::EpochRewardLedgerBuildResult ledgerBuildResult,
        std::uint64_t chainSizeBeforeProposal,
        std::string expectedPreviousHash
    );

    const Block& block() const;
    const economics::EpochRewardLedgerBuildResult& ledgerBuildResult() const;

    std::uint64_t chainSizeBeforeProposal() const;
    const std::string& expectedPreviousHash() const;

    bool isValidForBlockchain(
        const Blockchain& blockchain
    ) const;

    void appendToBlockchain(
        Blockchain& blockchain
    ) const;

    std::string serialize() const;

private:
    Block m_block;
    economics::EpochRewardLedgerBuildResult m_ledgerBuildResult;
    std::uint64_t m_chainSizeBeforeProposal;
    std::string m_expectedPreviousHash;
};

/*
 * ProtectionBlockBuilder builds a deterministic block proposal from reward
 * distribution output.
 */
class ProtectionBlockBuilder {
public:
    static ProtectionBlockProposal buildRewardBlockProposal(
        const Blockchain& blockchain,
        const economics::EpochRewardDistribution& distribution,
        std::int64_t timestamp
    );
};

} // namespace nodo::core

#endif
