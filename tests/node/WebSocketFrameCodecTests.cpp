#include "node/WebSocketFrameCodec.hpp"

#include <cassert>
#include <iostream>

using nodo::node::WebSocketFrameCodec;
using nodo::node::WebSocketOpcode;

void testServerTextFrame() {
  const std::string frame = WebSocketFrameCodec::textFrame("hello");
  assert(frame.size() == 7);
  assert(static_cast<unsigned char>(frame[0]) == 0x81);
  assert(static_cast<unsigned char>(frame[1]) == 5);
  assert(frame.substr(2) == "hello");
}

void testMaskedClientCloseFrameDecode() {
  std::string bytes;
  bytes.push_back(static_cast<char>(0x88));
  bytes.push_back(static_cast<char>(0x80));
  bytes.append("\x01\x02\x03\x04", 4);
  const auto decoded = WebSocketFrameCodec::decodeClientFrame(bytes);
  assert(decoded.has_value());
  assert(decoded->opcode == WebSocketOpcode::CLOSE);
}

int main() {
  testServerTextFrame();
  testMaskedClientCloseFrameDecode();
  std::cout << "Nodo WebSocketFrameCodec tests passed.\n";
  return 0;
}
