#include "node/PersistentMempoolStore.hpp"
#include "node/SeenTransactionCache.hpp"

#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "mempool/Mempool.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::PublicKey;
using SecurityContext = nodo::crypto::SecurityContext;
using nodo::crypto::SignatureBundle;
using nodo::crypto::SigningDomain;
using nodo::mempool::Mempool;
using nodo::node::PersistentMempoolStore;
using nodo::node::SeenTransactionCache;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

KeyPair txKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair("gossip-test-tx-key");
}

Transaction signedTransfer(std::uint64_t nonce) {
    const KeyPair kp = txKeyPair();
    Transaction tx(
        TransactionType::TRANSFER,
        kp.address().value(),
        "gossip-recipient",
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(100),
        nonce,
        kTimestamp + static_cast<std::int64_t>(nonce)
    );

    const Ed25519SignatureProvider provider;

    tx.withChainId("nodo-localnet-1");

    tx.attachSignatureBundle(
        SignatureBundle::createSignature(
            tx.signingPayload(),
            kp.publicKey(),
            kp.privateKeyForSigningOnly(),
            tx.timestamp(),
            provider,
            SigningDomain::USER_TRANSACTION
        )
    );

    return tx;
}

void testSerializeForGossipRoundTrip() {
    const Transaction tx = signedTransfer(1);
    const PublicKey pk = txKeyPair().publicKey();

    const std::string payload =
        PersistentMempoolStore::serializeForGossip(tx, pk, kTimestamp);

    requireCondition(!payload.empty(), "serializeForGossip should produce non-empty payload.");

    const CryptoPolicy policy = CryptoPolicy::developmentPolicy();
    Mempool mempool;

    const auto decoded = PersistentMempoolStore::deserializeGossip(
        payload,
        policy,
        SecurityContext::USER_TRANSACTION,
        "nodo-localnet-1"
    );

    const bool admitted = decoded.has_value() && mempool.admitTransaction(
        decoded->transaction, policy, SecurityContext::USER_TRANSACTION,
        decoded->acceptedAt
    ).success();
    requireCondition(admitted, "deserializeGossip should decode a valid signed tx for admission.");
    requireCondition(!mempool.empty(), "Mempool should have one transaction after admission.");
}

void testGossipDeduplicationViaSeenCache() {
    const Transaction tx = signedTransfer(2);
    const PublicKey pk = txKeyPair().publicKey();
    const std::string payload =
        PersistentMempoolStore::serializeForGossip(tx, pk, kTimestamp);

    requireCondition(!payload.empty(), "Payload should not be empty.");

    SeenTransactionCache cache;

    // Simulate payload hash as cache key (we use tx id as a proxy here).
    const std::string cacheKey = tx.id();

    const bool firstSeen  = cache.markSeen(cacheKey, kTimestamp);
    const bool secondSeen = cache.markSeen(cacheKey, kTimestamp + 1);

    requireCondition(firstSeen,  "First call should mark as newly seen.");
    requireCondition(!secondSeen, "Second call for same tx should return false (duplicate).");
}

void testInvalidGossipPayloadRejected() {
    const CryptoPolicy policy = CryptoPolicy::developmentPolicy();
    Mempool mempool;

    const auto decoded = PersistentMempoolStore::deserializeGossip(
        "invalid-gossip-payload",
        policy,
        SecurityContext::USER_TRANSACTION,
        "nodo-localnet-1"
    );

    requireCondition(!decoded.has_value(), "Invalid gossip payload should be rejected.");
    requireCondition(mempool.empty(), "Mempool should remain empty after rejection.");
}

void testUnsignedTransactionGossipIsEmpty() {
    Transaction tx(
        TransactionType::TRANSFER,
        "gossip-sender",
        "gossip-recipient",
        Amount::fromRawUnits(500),
        Amount::fromRawUnits(50),
        99,
        kTimestamp
    );

    const PublicKey pk = txKeyPair().publicKey();
    const std::string payload =
        PersistentMempoolStore::serializeForGossip(tx, pk, kTimestamp);

    requireCondition(payload.empty(), "serializeForGossip with unsigned tx should return empty.");
}

} // namespace

int main() {
    try {
        testSerializeForGossipRoundTrip();
        testGossipDeduplicationViaSeenCache();
        testInvalidGossipPayloadRejected();
        testUnsignedTransactionGossipIsEmpty();

        std::cout << "TransactionGossip tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TransactionGossip tests failed: " << error.what() << "\n";
        return 1;
    }
}
