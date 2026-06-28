#include "node/CanonicalSlashingTransition.hpp"

#include "crypto/CryptoSuiteId.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureProvider.hpp"
#include "serialization/LedgerRecordCodec.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000LL;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class TestBlsSignatureProvider final : public crypto::SignatureProvider {
public:
    crypto::CryptoAlgorithm algorithm() const override {
        return crypto::CryptoAlgorithm::BLS12_381;
    }

    crypto::Signature sign(
        const std::string&,
        const crypto::PublicKey& publicKey,
        const crypto::PrivateKey&,
        std::int64_t timestamp,
        crypto::SigningDomain domain
    ) const override {
        return crypto::Signature(
            crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
            domain,
            algorithm(),
            publicKey,
            std::string(192, 'c'),
            timestamp
        );
    }

    crypto::SignatureVerificationResult verify(
        const std::string& message,
        const crypto::Signature& signature
    ) const override {
        return !message.empty() && signature.isValid() &&
               signature.algorithm() == algorithm()
            ? crypto::SignatureVerificationResult::valid()
            : crypto::SignatureVerificationResult::invalid(
                "Invalid deterministic test signature."
            );
    }
};

crypto::KeyPair validatorKey() {
    return crypto::KeyPair(
        crypto::PublicKey(
            crypto::CryptoAlgorithm::BLS12_381,
            std::string(96, 'a')
        ),
        crypto::PrivateKey(
            crypto::CryptoAlgorithm::BLS12_381,
            std::string(64, 'b')
        )
    );
}

consensus::DoubleVoteEvidence evidenceFor(
    const crypto::KeyPair& key,
    const TestBlsSignatureProvider& provider
) {
    const auto makeVote = [&](const std::string& blockHash) {
        return consensus::ValidatorVoteRecord::createVote(
            key.address().value(),
            key.publicKey(),
            key.privateKeyForSigningOnly(),
            1,
            blockHash,
            "previous-block",
            1,
            consensus::ValidatorVoteDecision::PRECOMMIT,
            "double-vote-test",
            kTimestamp + 1,
            provider
        );
    };
    return consensus::DoubleVoteEvidence(
        makeVote("block-a"),
        makeVote("block-b"),
        kTimestamp + 2
    );
}

void testAppliesVerifiedEvidenceOnce() {
    const crypto::KeyPair key = validatorKey();
    const TestBlsSignatureProvider provider;
    const consensus::DoubleVoteEvidence evidence =
        evidenceFor(key, provider);

    core::ValidatorRegistry validators;
    const core::ValidatorRegistrationRecord registration(
        key.address().value(),
        key.publicKey(),
        1,
        "canonical-slashing-validator",
        kTimestamp
    );
    require(
        validators.registerValidator(registration).success(),
        "Test validator must register."
    );

    core::ValidatorSetHistory history;
    require(
        history.recordSet(1, validators),
        "Historical validator set must be recorded."
    );

    consensus::ValidatorPenaltyLedger ledger;
    const core::LedgerRecord record =
        node::CanonicalSlashingTransition::buildEvidenceRecord(
            evidence, kTimestamp + 3
        );
    const core::LedgerRecord restored =
        serialization::LedgerRecordCodec::deserialize(record.serialize());
    require(
        restored.serialize() == record.serialize(),
        "Slashing evidence ledger records must round-trip canonically."
    );
    node::CanonicalSlashingTransition::applyEvidenceRecords(
        {record},
        2,
        kTimestamp + 3,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        ledger,
        validators
    );

    require(
        ledger.size() == 1 && ledger.containsEvidence(evidence.evidenceId()),
        "Verified evidence must create one canonical penalty."
    );
    const core::ValidatorRegistryEntry* entry =
        validators.entryForAddress(key.address().value());
    require(
        entry != nullptr && entry->jailed(),
        "Double-voting validator must be jailed."
    );

    bool duplicateRejected = false;
    try {
        node::CanonicalSlashingTransition::applyEvidenceRecords(
            {record},
            3,
            kTimestamp + 4,
            history,
            crypto::CryptoPolicy::developmentPolicy(),
            provider,
            ledger,
            validators
        );
    } catch (const std::invalid_argument&) {
        duplicateRejected = true;
    }
    require(
        duplicateRejected,
        "The same evidence must not produce a second penalty."
    );
}

} // namespace

int main() {
    try {
        testAppliesVerifiedEvidenceOnce();
        std::cout << "Canonical slashing transition tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Canonical slashing transition tests FAILED: "
                  << error.what() << "\n";
        return 1;
    }
}
