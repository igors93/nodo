#include "node/ChainAuditResult.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testPassedAuditHasHumanReadableOutput() {
    const nodo::node::ChainAuditResult result =
        nodo::node::ChainAuditResult::passed(
            "nodo-localnet",
            "localnet",
            7,
            "latest-hash",
            8,
            2,
            1
        );

    requireCondition(
        result.passed(),
        "Passed audit should report passed."
    );

    const std::string output =
        result.toHumanReadableString();

    requireCondition(
        output.find("Nodo chain audit passed.") != std::string::npos &&
        output.find("Network: nodo-localnet") != std::string::npos &&
        output.find("Crypto profile: localnet") != std::string::npos &&
        output.find("Latest height: 7") != std::string::npos,
        "Passed audit should contain readable network, crypto and height fields."
    );
}

void testFailedAuditHasReason() {
    const nodo::node::ChainAuditResult result =
        nodo::node::ChainAuditResult::failed(
            "rebuilt blockchain is invalid"
        );

    requireCondition(
        !result.passed(),
        "Failed audit should not report passed."
    );

    requireCondition(
        result.toHumanReadableString().find("rebuilt blockchain is invalid") != std::string::npos,
        "Failed audit should include the failure reason."
    );
}

} // namespace

int main() {
    try {
        testPassedAuditHasHumanReadableOutput();
        testFailedAuditHasReason();

        std::cout << "Nodo chain audit result tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo chain audit result tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
