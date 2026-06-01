#include "core/GenesisVerifier.hpp"

#include <set>

namespace nodo::core {

std::string genesisVerificationStatusToString(GenesisVerificationStatus status) {
    switch (status) {
        case GenesisVerificationStatus::VALID:
            return "VALID";
        case GenesisVerificationStatus::EMPTY_VALIDATOR_SET:
            return "EMPTY_VALIDATOR_SET";
        case GenesisVerificationStatus::VALIDATOR_DUPLICATE:
            return "VALIDATOR_DUPLICATE";
        case GenesisVerificationStatus::EMPTY_ADDRESS:
            return "EMPTY_ADDRESS";
        case GenesisVerificationStatus::NEGATIVE_BALANCE:
            return "NEGATIVE_BALANCE";
        case GenesisVerificationStatus::NEGATIVE_SUPPLY:
            return "NEGATIVE_SUPPLY";
        case GenesisVerificationStatus::SUPPLY_MISMATCH:
            return "SUPPLY_MISMATCH";
        case GenesisVerificationStatus::INVALID_NETWORK_PARAMETERS:
            return "INVALID_NETWORK_PARAMETERS";
        case GenesisVerificationStatus::INVALID_TIMESTAMP:
            return "INVALID_TIMESTAMP";
        default:
            return "UNKNOWN";
    }
}

GenesisVerificationResult::GenesisVerificationResult()
    : m_valid(false),
      m_status(GenesisVerificationStatus::INVALID_NETWORK_PARAMETERS),
      m_reason("") {}

GenesisVerificationResult GenesisVerificationResult::valid() {
    GenesisVerificationResult r;
    r.m_valid = true;
    r.m_status = GenesisVerificationStatus::VALID;
    r.m_reason = "";
    return r;
}

GenesisVerificationResult GenesisVerificationResult::invalid(
    GenesisVerificationStatus status,
    std::string reason
) {
    GenesisVerificationResult r;
    r.m_valid = false;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool GenesisVerificationResult::isValid() const { return m_valid; }
GenesisVerificationStatus GenesisVerificationResult::status() const { return m_status; }
const std::string& GenesisVerificationResult::reason() const { return m_reason; }

GenesisVerificationResult GenesisVerifier::verify(
    const config::GenesisConfig& genesis
) {
    if (!genesis.networkParameters().isValid()) {
        return GenesisVerificationResult::invalid(
            GenesisVerificationStatus::INVALID_NETWORK_PARAMETERS,
            "Genesis network parameters are not valid."
        );
    }

    if (genesis.genesisTimestamp() <= 0) {
        return GenesisVerificationResult::invalid(
            GenesisVerificationStatus::INVALID_TIMESTAMP,
            "Genesis timestamp must be positive."
        );
    }

    const auto& validators = genesis.bootstrapValidators();
    if (validators.empty()) {
        return GenesisVerificationResult::invalid(
            GenesisVerificationStatus::EMPTY_VALIDATOR_SET,
            "Genesis must have at least one bootstrap validator."
        );
    }

    std::set<std::string> seenAddresses;
    for (const auto& v : validators) {
        const std::string addr = v.validatorAddress();
        if (addr.empty()) {
            return GenesisVerificationResult::invalid(
                GenesisVerificationStatus::EMPTY_ADDRESS,
                "A bootstrap validator has an empty address. "
                "Check that all validators have a valid public key."
            );
        }
        if (!seenAddresses.insert(addr).second) {
            return GenesisVerificationResult::invalid(
                GenesisVerificationStatus::VALIDATOR_DUPLICATE,
                "Duplicate bootstrap validator address: " + addr
            );
        }
    }

    utils::Amount totalBalances = utils::Amount::fromRawUnits(0);
    for (const auto& account : genesis.genesisAccounts()) {
        if (!account.isValid()) {
            return GenesisVerificationResult::invalid(
                GenesisVerificationStatus::EMPTY_ADDRESS,
                "A genesis account has an invalid configuration."
            );
        }
        if (account.balance().isNegative()) {
            return GenesisVerificationResult::invalid(
                GenesisVerificationStatus::NEGATIVE_BALANCE,
                "Genesis account '" + account.address() +
                "' has a negative balance."
            );
        }
        totalBalances = totalBalances + account.balance();
    }

    if (totalBalances.isNegative()) {
        return GenesisVerificationResult::invalid(
            GenesisVerificationStatus::NEGATIVE_SUPPLY,
            "Total genesis supply is negative."
        );
    }

    return GenesisVerificationResult::valid();
}

GenesisVerificationResult GenesisVerifier::verifySupplyBalance(
    utils::Amount totalAccountBalances,
    utils::Amount declaredTotalSupply
) {
    if (declaredTotalSupply.isNegative()) {
        return GenesisVerificationResult::invalid(
            GenesisVerificationStatus::NEGATIVE_SUPPLY,
            "Declared total supply is negative."
        );
    }
    if (totalAccountBalances.isNegative()) {
        return GenesisVerificationResult::invalid(
            GenesisVerificationStatus::NEGATIVE_SUPPLY,
            "Total account balances sum to a negative value."
        );
    }
    if (totalAccountBalances != declaredTotalSupply) {
        return GenesisVerificationResult::invalid(
            GenesisVerificationStatus::SUPPLY_MISMATCH,
            "Total account balances (" + totalAccountBalances.toString() +
            ") do not match declared total supply (" +
            declaredTotalSupply.toString() + ")."
        );
    }
    return GenesisVerificationResult::valid();
}

} // namespace nodo::core
