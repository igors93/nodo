#ifndef NODO_UTILS_JSON_TEXT_HPP
#define NODO_UTILS_JSON_TEXT_HPP

#include <string>

namespace nodo::utils {

/*
 * Returns value as a quoted JSON string literal.
 *
 * Every control character is escaped and invalid UTF-8 is replaced with
 * U+FFFD, so text read from the chain, from peers or from clients can never
 * produce invalid JSON in an RPC response.
 */
std::string jsonString(const std::string &value);

} // namespace nodo::utils

#endif
