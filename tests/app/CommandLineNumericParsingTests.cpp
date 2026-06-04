#include "app/CommandLineInterface.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::app::CommandLineInterface;
using nodo::app::CommandLineStatus;

void requireSuccess(
    const std::vector<std::string>& args,
    const std::string& label
) {
    const auto result = CommandLineInterface::execute(args);
    assert(result.success() && ("Expected success for: " + label).c_str());
}

void requireFailure(
    const std::vector<std::string>& args,
    const std::string& label
) {
    const auto result = CommandLineInterface::execute(args);
    assert(!result.success() && ("Expected failure for: " + label).c_str());
    assert(result.status() == CommandLineStatus::INVALID_ARGUMENTS
        && ("Expected INVALID_ARGUMENTS for: " + label).c_str());
}

void testAmountValidInteger() {
    const auto result = CommandLineInterface::execute({"help"});
    assert(result.success());
}

void testAmountAlphaFails() {
    requireFailure(
        {"tx", "submit", "--amount", "abc"},
        "--amount abc"
    );
}

void testFeeNegativeFails() {
    requireFailure(
        {"tx", "submit", "--fee", "-1"},
        "--fee -1"
    );
}

void testNonceNegativeFails() {
    requireFailure(
        {"tx", "submit", "--nonce", "-1"},
        "--nonce -1"
    );
}

void testNonceAlphanumericFails() {
    requireFailure(
        {"tx", "submit", "--nonce", "10x"},
        "--nonce 10x"
    );
}

void testAmountEmptyFails() {
    requireFailure(
        {"tx", "submit", "--amount", ""},
        "--amount empty"
    );
}

void testTimestampNonNumericFails() {
    requireFailure(
        {"tx", "submit", "--timestamp", "notanumber"},
        "--timestamp notanumber"
    );
}

} // namespace

int main() {
    testAmountValidInteger();
    testAmountAlphaFails();
    testFeeNegativeFails();
    testNonceNegativeFails();
    testNonceAlphanumericFails();
    testAmountEmptyFails();
    testTimestampNonNumericFails();
    return 0;
}
