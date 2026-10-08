#ifndef NODO_CONSENSUS_BFT_TIME_HPP
#define NODO_CONSENSUS_BFT_TIME_HPP

#include "consensus/QuorumCertificate.hpp"
#include "core/Block.hpp"

#include <cstdint>
#include <optional>

namespace nodo::consensus {

// Deterministic v1 time calculation. It uses only the parent block, its
// authenticated PRECOMMIT certificate, and the immutable set for that parent
// height. Local wall and monotonic clocks are deliberately absent.
class BftTime {
public:
  static std::optional<std::int64_t> expectedChildTime(
      const core::Block &parent, std::uint64_t targetBlockSeconds,
      const QuorumCertificate *parentCertificate,
      const core::ValidatorSetHistory *validatorSetHistory,
      const crypto::CryptoPolicy &policy,
      const crypto::SignatureProvider &provider);
};

} // namespace nodo::consensus

#endif
