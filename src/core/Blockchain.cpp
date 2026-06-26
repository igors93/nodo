#include "core/Blockchain.hpp"

#include <sstream>
#include <stdexcept>

namespace nodo::core {

Blockchain::Blockchain() = default;

void Blockchain::addGenesisBlock(const Block& block) {
    if (!m_blocks.empty()) {
        throw std::logic_error("Genesis block can only be added to an empty blockchain.");
    }

    if (!isValidGenesisBlock(block)) {
        throw std::invalid_argument("Invalid genesis block rejected by Blockchain.");
    }

    m_blocks.push_back(block);
}

void Blockchain::addBlock(const Block& block) {
    if (m_blocks.empty()) {
        throw std::logic_error("Cannot add a regular block before genesis block.");
    }

    if (!canAppendBlock(block)) {
        throw std::invalid_argument("Invalid block rejected by Blockchain.");
    }

    m_blocks.push_back(block);
}

bool Blockchain::empty() const {
    return m_blocks.empty();
}

std::size_t Blockchain::size() const {
    return m_blocks.size();
}

const Block& Blockchain::genesisBlock() const {
    if (m_blocks.empty()) {
        throw std::logic_error("Blockchain has no genesis block.");
    }

    return m_blocks.front();
}

const Block& Blockchain::latestBlock() const {
    if (m_blocks.empty()) {
        throw std::logic_error("Blockchain has no blocks.");
    }

    return m_blocks.back();
}

const std::vector<Block>& Blockchain::blocks() const {
    return m_blocks;
}

bool Blockchain::isValid() const {
    return isValid(true);
}

bool Blockchain::isValid(bool requireProtocolCommitments) const {
    if (m_blocks.empty()) {
        return false;
    }

    if (!isValidGenesisBlock(m_blocks.front())) {
        return false;
    }

    for (std::size_t i = 1; i < m_blocks.size(); ++i) {
        const Block& previousBlock = m_blocks[i - 1];
        const Block& currentBlock = m_blocks[i];

        if (!isValidNextBlock(previousBlock, currentBlock, requireProtocolCommitments)) {
            return false;
        }
    }

    return true;
}

bool Blockchain::canAppendBlock(const Block& block) const {
    if (m_blocks.empty()) {
        return isValidGenesisBlock(block);
    }

    // Protocol commitment enforcement is the validator's responsibility.
    // canAppendBlock checks structural linkage only for the candidate.
    return isValidNextBlock(latestBlock(), block, false);
}

std::string Blockchain::serialize() const {
    std::ostringstream oss;

    oss << "Blockchain{"
        << "blockCount=" << m_blocks.size()
        << ";blocks=[";

    for (std::size_t i = 0; i < m_blocks.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }

        oss << m_blocks[i].serialize();
    }

    oss << "]}";

    return oss.str();
}

bool Blockchain::isValidGenesisBlock(const Block& block) const {
    if (!block.isValid()) {
        return false;
    }

    if (!block.isGenesisBlock()) {
        return false;
    }

    if (block.index() != 0) {
        return false;
    }

    if (block.previousHash() != "GENESIS") {
        return false;
    }

    return true;
}

bool Blockchain::isValidNextBlock(
    const Block& previousBlock,
    const Block& currentBlock,
    bool requireProtocolCommitmentsForCurrentBlock
) const {
    if (!previousBlock.isValid(requireProtocolCommitmentsForCurrentBlock)) {
        return false;
    }

    if (!currentBlock.isValid(requireProtocolCommitmentsForCurrentBlock)) {
        return false;
    }

    if (currentBlock.isGenesisBlock()) {
        return false;
    }

    if (currentBlock.index() != previousBlock.index() + 1) {
        return false;
    }

    if (currentBlock.previousHash() != previousBlock.hash()) {
        return false;
    }

    if (currentBlock.timestamp() <= previousBlock.timestamp()) {
        return false;
    }

    return true;
}

} // namespace nodo::core