#ifndef NODO_UTILS_HASH_STRING_HPP
#define NODO_UTILS_HASH_STRING_HPP

#include "crypto/hash.h"

#include <string>
#include <string_view>

namespace nodo::utils {

// Preserve the existing C-string hash semantics for legacy protocol IDs.
inline std::string hashCString(const std::string &value) {
  char output[NODO_HASH_BUFFER_SIZE] = {0};
  nodo_hash_string(value.c_str(), output, sizeof(output));
  return std::string(output);
}

inline std::string hashBytes(std::string_view value) {
  char output[NODO_HASH_BUFFER_SIZE] = {0};
  nodo_hash_bytes(reinterpret_cast<const unsigned char *>(value.data()),
                  value.size(), output, sizeof(output));
  return std::string(output, NODO_HASH_HEX_SIZE);
}

} // namespace nodo::utils

#endif
