#ifndef NODO_CORE_PROTOCOL_LIMITS_HPP
#define NODO_CORE_PROTOCOL_LIMITS_HPP

#include <cstddef>
#include <cstdint>

namespace nodo::core::ProtocolLimits {

inline constexpr std::size_t MAX_BLOCK_RECORDS = 10'000;
inline constexpr std::size_t MAX_SERIALIZED_BLOCK_BYTES = 1024 * 1024;
inline constexpr std::size_t MAX_RECORD_PAYLOAD_BYTES = 512 * 1024;

// Network messages include proposal signatures or hex-encoded sync artifacts,
// so the transport envelope must have bounded headroom above one full block.
inline constexpr std::size_t MAX_NETWORK_PAYLOAD_BYTES = 4 * 1024 * 1024;
inline constexpr std::size_t MAX_TRANSPORT_FRAME_BYTES = 5 * 1024 * 1024;
inline constexpr std::uint64_t MAX_PERSISTENT_SYNC_BLOCK_BATCH = 1;
inline constexpr std::size_t MAX_SLASHING_EVIDENCE_PER_BLOCK = 32;

} // namespace nodo::core::ProtocolLimits

#endif
