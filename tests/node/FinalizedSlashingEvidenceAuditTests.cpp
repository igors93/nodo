#include "node/FinalizedSlashingEvidenceAudit.hpp"
#include "node/CanonicalSlashingTransition.hpp"
#include "node/StakingRegistry.hpp"

#include "crypto/CryptoSuiteId.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureProvider.hpp"
#include "economics/StakeAccount.hpp"

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
            "audit-test",
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

void registerValidator(
    const crypto::KeyPair& key,
    core::ValidatorRegistry& validators,
    node::StakingRegistry& staking
) {
    require(
        validators.registerValidator(
            core::ValidatorRegistrationRecord(
                key.address().value(),
                key.publicKey(),
                1,
                "finalized-slashing-audit-validator",
                kTimestamp
            )
        ).success(),
        "Test validator must register."
    );
    staking.setAccount(
        key.address().value(),
        economics::StakeAccount(
            key.address().value(),
            utils::Amount::fromRawUnits(
                static_cast<std::int64_t>(
                    core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS
                )
            )
        )
    );
}

void testFinalizedEvidenceMustHavePenaltyAndDomainEffects() {
    const crypto::KeyPair key = validatorKey();
    const TestBlsSignatureProvider provider;
    const consensus::DoubleVoteEvidence evidence = evidenceFor(key, provider);
    const core::LedgerRecord record =
        node::CanonicalSlashingTransition::buildEvidenceRecord(
            evidence,
            kTimestamp + 3
        );
    const core::Block block(
        2,
        "previous-finalized-block",
        {record},
        kTimestamp + 3,
        std::string(64, 'a'),
        std::string(64, 'b')
    );

    core::ValidatorRegistry validators;
    node::StakingRegistry staking;
    registerValidator(key, validators, staking);

    core::ValidatorSetHistory history;
    require(history.recordSet(1, validators), "Historical set must be recorded.");

    consensus::ValidatorPenaltyLedger ledger;
    node::CanonicalSlashingTransition::applyEvidenceRecords(
        {record},
        block.index(),
        block.timestamp(),
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        ledger,
        validators,
        staking,
        "chain-localnet"
    );

    const node::FinalizedSlashingEvidenceAuditResult audit =
        node::FinalizedSlashingEvidenceAudit::auditBlockEffects(
            block,
            ledger,
            validators,
            staking
        );
    require(
        audit.passed() && audit.evidenceCount() == 1,
        "Finalized evidence must audit after penalty, registry and staking effects."
    );
}

void testFinalizedEvidenceWithoutPenaltyIsRejected() {
    const crypto::KeyPair key = validatorKey();
    const TestBlsSignatureProvider provider;
    const consensus::DoubleVoteEvidence evidence = evidenceFor(key, provider);
    const core::LedgerRecord record =
        node::CanonicalSlashingTransition::buildEvidenceRecord(
            evidence,
            kTimestamp + 3
        );
    const core::Block block(
        2,
        "previous-finalized-block",
        {record},
        kTimestamp + 3,
        std::string(64, 'a'),
        std::string(64, 'b')
    );

    core::ValidatorRegistry validators;
    node::StakingRegistry staking;
    registerValidator(key, validators, staking);
    consensus::ValidatorPenaltyLedger emptyLedger;

    const node::FinalizedSlashingEvidenceAuditResult audit =
        node::FinalizedSlashingEvidenceAudit::auditBlockEffects(
            block,
            emptyLedger,
            validators,
            staking
        );
    require(
        !audit.passed(),
        "A finalized block with evidence but no penalty decision must be rejected."
    );
}

} // namespace

int main() {
    try {
        testFinalizedEvidenceMustHavePenaltyAndDomainEffects();
        testFinalizedEvidenceWithoutPenaltyIsRejected();
        std::cout << "Finalized slashing evidence audit tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Finalized slashing evidence audit tests FAILED: "
                  << error.what() << "\n";
        return 1;
    }
}
