#include "config/GenesisRegistry.hpp"

#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PublicKey.hpp"
#include "utils/Amount.hpp"

#include <stdexcept>

namespace nodo::config {

namespace {

crypto::PublicKey deterministicValidatorKey(const std::string& seed) {
    return crypto::KeyPair::createDeterministicBls12381KeyPair(seed).publicKey();
}

crypto::PublicKey deterministicUserKey(const std::string& seed) {
    return crypto::KeyPair::createDeterministicEd25519KeyPair(seed).publicKey();
}

std::string testnetCandidateUserKeySeed() {
    return "nodo-testnet-candidate-user-seed";
}

std::string testnetCandidateValidatorKeySeed(std::size_t index) {
    return "nodo-testnet-candidate-validator-seed-" + std::to_string(index);
}

GenesisConfig buildLocalnetGenesis() {
    const NetworkParameters params = NetworkParameters::developmentLocal();

    const std::string userAddress =
        crypto::AddressDerivation::deriveFromPublicKey(
            deterministicUserKey(GenesisRegistry::localnetUserKeySeed())
        ).value();

    return GenesisConfig(
        params,
        1900000000,
        {
            BootstrapValidatorConfig(
                deterministicValidatorKey("nodo-localnet-validator-seed"),
                1,
                1,
                "localnet-genesis-validator"
            )
        },
        {
            GenesisAccountConfig(
                userAddress,
                utils::Amount::fromRawUnits(1000000000000),
                0
            )
        },
        "nodo-localnet-genesis"
    );
}

GenesisConfig buildTestnetCandidateGenesis() {
    const NetworkParameters params = NetworkParameters::testnetCandidate();

    std::vector<BootstrapValidatorConfig> validators;
    validators.reserve(params.minimumValidatorCount());

    for (std::size_t i = 0; i < params.minimumValidatorCount(); ++i) {
        validators.emplace_back(
            deterministicValidatorKey(testnetCandidateValidatorKeySeed(i)),
            1,
            1,
            "testnet-candidate-genesis-validator-" + std::to_string(i)
        );
    }

    const std::string userAddress =
        crypto::AddressDerivation::deriveFromPublicKey(
            deterministicUserKey(testnetCandidateUserKeySeed())
        ).value();

    return GenesisConfig(
        params,
        1900000000,
        std::move(validators),
        {
            GenesisAccountConfig(
                userAddress,
                utils::Amount::fromRawUnits(1000000000000),
                0
            )
        },
        "nodo-testnet-candidate-genesis"
    );
}

bool isLocalnetName(const std::string& name) {
    return name == "localnet";
}

bool isTestnetCandidateName(const std::string& name) {
    return name == "testnet-candidate";
}

} // namespace

// ---------------------------------------------------------------------------
// GenesisLookupResult
// ---------------------------------------------------------------------------

GenesisLookupResult::GenesisLookupResult()
    : m_found(false),
      m_genesis(),
      m_reason("Uninitialized genesis lookup result.") {}

GenesisLookupResult GenesisLookupResult::found(GenesisConfig genesis) {
    GenesisLookupResult r;
    r.m_found = true;
    r.m_genesis = std::move(genesis);
    r.m_reason = "";
    return r;
}

GenesisLookupResult GenesisLookupResult::missing(std::string reason) {
    GenesisLookupResult r;
    r.m_found = false;
    r.m_reason = std::move(reason);
    return r;
}

bool GenesisLookupResult::found() const { return m_found; }

const GenesisConfig& GenesisLookupResult::genesis() const {
    if (!m_found) {
        throw std::logic_error("GenesisLookupResult: genesis not found.");
    }
    return m_genesis;
}

const std::string& GenesisLookupResult::reason() const { return m_reason; }

// ---------------------------------------------------------------------------
// GenesisRegistry
// ---------------------------------------------------------------------------

GenesisLookupResult GenesisRegistry::get(const std::string& networkName) {
    if (isLocalnetName(networkName)) {
        return GenesisLookupResult::found(buildLocalnetGenesis());
    }

    if (isTestnetCandidateName(networkName)) {
        return GenesisLookupResult::found(buildTestnetCandidateGenesis());
    }

    if (networkName == "mainnet") {
        return GenesisLookupResult::missing(
            "mainnet does not have a registered genesis in this build. "
            "mainnet startup is not permitted until an audited genesis configuration is registered."
        );
    }

    return GenesisLookupResult::missing(
        "No registered genesis for network '" + networkName + "'. "
        "Unknown network profiles cannot start a runtime."
    );
}

bool GenesisRegistry::hasRegisteredGenesis(const std::string& networkName) {
    return get(networkName).found();
}

std::string GenesisRegistry::registeredGenesisId(const std::string& networkName) {
    const GenesisLookupResult result = get(networkName);
    if (!result.found()) {
        return "";
    }
    return result.genesis().deterministicId();
}

std::string GenesisRegistry::localnetUserKeySeed() {
    return "nodo-localnet-user-seed";
}

} // namespace nodo::config
