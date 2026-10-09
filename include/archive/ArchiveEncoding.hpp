#ifndef NODO_ARCHIVE_ARCHIVE_ENCODING_HPP
#define NODO_ARCHIVE_ARCHIVE_ENCODING_HPP

#include "serialization/V1EncodingPrimitives.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace nodo::archive {

// Shared encoding rules for history and archival objects (ADR 0014). Every
// new commitment uses the v1 domain-separated SHA-256 from ADR 0008 and is
// carried as 64 lowercase hex characters, like the development state roots.

inline bool isDigestHex(std::string_view value) {
  if (value.size() != 64) {
    return false;
  }
  bool nonZero = false;
  for (const char c : value) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
    nonZero = nonZero || c != '0';
  }
  return nonZero;
}

inline std::string hashHex(std::string_view domain,
                           const std::vector<unsigned char> &bytes) {
  return serialization::V1EncodingPrimitives::hex(
      serialization::V1EncodingPrimitives::hash(domain, bytes));
}

inline std::string hashHex(std::string_view domain, std::string_view bytes) {
  const std::vector<unsigned char> data(bytes.begin(), bytes.end());
  return hashHex(domain, data);
}

} // namespace nodo::archive

#endif
