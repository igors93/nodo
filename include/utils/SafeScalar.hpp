#ifndef NODO_UTILS_SAFE_SCALAR_HPP
#define NODO_UTILS_SAFE_SCALAR_HPP

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string_view>

namespace nodo::utils {

inline bool isSafeIdentifier(
    std::string_view value,
    std::size_t maximum = std::numeric_limits<std::size_t>::max(),
    std::string_view extra = "_-.:/") {
  if (value.empty() || value.size() > maximum) return false;
  return std::all_of(value.begin(), value.end(), [extra](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || extra.find(c) != std::string_view::npos;
  });
}

inline bool isSafeDelimitedText(
    std::string_view value,
    std::size_t maximum,
    std::string_view forbidden) {
  if (value.empty() || value.size() > maximum) return false;
  return value.find_first_of(forbidden) == std::string_view::npos;
}

} // namespace nodo::utils

#endif
