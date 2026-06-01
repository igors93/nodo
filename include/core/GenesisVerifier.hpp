#ifndef NODO_CORE_GENESIS_VERIFIER_HPP
#define NODO_CORE_GENESIS_VERIFIER_HPP

#include "config/NetworkParameters.hpp"
#include "utils/Amount.hpp"

#include <string>

namespace nodo::core {

enum class GenesisVerificationStatus {
    VALID,
    EMPTY_VALIDATOR_SET,
    VALIDATOR_DUPLICATE,
    EMPTY_ADDRESS,
    NEGATIVE_BALANCE,
    NEGATIVE_SUPPLY,
    SUPPLY_MISMATCH,
    INVALID_NETWORK_PARAMETERS,
    INVALID_TIMESTAMP
};

std::string genesisVerificationStatusToString(GenesisVerificationStatus status);

class GenesisVerificationResult {
public:
    GenesisVerificationResult();

    static GenesisVerificationResult valid();
    static GenesisVerificationResult invalid(
        GenesisVerificationStatus status,
        std::string reason
    );

    bool isValid() const;
    GenesisVerificationStatus status() const;
    const std::string& reason() const;

private:
    bool m_valid;
    GenesisVerificationStatus m_status;
    std::string m_reason;
};

/*
 * GenesisVerifier validates that a GenesisConfig is self-consistent and safe
 * to use as the canonical starting point of a Nodo network.
 *
 * Security principle:
 * Genesis is the root of trust for the entire chain. A genesis with duplicate
 * validators, negative balances, or a mismatched supply total would silently
 * corrupt all downstream state. Every genesis used on an official network must
 * pass this verifier before any block is produced.
 */
class GenesisVerifier {
public:
    static GenesisVerificationResult verify(
        const config::GenesisConfig& genesis
    );

    static GenesisVerificationResult verifySupplyBalance(
        utils::Amount totalAccountBalances,
        utils::Amount declaredTotalSupply
    );
};

} // namespace nodo::core

#endif
