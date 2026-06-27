#ifndef NODO_TESTS_COMMON_CONSENSUS_PHASE_TEST_FIXTURES_HPP
#define NODO_TESTS_COMMON_CONSENSUS_PHASE_TEST_FIXTURES_HPP

#include "config/NetworkParameters.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace nodo::test {

inline crypto::KeyPair consensusTestUserKey(const std::string& seed) {
    return crypto::KeyPair::createDeterministicEd25519KeyPair(seed);
}

inline config::GenesisAccountConfig fundedConsensusTestAccount(
    const crypto::KeyPair& userKey
) {
    return config::GenesisAccountConfig(
        userKey.address().value(),
        utils::Amount::fromRawUnits(2'000'000'000'000LL),
        0
    );
}

inline void admitConsensusTestTransfer(
    node::NodeRuntime& runtime,
    const crypto::KeyPair& userKey,
    std::uint64_t nonce,
    std::int64_t transactionTimestamp
) {
    const crypto::Ed25519SignatureProvider provider;
    const core::Transaction transaction =
        core::TransactionBuilder::buildSignedTransfer(
            core::TransactionBuildRequest(
                "consensus-phase-recipient",
                utils::Amount::fromRawUnits(1'000),
                utils::Amount::fromRawUnits(100),
                nonce,
                transactionTimestamp
            ),
            crypto::Signer(userKey, provider),
            runtime.config().genesisConfig().networkParameters().chainId()
        );

    const auto admission = runtime.mutableMempool().admitTransaction(
        transaction,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        transactionTimestamp + 1
    );

    if (!admission.accepted()) {
        throw std::runtime_error(
            "Consensus test transaction admission failed: " + admission.reason()
        );
    }
}

} // namespace nodo::test

#endif
