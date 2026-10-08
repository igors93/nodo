#include "serialization/V1EncodingPrimitives.hpp"

#include "serialization/CanonicalWriter.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace nodo::serialization {

namespace {

constexpr std::size_t kPrefixBytes = 8;
constexpr std::size_t kMaxFrameBytes = 5 * 1024 * 1024;

std::vector<unsigned char> concat(std::span<const unsigned char> first,
                                  std::span<const unsigned char> second) {
  std::vector<unsigned char> result;
  result.reserve(first.size() + second.size());
  result.insert(result.end(), first.begin(), first.end());
  result.insert(result.end(), second.begin(), second.end());
  return result;
}

std::string merkleDomain(std::string_view label, std::string_view kind) {
  if (kind != "tx" && kind != "receipt" && kind != "evidence" &&
      kind != "state") {
    throw std::invalid_argument("Unknown v1 Merkle kind.");
  }
  return std::string(label) + std::string(kind);
}

} // namespace

std::uint32_t V1EncodingPrimitives::maxObjectBytes(Kind kind) {
  switch (kind) {
  case Kind::TRANSACTION:
    return 262144;
  case Kind::HEADER:
    return 4096;
  case Kind::VOTE:
    return 1024;
  case Kind::RECEIPT:
    return 65536;
  case Kind::EVIDENCE:
    return 262144;
  case Kind::BLOCK_BODY:
  case Kind::QUORUM_CERTIFICATE:
    return 1'048'576;
  case Kind::GENESIS:
  case Kind::VALIDATOR_SET:
  case Kind::PARAMETER_SET:
    return 1'048'576;
  case Kind::STATE_SNAPSHOT:
  case Kind::NETWORK_ENVELOPE:
  case Kind::PROPOSAL:
  case Kind::FINALIZED_ARTIFACT:
    return 4'194'304;
  }
  throw std::invalid_argument("Unknown v1 object kind.");
}

std::vector<unsigned char> V1EncodingPrimitives::topLevel(
    Kind kind, std::span<const unsigned char> typedPayload) {
  const auto maxBytes = maxObjectBytes(kind);
  if (typedPayload.size() > maxBytes - kPrefixBytes) {
    throw std::length_error("V1 object exceeds its byte limit.");
  }
  CanonicalWriter writer;
  for (const unsigned char byte : {'N', 'O', 'D', 'O'}) {
    writer.writeUInt8(byte);
  }
  writer.writeUInt16(kVersion);
  writer.writeUInt16(static_cast<std::uint16_t>(kind));
  std::vector<unsigned char> result = writer.bytes();
  result.insert(result.end(), typedPayload.begin(), typedPayload.end());
  return result;
}

V1EncodingPrimitives::Digest V1EncodingPrimitives::hash(
    std::string_view domain, std::span<const unsigned char> bytes) {
  if (domain.empty() || domain.size() > 64 ||
      !std::all_of(domain.begin(), domain.end(), [](unsigned char ch) {
        return ch >= 0x21 && ch <= 0x7e;
      }) ||
      bytes.size() > kMaxFrameBytes) {
    throw std::invalid_argument("Invalid v1 hash domain or input length.");
  }
  constexpr std::string_view prefix = "NODO/V1/";
  std::vector<unsigned char> input;
  input.reserve(prefix.size() + domain.size() + 1 + bytes.size());
  input.insert(input.end(), prefix.begin(), prefix.end());
  input.insert(input.end(), domain.begin(), domain.end());
  input.push_back(0);
  input.insert(input.end(), bytes.begin(), bytes.end());

  Digest digest{};
  unsigned int length = 0;
  if (EVP_Digest(input.data(), input.size(), digest.data(), &length,
                 EVP_sha256(), nullptr) != 1 || length != digest.size()) {
    throw std::runtime_error("V1 SHA-256 calculation failed.");
  }
  return digest;
}

V1EncodingPrimitives::Digest V1EncodingPrimitives::merkleRoot(
    std::string_view kind,
    const std::vector<std::vector<unsigned char>> &elements) {
  const std::string leafDomain = merkleDomain("MERKLE-LEAF/", kind);
  if (elements.size() > kMaxListElements) {
    throw std::length_error("V1 Merkle tree exceeds its leaf count limit.");
  }
  if (elements.empty()) {
    return hash(merkleDomain("MERKLE-EMPTY/", kind), {});
  }
  std::vector<Digest> leaves;
  leaves.reserve(elements.size());
  for (std::size_t index = 0; index < elements.size(); ++index) {
    if (elements[index].size() > kMaxFrameBytes - 4) {
      throw std::length_error("V1 Merkle element exceeds its byte limit.");
    }
    CanonicalWriter writer;
    writer.writeUInt32(static_cast<std::uint32_t>(index));
    std::vector<unsigned char> leafBytes = writer.bytes();
    leafBytes.insert(leafBytes.end(), elements[index].begin(),
                     elements[index].end());
    leaves.push_back(hash(leafDomain, leafBytes));
  }
  const Digest tree = merkleTree(kind, leaves, 0, leaves.size());
  CanonicalWriter writer;
  writer.writeUInt32(static_cast<std::uint32_t>(leaves.size()));
  const std::vector<unsigned char> counted = concat(writer.bytes(), tree);
  return hash(merkleDomain("MERKLE-ROOT/", kind), counted);
}

V1EncodingPrimitives::Digest V1EncodingPrimitives::merkleTree(
    std::string_view kind, const std::vector<Digest> &leaves,
    std::size_t begin, std::size_t end) {
  const std::size_t count = end - begin;
  if (count == 1) {
    return leaves[begin];
  }
  // RFC 6962 split: largest power of two strictly less than count.
  std::size_t split = 1;
  while (split <= (count - 1) / 2) {
    split *= 2;
  }
  const Digest left = merkleTree(kind, leaves, begin, begin + split);
  const Digest right = merkleTree(kind, leaves, begin + split, end);
  return hash(merkleDomain("MERKLE-NODE/", kind), concat(left, right));
}

std::string V1EncodingPrimitives::hex(const Digest &digest) {
  std::ostringstream output;
  for (const unsigned char byte : digest) {
    output << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<unsigned int>(byte);
  }
  return output.str();
}

} // namespace nodo::serialization
