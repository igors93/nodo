#ifndef NODO_SERIALIZATION_V1_ENCODING_PRIMITIVES_HPP
#define NODO_SERIALIZATION_V1_ENCODING_PRIMITIVES_HPP

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nodo::serialization {

// Byte-level reference primitives for the future v1 format. They do not
// validate an object's typed payload; Phase 2 codecs must do that before any
// bytes are hashed, signed, stored or accepted from a peer.
class V1EncodingPrimitives {
public:
  using Digest = std::array<unsigned char, 32>;

  enum class Kind : std::uint16_t {
    GENESIS = 1,
    TRANSACTION = 2,
    HEADER = 3,
    BLOCK_BODY = 4,
    RECEIPT = 5,
    VOTE = 6,
    PROPOSAL = 7,
    QUORUM_CERTIFICATE = 8,
    EVIDENCE = 9,
    VALIDATOR_SET = 10,
    PARAMETER_SET = 11,
    FINALIZED_ARTIFACT = 12,
    STATE_SNAPSHOT = 13,
    NETWORK_ENVELOPE = 14
  };

  static constexpr std::uint16_t kVersion = 1;
  static constexpr std::uint32_t kMaxListElements = 1'048'576;

  static std::uint32_t maxObjectBytes(Kind kind);
  static std::vector<unsigned char> topLevel(
      Kind kind, std::span<const unsigned char> typedPayload);
  static Digest hash(std::string_view domain,
                     std::span<const unsigned char> bytes);
  static Digest merkleRoot(
      std::string_view kind,
      const std::vector<std::vector<unsigned char>> &elements);
  static std::string hex(const Digest &digest);

private:
  static Digest merkleTree(
      std::string_view kind,
      const std::vector<Digest> &leaves,
      std::size_t begin,
      std::size_t end);
};

} // namespace nodo::serialization

#endif
