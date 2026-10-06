#ifndef NODO_UTILS_TIME_HPP
#define NODO_UTILS_TIME_HPP

#include <cstdint>

namespace nodo::utils {

/* Return the current Unix timestamp in seconds. Centralizing clock reads keeps time handling consistent. */
std::int64_t currentUnixTimestamp();

} // namespace nodo::utils

#endif