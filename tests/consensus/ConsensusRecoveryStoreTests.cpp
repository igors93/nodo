#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ConsensusRoundManager.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "storage/AtomicFile.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

nodo::consensus::ValidatorVoteRecord makeVote(
    nodo::consensus::ValidatorVoteDecision decision,
    std::uint64_t height,
    std::uint64_t round,
    const std::string& blockHash,
    const std::string& previousHash,
    std::int64_t createdAt
) {
    static const nodo::crypto::Bls12381SignatureProvider provider;
    const nodo::crypto::KeyPair keyPair =
        nodo::crypto::KeyPair::createDeterministicBls12381KeyPair(
            "consensus-recovery-store-validator"
        );
    const nodo::crypto::Signer signer(keyPair, provider);
    return signer.signValidatorVote(
        height,
        blockHash,
        previousHash,
        round,
        decision,
        decision == nodo::consensus::ValidatorVoteDecision::PREVOTE
            ? "recovery-prevote"
            : "recovery-precommit",
        createdAt
    );
}

} // namespace

int main() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path()
        / "nodo-consensus-recovery-store-test.state";

    std::error_code cleanupError;
    std::filesystem::remove(path, cleanupError);
    std::filesystem::remove(path.string() + ".tmp", cleanupError);

    // Test 1: basic round state round-trips correctly.
    {
        const nodo::consensus::ConsensusRoundState state(
            42,
            3,
            "validator-a",
            1900000000
        );

        assert(nodo::consensus::ConsensusRecoveryStore::save(path, state));

        const auto loaded =
            nodo::consensus::ConsensusRecoveryStore::load(path);
        assert(loaded.has_value());
        assert(loaded->height() == 42);
        assert(loaded->round() == 3);
        assert(loaded->proposerAddress() == "validator-a");
        assert(loaded->lockedBlockHash() == "");
        assert(loaded->lockedRound() == 0);
        assert(loaded->votedPrevote() == false);
        assert(loaded->votedPrecommit() == false);
    }

    // Test 2: lock/vote fields serialize and deserialize correctly.
    {
        const auto prevote = makeVote(
            nodo::consensus::ValidatorVoteDecision::PREVOTE,
            10,
            2,
            "abc123blockhashhex",
            "prevhash10",
            1900000001
        );
        const nodo::consensus::ConsensusRoundState stateWithLock(
            10,
            2,
            "validator-b",
            1900000001,
            "abc123blockhashhex",
            2,
            true,
            false,
            prevote,
            std::nullopt
        );

        assert(nodo::consensus::ConsensusRecoveryStore::save(path, stateWithLock));

        const auto loaded =
            nodo::consensus::ConsensusRecoveryStore::load(path);
        assert(loaded.has_value());
        assert(loaded->height() == 10);
        assert(loaded->round() == 2);
        assert(loaded->proposerAddress() == "validator-b");
        assert(loaded->lockedBlockHash() == "abc123blockhashhex");
        assert(loaded->lockedRound() == 2);
        assert(loaded->votedPrevote() == true);
        assert(loaded->votedPrecommit() == false);
        assert(loaded->persistedPrevote().has_value());
        assert(!loaded->persistedPrecommit().has_value());
        assert(loaded->persistedPrevote()->serialize() == prevote.serialize());
    }

    // Test 3: both voted flags true.
    {
        const auto prevote = makeVote(
            nodo::consensus::ValidatorVoteDecision::PREVOTE,
            77,
            1,
            "deadbeef01020304",
            "prevhash77",
            1900000002
        );
        const auto precommit = makeVote(
            nodo::consensus::ValidatorVoteDecision::PRECOMMIT,
            77,
            1,
            "deadbeef01020304",
            "prevhash77",
            1900000003
        );
        const nodo::consensus::ConsensusRoundState statePrecommitted(
            77,
            1,
            "validator-c",
            1900000002,
            "deadbeef01020304",
            1,
            true,
            true,
            prevote,
            precommit
        );

        assert(nodo::consensus::ConsensusRecoveryStore::save(path, statePrecommitted));

        const auto loaded =
            nodo::consensus::ConsensusRecoveryStore::load(path);
        assert(loaded.has_value());
        assert(loaded->votedPrevote() == true);
        assert(loaded->votedPrecommit() == true);
        assert(loaded->lockedBlockHash() == "deadbeef01020304");
        assert(loaded->lockedRound() == 1);
        assert(loaded->persistedPrevote().has_value());
        assert(loaded->persistedPrecommit().has_value());
        assert(loaded->persistedPrecommit()->serialize() == precommit.serialize());
    }

    // Test 4: file with unexpected extra field is rejected.
    {
        {
            std::ofstream out(path, std::ios::binary | std::ios::app);
            out << "unexpected=field\n";
        }

        assert(!nodo::consensus::ConsensusRecoveryStore::load(path).has_value());
    }

    // Test 5: remove and confirm gone.
    assert(nodo::consensus::ConsensusRecoveryStore::remove(path));
    assert(!nodo::consensus::ConsensusRecoveryStore::load(path).has_value());

    // Test 6: atomic write — no temporary file is left behind after a
    // successful save.  AtomicFile renames .tmp.* → final before returning,
    // so no leftover with the temporary-file naming convention should exist.
    // Uses an isolated subdirectory so that unrelated system temp files in
    // the OS temp directory do not cause false failures on Windows.
    {
        const std::filesystem::path isolatedDir =
            std::filesystem::temp_directory_path()
            / "nodo-consensus-recovery-atomic-test";
        std::filesystem::create_directories(isolatedDir);
        const std::filesystem::path isolatedPath =
            isolatedDir / "recovery.state";

        const nodo::consensus::ConsensusRoundState state(
            99, 1, "validator-d", 1900000010
        );
        assert(nodo::consensus::ConsensusRecoveryStore::save(isolatedPath, state));
        assert(std::filesystem::exists(isolatedPath));

        const auto temporaryFiles =
            nodo::storage::AtomicFile::listTemporaryWriteFiles(isolatedDir);
        assert(temporaryFiles.empty());

        std::error_code cleanupError2;
        std::filesystem::remove_all(isolatedDir, cleanupError2);
    }

    // Test 7: serialisation round-trip produces identical content on repeated
    // saves (determinism requirement).
    {
        const auto prevote = makeVote(
            nodo::consensus::ValidatorVoteDecision::PREVOTE,
            55,
            2,
            "deadbeef",
            "prevhash55",
            1900000020
        );
        const auto precommit = makeVote(
            nodo::consensus::ValidatorVoteDecision::PRECOMMIT,
            55,
            2,
            "deadbeef",
            "prevhash55",
            1900000021
        );
        const nodo::consensus::ConsensusRoundState state(
            55, 2, "validator-e", 1900000020, "deadbeef", 2, true, true,
            prevote, precommit
        );
        assert(nodo::consensus::ConsensusRecoveryStore::save(path, state));
        const auto first = nodo::consensus::ConsensusRecoveryStore::load(path);
        assert(nodo::consensus::ConsensusRecoveryStore::save(path, state));
        const auto second = nodo::consensus::ConsensusRecoveryStore::load(path);

        assert(first.has_value() && second.has_value());
        assert(first->height()          == second->height());
        assert(first->round()           == second->round());
        assert(first->lockedBlockHash() == second->lockedBlockHash());
        assert(first->votedPrevote()    == second->votedPrevote());
        assert(first->votedPrecommit()  == second->votedPrecommit());

        std::error_code cleanupError3;
        std::filesystem::remove(path, cleanupError3);
    }

    // Test 8: flags cannot claim a vote without the signed vote material.
    {
        const nodo::consensus::ConsensusRoundState unsafeState(
            60, 1, "validator-f", 1900000030, "", 0, true, false
        );
        assert(!nodo::consensus::ConsensusRecoveryStore::save(path, unsafeState));
    }

    return 0;
}
