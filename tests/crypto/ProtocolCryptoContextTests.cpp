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
        context.profile() == nodo::crypto::ProtocolNetworkProfile::LOCALNET,
        "Localnet crypto context should expose the localnet profile enum."
    );

    requireCondition(
        context.networkProfile() == "localnet",
        "Localnet crypto context should expose the localnet profile name."
    );

    requireCondition(
        !context.temporaryProviderAllowed(),
        "Localnet should not allow temporary providers in protocol crypto."
    );

    requireCondition(
        !context.requiresProductionProvider(),
        "Localnet should not require a production provider yet."
    );

    requireCondition(
        !context.productionSafe(),
        "Localnet development policy must not be marked production-safe."
    );

    requireCondition(
        context.policy().developmentMode(),
        "Localnet crypto context should currently use development-mode policy."
    );

    requireCondition(
        context.signatureProvider().algorithm() ==
            nodo::crypto::CryptoAlgorithm::BLS12_381,
        "Localnet validator crypto context should expose BLS12-381."
    );

    requireCondition(
        context.userSignatureProvider().algorithm() ==
            nodo::crypto::CryptoAlgorithm::CLASSIC_ED25519,
        "Localnet user crypto context should expose Ed25519."
    );
}

void testNetworkNameMappingAcceptsLocalnet() {
    const nodo::crypto::ProtocolCryptoContext context =
        nodo::crypto::ProtocolCryptoContext::fromNetworkName("localnet");

    requireCondition(
        context.isValid(),
        "localnet should map to a valid localnet crypto context."
    );

    requireCondition(
        context.profile() == nodo::crypto::ProtocolNetworkProfile::LOCALNET,
        "localnet should map to the localnet profile."
    );
}

void testTestnetRefusesTemporaryProvider() {
    const nodo::crypto::ProtocolCryptoContext context =
        nodo::crypto::ProtocolCryptoContext::testnet();

    requireCondition(
        !context.isValid(),
        "Testnet must refuse temporary cryptography until a production provider exists."
    );

    requireCondition(
        context.profile() == nodo::crypto::ProtocolNetworkProfile::TESTNET,
        "Testnet crypto context should expose the testnet profile."
    );

    requireCondition(
        context.requiresProductionProvider(),
        "Testnet should require a production provider."
    );

    requireCondition(
        !context.temporaryProviderAllowed(),
        "Testnet should not allow temporary provider."
    );

    requireCondition(
        context.rejectionReason().find("production-safe") != std::string::npos,
        "Testnet rejection reason should explain production provider requirement."
    );
}

void testMainnetRefusesTemporaryProvider() {
    const nodo::crypto::ProtocolCryptoContext context =
        nodo::crypto::ProtocolCryptoContext::mainnet();

    requireCondition(
        !context.isValid(),
        "Mainnet must refuse temporary cryptography until a production provider exists."
    );

    requireCondition(
        context.profile() == nodo::crypto::ProtocolNetworkProfile::MAINNET,
        "Mainnet crypto context should expose the mainnet profile."
    );

    requireCondition(
        context.requiresProductionProvider(),
        "Mainnet should require a production provider."
    );

    requireCondition(
        !context.temporaryProviderAllowed(),
        "Mainnet should not allow temporary provider."
    );

    requireCondition(
        context.rejectionReason().find("production-safe") != std::string::npos,
        "Mainnet rejection reason should explain production provider requirement."
    );
}

void testUnknownNetworkIsRejected() {
    const nodo::crypto::ProtocolCryptoContext context =
        nodo::crypto::ProtocolCryptoContext::fromNetworkName("unknown-network");

    requireCondition(
        !context.isValid(),
        "Unknown network should be rejected."
    );

    requireCondition(
        context.profile() == nodo::crypto::ProtocolNetworkProfile::UNKNOWN,
        "Unknown network should map to the unknown profile."
    );

    requireCondition(
        context.rejectionReason().find("Unknown network profile") != std::string::npos,
        "Unknown network rejection should explain the mapping failure."
    );
}

} // namespace

int main() {
    try {
        testLocalnetContextIsExplicitAndValid();
        testNetworkNameMappingAcceptsLocalnet();
        testTestnetRefusesTemporaryProvider();
        testMainnetRefusesTemporaryProvider();
        testUnknownNetworkIsRejected();

        std::cout << "Nodo protocol crypto context tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo protocol crypto context tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
