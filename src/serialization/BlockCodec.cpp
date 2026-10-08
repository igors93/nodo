#include "serialization/BlockCodec.hpp"

#include "storage/BlockSnapshotHeader.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::serialization {

core::Block BlockCodec::deserialize(const std::string &serializedBlock) {
  const auto header =
      storage::BlockSnapshotHeader::fromSerializedBlock(serializedBlock);
  const auto block = core::Block::deserialize(serializedBlock);
  if (!block || !block->isValid(false) || block->hash() != header.blockHash()) {
    throw std::invalid_argument("Invalid canonical serialized Block.");
  }
  return *block;
}

core::Block BlockCodec::deserializeFromFile(const std::string &filePath) {
  return deserialize(readFile(filePath));
}

std::vector<core::Block>
BlockCodec::deserializeFiles(const std::vector<std::string> &filePaths) {
  std::vector<core::Block> blocks;

  for (const auto &filePath : filePaths) {
    blocks.push_back(deserializeFromFile(filePath));
  }

  return blocks;
}

std::string BlockCodec::readFile(const std::string &filePath) {
  if (filePath.empty()) {
    throw std::invalid_argument("Block snapshot file path cannot be empty.");
  }

  std::ifstream input(filePath, std::ios::in | std::ios::binary);

  if (!input.is_open()) {
    throw std::runtime_error(
        "Failed to open block snapshot file for BlockCodec.");
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();

  if (!input.good() && !input.eof()) {
    throw std::runtime_error(
        "Failed while reading block snapshot file for BlockCodec.");
  }

  return buffer.str();
}

} // namespace nodo::serialization
