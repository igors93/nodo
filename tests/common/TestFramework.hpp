#ifndef NODO_TESTS_COMMON_TEST_FRAMEWORK_HPP
#define NODO_TESTS_COMMON_TEST_FRAMEWORK_HPP

#include <stdexcept>
#include <string>
#include <string_view>

namespace nodo::test {

inline void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

} // namespace nodo::test

#endif
