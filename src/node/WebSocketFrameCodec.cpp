#include "node/WebSocketFrameCodec.hpp"

#include <array>
#include <limits>

namespace nodo::node {

std::string WebSocketFrameCodec::encodeFrame(WebSocketOpcode opcode,
                                             const std::string &payload) {
  std::string out;
  out.push_back(static_cast<char>(0x80u | static_cast<std::uint8_t>(opcode)));

  const std::uint64_t size = static_cast<std::uint64_t>(payload.size());
  if (size <= 125) {
    out.push_back(static_cast<char>(size));
  } else if (size <= std::numeric_limits<std::uint16_t>::max()) {
    out.push_back(static_cast<char>(126));
    out.push_back(static_cast<char>((size >> 8) & 0xffu));
    out.push_back(static_cast<char>(size & 0xffu));
  } else {
    out.push_back(static_cast<char>(127));
    for (int shift = 56; shift >= 0; shift -= 8) {
      out.push_back(static_cast<char>((size >> shift) & 0xffu));
    }
  }
  out += payload;
  return out;
}

std::string WebSocketFrameCodec::textFrame(const std::string &payload) {
  return encodeFrame(WebSocketOpcode::TEXT, payload);
}

std::string WebSocketFrameCodec::closeFrame(const std::string &reason) {
  return encodeFrame(WebSocketOpcode::CLOSE, reason);
}

std::string WebSocketFrameCodec::pongFrame(const std::string &payload) {
  return encodeFrame(WebSocketOpcode::PONG, payload);
}

std::optional<WebSocketFrame>
WebSocketFrameCodec::decodeClientFrame(const std::string &bytes) {
  if (bytes.size() < 2) {
    return std::nullopt;
  }
  const auto b0 = static_cast<unsigned char>(bytes[0]);
  const auto b1 = static_cast<unsigned char>(bytes[1]);
  const bool fin = (b0 & 0x80u) != 0;
  const auto opcode = static_cast<WebSocketOpcode>(b0 & 0x0fu);
  const bool masked = (b1 & 0x80u) != 0;
  std::uint64_t payloadLen = (b1 & 0x7fu);
  std::size_t cursor = 2;

  if (payloadLen == 126) {
    if (bytes.size() < cursor + 2)
      return std::nullopt;
    payloadLen = (static_cast<unsigned char>(bytes[cursor]) << 8) |
                 static_cast<unsigned char>(bytes[cursor + 1]);
    cursor += 2;
  } else if (payloadLen == 127) {
    if (bytes.size() < cursor + 8)
      return std::nullopt;
    payloadLen = 0;
    for (int i = 0; i < 8; ++i) {
      payloadLen =
          (payloadLen << 8) | static_cast<unsigned char>(bytes[cursor + i]);
    }
    cursor += 8;
  }

  std::array<unsigned char, 4> mask{0, 0, 0, 0};
  if (masked) {
    if (bytes.size() < cursor + 4)
      return std::nullopt;
    for (int i = 0; i < 4; ++i) {
      mask[static_cast<std::size_t>(i)] =
          static_cast<unsigned char>(bytes[cursor + i]);
    }
    cursor += 4;
  }

  if (payloadLen > static_cast<std::uint64_t>(bytes.size() - cursor)) {
    return std::nullopt;
  }
  std::string payload;
  payload.resize(static_cast<std::size_t>(payloadLen));
  for (std::size_t i = 0; i < payload.size(); ++i) {
    unsigned char value = static_cast<unsigned char>(bytes[cursor + i]);
    if (masked) {
      value ^= mask[i % 4];
    }
    payload[i] = static_cast<char>(value);
  }
  return WebSocketFrame{opcode, payload, fin};
}

} // namespace nodo::node
