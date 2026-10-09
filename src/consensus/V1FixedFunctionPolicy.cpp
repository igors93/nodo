#include "consensus/V1FixedFunctionPolicy.hpp"

#include <cstddef>

namespace nodo::consensus {

namespace {

std::uint32_t readU32(std::span<const unsigned char> bytes,
                      std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
         static_cast<std::uint32_t>(bytes[offset + 3]);
}

} // namespace

V1FixedFunctionPolicy::Violation V1FixedFunctionPolicy::validateGovernanceAction(
    std::uint8_t kind, std::span<const unsigned char> action) {
  switch (kind) {
  case 1: // PARAMETER_CHANGE: tag:u8, value:u64, activation_epoch:u64
    if (action.size() != 17 || action[0] < 1 || action[0] > 7) {
      return Violation::INVALID_ACTION;
    }
    return Violation::NONE;
  case 2: // TREASURY_SPEND: recipient:account, amount:u64
    if (action.size() != 40) {
      return Violation::INVALID_ACTION;
    }
    for (std::size_t offset = 32; offset < 40; ++offset) {
      if (action[offset] != 0) {
        return Violation::NONE;
      }
    }
    return Violation::INVALID_ACTION;
  case 3: // TEXT: no executable action
    return action.empty() ? Violation::NONE : Violation::INVALID_ACTION;
  case 4: // PROTOCOL_UPGRADE: u16, u64, three hashes
    return action.size() == 106 ? Violation::NONE : Violation::INVALID_ACTION;
  case 5: // UPGRADE_CANCEL: u64, rules hash
    return action.size() == 40 ? Violation::NONE : Violation::INVALID_ACTION;
  default:
    return Violation::INVALID_ACTION;
  }
}

V1FixedFunctionPolicy::Violation V1FixedFunctionPolicy::validateTransaction(
    std::uint8_t type, std::uint64_t amount,
    std::span<const unsigned char> payload) {
  if (payload.size() > kMaxTransactionBytes) {
    return Violation::OVERSIZE;
  }
  if (type < 1 || type > 13) {
    return Violation::UNKNOWN_TYPE;
  }
  if ((type <= 6 || type == 11) ? amount == 0 : amount != 0) {
    return Violation::INVALID_AMOUNT;
  }

  std::size_t expectedSize = 0;
  switch (type) {
  case 1: // recipient account
  case 3: // validator
  case 4: // position
  case 5: // position
  case 6: // position
  case 8: // validator
  case 9: // validator
  case 13: // proposal
    expectedSize = 32;
    break;
  case 2: // BURN
    expectedSize = 0;
    break;
  case 7: // consensus key and stake position
    expectedSize = 64;
    break;
  case 10: // validator, new key, activation epoch
    expectedSize = 72;
    break;
  case 11: { // kind:u8, title:hash, action:bytes, expiry_epoch:u64
    constexpr std::size_t minimum = 1 + 32 + 4 + 8;
    if (payload.size() < minimum) {
      return Violation::INVALID_SHAPE;
    }
    const std::size_t actionLength = readU32(payload, 33);
    if (actionLength != payload.size() - minimum) {
      return Violation::INVALID_SHAPE;
    }
    return validateGovernanceAction(
        payload[0], payload.subspan(37, actionLength));
  }
  case 12: // proposal:hash, choice:u8, validator:validator
    if (payload.size() != 65) {
      return Violation::INVALID_SHAPE;
    }
    return payload[32] >= 1 && payload[32] <= 3 ? Violation::NONE
                                                 : Violation::INVALID_CHOICE;
  default:
    return Violation::UNKNOWN_TYPE;
  }
  return payload.size() == expectedSize ? Violation::NONE
                                        : Violation::INVALID_SHAPE;
}

} // namespace nodo::consensus
