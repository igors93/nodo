#include "utils/JsonText.hpp"

#include <nlohmann/json.hpp>

namespace nodo::utils {

std::string jsonString(const std::string &value) {
  return nlohmann::json(value).dump(-1, ' ', false,
                                    nlohmann::json::error_handler_t::replace);
}

} // namespace nodo::utils
