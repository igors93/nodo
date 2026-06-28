#include "core/Transaction.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace nodo;

    constexpr std::int64_t timestamp = 1900000000;
    const crypto::KeyPair keyPair =
        crypto::KeyPair::createDeterministicEd25519KeyPair(
            "canonical-transaction-authorization"
        );
    const crypto::Ed25519SignatureProvider provider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();

    core::Transaction transaction(
        core::TransactionType::TRANSFER,
        keyPair.address().value(),
        "canonical-authorization-recipient",
        utils::Amount::fromRawUnits(1000),
        utils::Amount::fromRawUnits(100),
        1,
        timestamp
    );
    transaction.withChainId("nodo-localnet-1");
    transaction.attachSignatureBundle(
        crypto::SignatureBundle::createSignature(
            transaction.signingPayload(),
            keyPair.publicKey(),
            keyPair.privateKeyForSigningOnly(),
            timestamp,
            provider,
            crypto::SigningDomain::USER_TRANSACTION
        )
    );

    const core::Transaction restored =
        core::Transaction::deserialize(transaction.serialize());
    assert(restored.serialize() == transaction.serialize());
    assert(restored.chainId() == "nodo-localnet-1");
    assert(restored.signatureBundle().signatures().front().publicKey().serialize()
           == keyPair.publicKey().serialize());
    assert(restored.verifyAuthorization(
        "nodo-localnet-1", policy,
        crypto::SecurityContext::USER_TRANSACTION, provider
    ));
    assert(!restored.verifyAuthorization(
        "nodo-other-chain", policy,
        crypto::SecurityContext::USER_TRANSACTION, provider
    ));

    bool rejectedSignatureReplacement = false;
    try {
        core::Transaction replacementAttempt = restored;
        replacementAttempt.attachSignatureBundle(transaction.signatureBundle());
    } catch (const std::logic_error&) {
        rejectedSignatureReplacement = true;
    }
    assert(rejectedSignatureReplacement);

    core::Transaction wrongSignature(
        core::TransactionType::TRANSFER,
        keyPair.address().value(),
        "canonical-authorization-recipient",
        utils::Amount::fromRawUnits(1000),
        utils::Amount::fromRawUnits(100),
        2,
        timestamp + 1
    );
    wrongSignature.withChainId("nodo-localnet-1");
    wrongSignature.attachSignatureBundle(
        crypto::SignatureBundle::createSignature(
            "not-the-transaction-payload",
            keyPair.publicKey(),
            keyPair.privateKeyForSigningOnly(),
            timestamp + 1,
            provider,
            crypto::SigningDomain::USER_TRANSACTION
        )
    );
    const core::Transaction restoredWrongSignature =
        core::Transaction::deserialize(wrongSignature.serialize());
    assert(!restoredWrongSignature.verifyAuthorization(
        "nodo-localnet-1", policy,
        crypto::SecurityContext::USER_TRANSACTION, provider
    ));

    std::cout << "canonical transaction authorization tests passed\n";
    return 0;
}
