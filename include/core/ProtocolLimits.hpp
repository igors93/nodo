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
inline constexpr std::size_t MAX_PENDING_SLASHING_EVIDENCE = 4096;
inline constexpr std::size_t MAX_SLASHING_EVIDENCE_GOSSIP_BYTES = 64 * 1024;
inline constexpr std::size_t MAX_SLASHING_EVIDENCE_PER_PEER_WINDOW = 16;
inline constexpr std::uint32_t SLASHING_EVIDENCE_PEER_WINDOW_SECONDS = 60;
inline constexpr std::size_t MAX_TRACKED_SLASHING_EVIDENCE_PEERS = 4096;
inline constexpr std::int64_t MAX_SLASHING_EVIDENCE_CLOCK_SKEW_SECONDS = 30;

} // namespace nodo::core::ProtocolLimits

#endif
