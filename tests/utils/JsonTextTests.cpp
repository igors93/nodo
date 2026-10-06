#include "utils/JsonText.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::utils::jsonString;

void testEscapesQuotesAndBackslashes() {
  assert(jsonString("plain") == "\"plain\"");
  assert(jsonString("a\"b\\c") == "\"a\\\"b\\\\c\"");
  assert(jsonString("") == "\"\"");
}

// The copies this replaced escaped only \n, \r and \t, so any other control
// byte produced invalid JSON in RPC responses.
void testEscapesEveryControlCharacter() {
  assert(jsonString("\n\r\t") == "\"\\n\\r\\t\"");
  assert(jsonString(std::string("a\x01" "b\x1f" "c", 5)) ==
         "\"a\\u0001b\\u001fc\"");
  assert(jsonString(std::string("x\0y", 3)) == "\"x\\u0000y\"");
}

void testKeepsUtf8AndReplacesInvalidBytes() {
  assert(jsonString("caf\xc3\xa9") == "\"caf\xc3\xa9\"");
  assert(jsonString("bad\xff") == "\"bad\xef\xbf\xbd\"");
}

} // namespace

int main() {
  testEscapesQuotesAndBackslashes();
  testEscapesEveryControlCharacter();
  testKeepsUtf8AndReplacesInvalidBytes();
  return 0;
}
