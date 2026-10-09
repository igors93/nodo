#ifndef NODO_CONSENSUS_V1_FIXED_FUNCTION_POLICY_HPP
#define NODO_CONSENSUS_V1_FIXED_FUNCTION_POLICY_HPP

#include <cstdint>
#include <span>

namespace nodo::consensus {

// Closed v1 shape reference. The caller must separately verify the outer
// canonical transaction, signature, account/state rules and resource fee.
// Violation values describe this reference only; the full validator applies
// protocol-v1's ordered consensus rejection-code registry.
class V1FixedFunctionPolicy {
public:
  enum class Violation {
    NONE,
    OVERSIZE,
    UNKNOWN_TYPE,
    INVALID_AMOUNT,
    INVALID_SHAPE,
    INVALID_ACTION,
    INVALID_CHOICE,
  };

  static constexpr std::uint32_t kMaxTransactionBytes = 262'144;

  // payload is the contents of the canonical transaction's payload:bytes.
  static Violation validateTransaction(std::uint8_t type,
                                       std::uint64_t amount,
                                       std::span<const unsigned char> payload);

  // action is the contents of a canonical GOVERNANCE_PROPOSE action:bytes.
  static Violation validateGovernanceAction(
      std::uint8_t kind, std::span<const unsigned char> action);
};

} // namespace nodo::consensus

#endif
