#ifndef NODO_NODE_WEBSOCKET_FRAME_CODEC_HPP
#define NODO_NODE_WEBSOCKET_FRAME_CODEC_HPP

#include <cstddef>
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
  // RFC 6455 requires every client-to-server frame to be masked; a server
  // must close the connection on an unmasked one.
  bool masked = false;
  // Bytes of the input this frame occupied, so a caller holding several
  // frames, or the start of the next one, can consume them in order.
  std::size_t consumedBytes = 0;
};

class WebSocketFrameCodec {
public:
  static std::string textFrame(const std::string &payload);
  static std::string closeFrame(const std::string &reason = "");
  static std::string pongFrame(const std::string &payload = "");

  // Decodes the first frame in bytes. Returns nullopt when bytes do not yet
  // hold a complete frame.
  static std::optional<WebSocketFrame>
  decodeClientFrame(const std::string &bytes);

private:
  static std::string encodeFrame(WebSocketOpcode opcode,
                                 const std::string &payload);
};

} // namespace nodo::node

#endif
