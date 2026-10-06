#include "utils/Logger.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
  std::ostringstream captured;
  std::streambuf *original = std::clog.rdbuf(captured.rdbuf());
  nodo::utils::log(nodo::utils::LogLevel::ERROR, "rpc\"worker")
      << "bad\nrequest";
  std::clog.rdbuf(original);

  const std::string line = captured.str();
  assert(line.find("\"level\":\"ERROR\"") != std::string::npos);
  assert(line.find("\"component\":\"rpc\\\"worker\"") !=
         std::string::npos);
  assert(line.find("\"message\":\"bad\\u000arequest\"") !=
         std::string::npos);
  assert(!line.empty() && line.back() == '\n');
  assert(nodo::utils::escapeLogField(std::string(1, '\xFF')) == "\\u00ff");
  return 0;
}
