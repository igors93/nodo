#include "serialization/ProtocolMessageCodec.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                        std::size_t size) {
  if (size > 65536) return 0;
  try {
    std::vector<unsigned char> bytes(data, data + size);
    (void)nodo::serialization::ProtocolMessageCodec::decodeNetworkEnvelope(bytes);
  } catch (const std::exception &) {
    // Malformed input is expected; memory errors and crashes are not.
  }
  return 0;
}
