#include "node/PersistentSlashingEvidencePool.hpp"

#include "crypto/AddressDerivation.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/SignatureProvider.hpp"
#include "storage/AtomicFile.hpp"
#include "storage/SlashingEvidenceStore.hpp"

#include <cassert>
#include <filesystem>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kNow = 1900000000LL;

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

struct TestKey {
    crypto::PublicKey publicKey;
    crypto::PrivateKey privateKey;

    std::string address() const {
        return crypto::AddressDerivation::deriveFromPublicKey(publicKey).value();
    }
};

TestKey key(char marker) {
    return {
        crypto::PublicKey(
            crypto::CryptoAlgorithm::BLS12_381,
            std::string(96, marker)
        ),
        crypto::PrivateKey(
            crypto::CryptoAlgorithm::BLS12_381,
            std::string(64, static_cast<char>(marker + 1))
        )
    };
}

consensus::DoubleVoteEvidence evidenceFor(
    const TestKey& validator,
    const TestBlsSignatureProvider& provider
) {
    const auto vote = [&](const std::string& blockHash) {
        return consensus::ValidatorVoteRecord::createVote(
            validator.address(),
            validator.publicKey,
            validator.privateKey,
            1,
            blockHash,
            "previous-block",
            1,
            consensus::ValidatorVoteDecision::PRECOMMIT,
            "persistent-evidence-test",
            kNow - 2,
            provider
        );
    };
    return consensus::DoubleVoteEvidence(
        vote("block-a"), vote("block-b"), kNow - 1
    );
}

core::ValidatorSetHistory historyFor(const TestKey& validator) {
    core::ValidatorRegistry registry;
    assert(registry.registerValidator(
        core::ValidatorRegistrationRecord(
            validator.address(),
            validator.publicKey,
            1,
            "persistent-evidence-validator",
            kNow - 10
        )
    ).success());

    core::ValidatorSetHistory history;
    assert(history.recordSet(1, registry));
    return history;
}

std::filesystem::path testDirectory(const std::string& suffix) {
    return std::filesystem::temp_directory_path() /
        ("nodo-persistent-slashing-evidence-" + suffix);
}

void clean(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
}

void testRestoresVerifiedEvidenceAfterRestart() {
    const std::filesystem::path directory = testDirectory("restart");
    clean(directory);

    const TestBlsSignatureProvider provider;
    const TestKey validator = key('a');
    const consensus::DoubleVoteEvidence evidence =
        evidenceFor(validator, provider);
    const core::ValidatorSetHistory history = historyFor(validator);
    consensus::ValidatorPenaltyLedger penalties;
    storage::SlashingEvidenceStore store(directory);
    store.persist(evidence);

    consensus::EvidencePool firstProcessPool;
    const auto firstRestore = node::PersistentSlashingEvidencePool::restore(
        firstProcessPool,
        store,
        2,
        kNow,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        penalties
    );
    assert(firstRestore.success());
    assert(firstRestore.loadedEvidenceCount() == 1);
    assert(firstProcessPool.hasPersistence());
    assert(firstProcessPool.contains(evidence.evidenceId()));

    consensus::EvidencePool restartedPool;
    const auto restarted = node::PersistentSlashingEvidencePool::restore(
        restartedPool,
        store,
        2,
        kNow,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        penalties
    );
    assert(restarted.success());
    assert(restartedPool.contains(evidence.evidenceId()));
    assert(restartedPool.removeEvidence(evidence.evidenceId()));
    assert(store.count() == 0);

    clean(directory);
}

void testReconcilesEvidenceFinalizedBeforeCleanup() {
    const std::filesystem::path directory = testDirectory("finalized");
    clean(directory);

    const TestBlsSignatureProvider provider;
    const TestKey validator = key('a');
    const consensus::DoubleVoteEvidence evidence =
        evidenceFor(validator, provider);
    const core::ValidatorSetHistory history = historyFor(validator);
    storage::SlashingEvidenceStore store(directory);
    store.persist(evidence);

    consensus::ValidatorPenaltyLedger penalties;
    assert(penalties.applyEvidence(
        evidence.toRecord(),
        consensus::ValidatorPenaltyPolicy::conservativeTestnetPolicy(),
        kNow
    ).applied());

    consensus::EvidencePool pool;
    const auto restored = node::PersistentSlashingEvidencePool::restore(
        pool,
        store,
        2,
        kNow,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        penalties
    );
    assert(restored.success());
    assert(restored.loadedEvidenceCount() == 0);
    assert(restored.removedFinalizedEvidenceCount() == 1);
    assert(pool.size() == 0);
    assert(store.count() == 0);

    clean(directory);
}

void testRejectsCorruptedAndUnauthorizedEvidence() {
    const TestBlsSignatureProvider provider;
    const TestKey validator = key('a');
    const core::ValidatorSetHistory history = historyFor(validator);
    consensus::ValidatorPenaltyLedger penalties;

    const std::filesystem::path corruptedDirectory =
        testDirectory("corrupted");
    clean(corruptedDirectory);
    storage::AtomicFile::writeTextFile(
        corruptedDirectory / (std::string(64, 'f') + ".evidence"),
        "corrupted"
    );
    storage::SlashingEvidenceStore corruptedStore(corruptedDirectory);
    consensus::EvidencePool corruptedPool;
    const auto corrupted = node::PersistentSlashingEvidencePool::restore(
        corruptedPool,
        corruptedStore,
        2,
        kNow,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        penalties
    );
    assert(!corrupted.success());
    assert(
        corrupted.status() ==
        node::PersistentSlashingEvidencePoolLoadStatus::IO_ERROR
    );
    clean(corruptedDirectory);

    const std::filesystem::path unauthorizedDirectory =
        testDirectory("unauthorized");
    clean(unauthorizedDirectory);
    storage::SlashingEvidenceStore unauthorizedStore(unauthorizedDirectory);
    unauthorizedStore.persist(evidenceFor(key('d'), provider));
    consensus::EvidencePool unauthorizedPool;
    const auto unauthorized = node::PersistentSlashingEvidencePool::restore(
        unauthorizedPool,
        unauthorizedStore,
        2,
        kNow,
        history,
        crypto::CryptoPolicy::developmentPolicy(),
        provider,
        penalties
    );
    assert(!unauthorized.success());
    assert(
        unauthorized.status() ==
        node::PersistentSlashingEvidencePoolLoadStatus::INVALID_EVIDENCE
    );
    clean(unauthorizedDirectory);
}

} // namespace

int main() {
    testRestoresVerifiedEvidenceAfterRestart();
    testReconcilesEvidenceFinalizedBeforeCleanup();
    testRejectsCorruptedAndUnauthorizedEvidence();
    return 0;
}
