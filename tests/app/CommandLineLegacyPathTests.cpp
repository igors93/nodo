#include "app/CommandLineInterface.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::app::CommandLineInterface;
using nodo::app::CommandLineResult;
using nodo::app::CommandLineStatus;

CommandLineResult execute(const std::vector<std::string>& args) {
    return CommandLineInterface::execute(args);
}

void testDemoCommandBlockedOnTestnetCandidate() {
    const auto result = execute({"demo", "--network", "testnet-candidate"});
    assert(!result.success());
    assert(result.status() == CommandLineStatus::COMMAND_FAILED);
    const std::string& msg = result.message();
    assert(msg.find("localnet") != std::string::npos ||
           msg.find("testnet-candidate") != std::string::npos ||
           msg.find("demo") != std::string::npos);
}

void testHelpTextContainsLocalnetOnlyLabel() {
    const auto result = execute({"help"});
    assert(result.success());
    const std::string& msg = result.message();
    assert(msg.find("Localnet-only") != std::string::npos ||
           msg.find("localnet") != std::string::npos);
    assert(msg.find("nodo init") != std::string::npos);
    assert(msg.find("nodo block produce") != std::string::npos);
}

void testUnknownCommandRejected() {
    const auto result = execute({"bogus-command-xyz"});
    assert(!result.success());
}

} // namespace

int main() {
    testDemoCommandBlockedOnTestnetCandidate();
    testHelpTextContainsLocalnetOnlyLabel();
    testUnknownCommandRejected();
    return 0;
}
