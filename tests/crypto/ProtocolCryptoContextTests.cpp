#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/CryptoAlgorithm.hpp"

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

void testLocalnetContextIsExplicitAndValid() {
    const nodo::crypto::ProtocolCryptoContext context =
        nodo::crypto::ProtocolCryptoContext::localnet();

    requireCondition(
        context.isValid(),
        "Localnet crypto context should be valid."
    );

    requireCondition(
        context.networkProfile() == "localnet",
        "Localnet crypto context should expose the localnet profile."
    );

    requireCondition(
        !context.productionSafe(),
        "Localnet temporary crypto context must not be marked production-safe."
    );

    requireCondition(
        context.policy().developmentMode(),
        "Localnet crypto context should currently use development-mode policy."
    );

    requireCondition(
        context.signatureProvider().algorithm() ==
            nodo::crypto::CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "Localnet crypto context should expose the temporary local signature provider."
    );
}

} // namespace

int main() {
    try {
        testLocalnetContextIsExplicitAndValid();

        std::cout << "Nodo protocol crypto context tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo protocol crypto context tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
