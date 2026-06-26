#ifndef NODO_CORE_BLOCKCHAIN_HPP
#define NODO_CORE_BLOCKCHAIN_HPP

#include "core/Block.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {

/*
 * Blockchain owns the ordered list of blocks.
 *
 * Security principle:
 * Blocks are not trusted individually. The entire chain must be validated
 * from genesis to the latest block.
 */
class Blockchain {
public:
    Blockchain();

    void addGenesisBlock(const Block& block);
    void addBlock(const Block& block);

    bool empty() const;
    std::size_t size() const;

    const Block& genesisBlock() const;
    const Block& latestBlock() const;
    const std::vector<Block>& blocks() const;

    bool isValid() const;

    /*
     * Returns true when the given block can be appended to the current chain.
     */
    bool canAppendBlock(const Block& block) const;

    /*
     * Deterministic serialization of the full chain.
     *
     * This will later help storage and chain audit.
     */
    std::string serialize() const;

private:
    std::vector<Block> m_blocks;

    bool isValidGenesisBlock(const Block& block) const;

    // requireProtocolCommitmentsForCurrentBlock: when false, validates the
    // current block structurally only (no canonical-root check). Used by
    // canAppendBlock so that StructuralOnly candidates are not rejected.
    bool isValidNextBlock(
        const Block& previousBlock,
        const Block& currentBlock,
        bool requireProtocolCommitmentsForCurrentBlock = true
    ) const;
};

} // namespace nodo::core

#endif