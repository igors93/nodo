#include "serialization/BlockCodec.hpp"

#include "serialization/LedgerRecordCodec.hpp"
#include "storage/BlockSnapshotHeader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::serialization {

core::Block BlockCodec::deserialize(
    const std::string& serializedBlock
) {
    if (serializedBlock.rfind("Block{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a Block.");
    }

    const storage::BlockSnapshotHeader snapshotHeader =
        storage::BlockSnapshotHeader::fromSerializedBlock(serializedBlock);

    std::vector<core::LedgerRecord> records =
        LedgerRecordCodec::deserializeListFromBlockHeaderPayload(
            snapshotHeader.headerPayload()
        );

    if (records.size() != snapshotHeader.recordCount()) {
        throw std::logic_error("BlockCodec record count mismatch.");
    }

    /*
     * Block constructor recalculates the block hash from the reconstructed
     * header payload. This protects against trusting the serialized hash field.
     */
    core::Block block(
        snapshotHeader.blockIndex(),
        snapshotHeader.previousHash(),
        std::move(records),
        snapshotHeader.timestamp()
    );

    if (!block.isValid()) {
        throw std::invalid_argument("Deserialized Block is invalid.");
    }

    if (block.hash() != snapshotHeader.blockHash()) {
        throw std::logic_error("Deserialized Block hash mismatch.");
    }

    if (block.serialize() != serializedBlock) {
        throw std::logic_error("Block round-trip serialization mismatch.");
    }

    return block;
}

core::Block BlockCodec::deserializeFromFile(
    const std::string& filePath
) {
    return deserialize(readFile(filePath));
}

std::vector<core::Block> BlockCodec::deserializeFiles(
    const std::vector<std::string>& filePaths
) {
    std::vector<core::Block> blocks;

    for (const auto& filePath : filePaths) {
        blocks.push_back(
            deserializeFromFile(filePath)
        );
    }

    return blocks;
}

std::string BlockCodec::readFile(
    const std::string& filePath
) {
    if (filePath.empty()) {
        throw std::invalid_argument("Block snapshot file path cannot be empty.");
    }

    std::ifstream input(filePath, std::ios::in | std::ios::binary);

    if (!input.is_open()) {
        throw std::runtime_error("Failed to open block snapshot file for BlockCodec.");
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (!input.good() && !input.eof()) {
        throw std::runtime_error("Failed while reading block snapshot file for BlockCodec.");
    }

    return buffer.str();
}

} // namespace nodo::serialization