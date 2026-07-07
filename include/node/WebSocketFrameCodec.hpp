#ifndef NODO_NODE_WEBSOCKET_FRAME_CODEC_HPP
#define NODO_NODE_WEBSOCKET_FRAME_CODEC_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

enum class WebSocketOpcode : std::uint8_t {
  CONTINUATION = 0x0,
  TEXT = 0x1,
  BINARY = 0x2,
  CLOSE = 0x8,
  PING = 0x9,
  PONG = 0xA
};

struct WebSocketFrame {
  WebSocketOpcode opcode = WebSocketOpcode::TEXT;
  std::string payload;
  bool finalFragment = true;
};

class WebSocketFrameCodec {
public:
  static std::string textFrame(const std::string &payload);
  static std::string closeFrame(const std::string &reason = "");
  static std::string pongFrame(const std::string &payload = "");

  static std::optional<WebSocketFrame>
  decodeClientFrame(const std::string &bytes);

private:
  static std::string encodeFrame(WebSocketOpcode opcode,
                                 const std::string &payload);
};

} // namespace nodo::node

#endif
