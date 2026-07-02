#include "utils/Time.hpp"

#include <chrono>

namespace nodo::utils {

std::int64_t currentUnixTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
  return seconds.time_since_epoch().count();
}

} // namespace nodo::utils