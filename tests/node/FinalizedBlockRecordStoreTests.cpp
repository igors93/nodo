#include "node/FinalizedBlockRecordStore.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SigningDomain.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace nodo;
using namespace nodo::node;

static const std::filesystem::path kTempDir =
    std::filesystem::temp_directory_path() / "nodo_finalized_record_store_test";

static void cleanup() {
    std::filesystem::remove_all(kTempDir);
}

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string registerValidator(
    core::ValidatorRegistry& registry,
    const crypto::KeyPair& kp,
    const std::string& seed
) {
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(kp.publicKey()).value();
    core::ValidatorRegistrationRecord rec(
        address, kp.publicKey(), 1, "meta-" + seed, 1900000000
    );
    if (!registry.registerValidator(rec).accepted()) {
        throw std::runtime_error("registerValidator failed: " + seed);
    }
    return address;
}

static consensus::FinalizedBlockRecord buildRecord(
    std::uint64_t height,
    const std::string& blockHash,
    const std::string& previousHash,
    const crypto::KeyPair& validatorKp,
    const core::ValidatorRegistry& registry
) {
    constexpr std::int64_t kTimestamp = 1900000000;
    constexpr std::uint64_t kRound = 1;

    const crypto::Bls12381SignatureProvider blsProvider;
    const crypto::CryptoPolicy policy = crypto::CryptoPolicy::developmentPolicy();
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(validatorKp.publicKey()).value();

    const consensus::ValidatorVoteRecord vote =
        consensus::ValidatorVoteRecord::createVote(
            address,
            validatorKp.publicKey(),
            validatorKp.privateKeyForSigningOnly(),
            height,
            blockHash,
            previousHash,
            kRound,
            consensus::ValidatorVoteDecision::PRECOMMIT,
            "reason-" + blockHash,
            kTimestamp,
            blsProvider
        );

    const consensus::QuorumCertificateBuildResult qcResult =
        consensus::QuorumCertificateBuilder::buildFromVotes(
            height,
            blockHash,
            previousHash,
            kRound,
            {vote},
            registry,
            policy,
            blsProvider
        );

    if (!qcResult.certified()) {
        throw std::runtime_error("QC build failed: " + qcResult.reason());
    }

    return consensus::FinalizedBlockRecord(
        height,
        blockHash,
        previousHash,
        kRound,
        kTimestamp,
        qcResult.certificate()
    );
}

// ── Tests ────────────────────────────────────────────────────────────────────

static void test_load_missing_returns_nullopt() {
    cleanup();
    FinalizedBlockRecordStore store(kTempDir);

    const auto result = store.load(1);
    assert(!result.has_value());
}

static void test_save_and_load_roundtrip() {
    cleanup();

    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("store-test-key");
    core::ValidatorRegistry registry;
    registerValidator(registry, kp, "store-test-key");

    const consensus::FinalizedBlockRecord record = buildRecord(
        42, "block-hash-42", "block-hash-41", kp, registry
    );

    FinalizedBlockRecordStore store(kTempDir);
    assert(store.save(record));
    assert(std::filesystem::exists(store.recordFilePath(42)));

    const auto loaded = store.load(42);
    assert(loaded.has_value());
    assert(loaded->blockIndex() == 42);
    assert(loaded->blockHash() == "block-hash-42");
    assert(loaded->previousHash() == "block-hash-41");
    assert(loaded->isStructurallyValid());
}

static void test_save_identical_is_idempotent() {
    cleanup();

    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("idem-key");
    core::ValidatorRegistry registry;
    registerValidator(registry, kp, "idem-key");

    const consensus::FinalizedBlockRecord record = buildRecord(
        5, "block-hash-5", "block-hash-4", kp, registry
    );

    FinalizedBlockRecordStore store(kTempDir);
    assert(store.save(record));
    // Second save with identical content must succeed.
    assert(store.save(record));

    const auto loaded = store.load(5);
    assert(loaded.has_value());
    assert(loaded->blockIndex() == 5);
}

static void test_load_all_empty_returns_empty_vector() {
    cleanup();
    FinalizedBlockRecordStore store(kTempDir);

    const auto records = store.loadAll();
    assert(records.empty());
}

static void test_load_all_returns_all_sorted_by_height() {
    cleanup();

    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("multi-key");
    core::ValidatorRegistry registry;
    registerValidator(registry, kp, "multi-key");

    FinalizedBlockRecordStore store(kTempDir);

    // Save out-of-order to verify sorting.
    const consensus::FinalizedBlockRecord r3 =
        buildRecord(3, "hash-3", "hash-2", kp, registry);
    const consensus::FinalizedBlockRecord r1 =
        buildRecord(1, "hash-1", "genesis-hash", kp, registry);
    const consensus::FinalizedBlockRecord r2 =
        buildRecord(2, "hash-2", "hash-1", kp, registry);

    assert(store.save(r3));
    assert(store.save(r1));
    assert(store.save(r2));

    const auto records = store.loadAll();
    assert(records.size() == 3U);
    assert(records[0].blockIndex() == 1);
    assert(records[1].blockIndex() == 2);
    assert(records[2].blockIndex() == 3);
}

static void test_load_corrupted_file_returns_nullopt() {
    cleanup();
    FinalizedBlockRecordStore store(kTempDir);

    // Write garbage directly — bypassing the store.
    const std::filesystem::path path = store.recordFilePath(7);
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream f(path);
        f << "this is not a valid FinalizedBlockRecord\n!!!garbage!!!\n";
    }

    const auto loaded = store.load(7);
    assert(!loaded.has_value());
}

static void test_record_file_path_uses_height_and_dot_qc_extension() {
    FinalizedBlockRecordStore store(kTempDir);

    const std::filesystem::path path = store.recordFilePath(100);
    assert(path.filename().string() == "100.qc");
    assert(path.parent_path().filename().string() == "qc");
}

static void test_load_all_skips_corrupted_files() {
    cleanup();

    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("skip-corrupt-key");
    core::ValidatorRegistry registry;
    registerValidator(registry, kp, "skip-corrupt-key");

    FinalizedBlockRecordStore store(kTempDir);

    const consensus::FinalizedBlockRecord r10 =
        buildRecord(10, "hash-10", "hash-9", kp, registry);
    assert(store.save(r10));

    // Plant a corrupted file at height 11.
    const std::filesystem::path badPath = store.recordFilePath(11);
    std::filesystem::create_directories(badPath.parent_path());
    {
        std::ofstream f(badPath);
        f << "garbage";
    }

    const auto records = store.loadAll();
    assert(records.size() == 1U);
    assert(records[0].blockIndex() == 10);
}

int main() {
    try {
        test_load_missing_returns_nullopt();
        test_save_and_load_roundtrip();
        test_save_identical_is_idempotent();
        test_load_all_empty_returns_empty_vector();
        test_load_all_returns_all_sorted_by_height();
        test_load_corrupted_file_returns_nullopt();
        test_record_file_path_uses_height_and_dot_qc_extension();
        test_load_all_skips_corrupted_files();

        cleanup();
        std::cout << "FinalizedBlockRecordStore tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        cleanup();
        return 1;
    }
}
