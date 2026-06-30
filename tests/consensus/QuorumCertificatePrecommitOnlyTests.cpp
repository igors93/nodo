#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "economics/ValidationWorkRecord.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::consensus::QuorumCertificateBuilder;
using nodo::consensus::ValidatorVoteDecision;
using nodo::consensus::ValidatorVoteRecord;
using nodo::core::Block;
using nodo::core::LedgerRecord;
using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorRegistry;
using nodo::crypto::AddressDerivation;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::KeyPair;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

KeyPair keyPair(const std::string& suffix) {
    return KeyPair::createDeterministicBls12381KeyPair(
        "qc-precommit-only-validator-" + suffix
    );
}

std::string addressFor(const KeyPair& keyPair) {
    return AddressDerivation::deriveFromPublicKey(keyPair.publicKey()).value();
}

ValidatorRegistrationRecord registrationFor(const KeyPair& keyPair) {
    return ValidatorRegistrationRecord(
        addressFor(keyPair),
        keyPair.publicKey(),
        1,
        "qc-precommit-only-metadata-" + keyPair.publicKey().fingerprint().substr(0, 12),
        kTimestamp
    );
}

ValidatorRegistry registryWithThree() {
    ValidatorRegistry registry;
    requireCondition(registry.registerValidator(registrationFor(keyPair("a"))).accepted(), "register a");
    requireCondition(registry.registerValidator(registrationFor(keyPair("b"))).accepted(), "register b");
    requireCondition(registry.registerValidator(registrationFor(keyPair("c"))).accepted(), "register c");
    return registry;
}

Block candidateBlock() {
    const ValidationWorkRecord work(
        "validator-a",
        1,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "qc-precommit-only-target",
        "qc-precommit-only-evidence",
        1,
        kTimestamp
    );

    return Block(
        1,
        Block::createGenesisBlock({LedgerRecord::fromValidationWorkRecord(work, kTimestamp)}, kTimestamp).hash(),
        {LedgerRecord::fromValidationWorkRecord(work, kTimestamp)},
        kTimestamp + 10
    );
}

ValidatorVoteRecord voteFor(
    const KeyPair& keyPair,
    const Block& block,
    ValidatorVoteDecision decision,
    std::int64_t createdAt
) {
    const Bls12381SignatureProvider provider;
    return ValidatorVoteRecord::createVote(
        addressFor(keyPair),
        keyPair.publicKey(),
        keyPair.privateKeyForSigningOnly(),
        block.index(),
        block.hash(),
        block.previousHash(),
        1,
        decision,
        "qc-precommit-only-reason",
        createdAt,
        provider
    );
}

void testPrecommitVotesBuildQc() {
    const ValidatorRegistry registry = registryWithThree();
    const Block block = candidateBlock();
    const Bls12381SignatureProvider provider;

    const auto result = QuorumCertificateBuilder::buildFromVotes(
        block.index(),
        block.hash(),
        block.previousHash(),
        1,
        {
            voteFor(keyPair("a"), block, ValidatorVoteDecision::PRECOMMIT, kTimestamp + 20),
            voteFor(keyPair("b"), block, ValidatorVoteDecision::PRECOMMIT, kTimestamp + 21)
        },
        registry,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    requireCondition(result.certified(), "PRECOMMIT quorum should certify.");
    requireCondition(result.certificate().isStructurallyValid(), "PRECOMMIT QC should be valid.");
}

void testPrevoteVotesCannotBuildQc() {
    const ValidatorRegistry registry = registryWithThree();
    const Block block = candidateBlock();
    const Bls12381SignatureProvider provider;

    const auto result = QuorumCertificateBuilder::buildFromVotes(
        block.index(),
        block.hash(),
        block.previousHash(),
        1,
        {
            voteFor(keyPair("a"), block, ValidatorVoteDecision::PREVOTE, kTimestamp + 20),
            voteFor(keyPair("b"), block, ValidatorVoteDecision::PREVOTE, kTimestamp + 21)
        },
        registry,
        CryptoPolicy::developmentPolicy(),
        provider
    );

    requireCondition(!result.certified(), "PREVOTE votes must not certify a QC.");
    requireCondition(
        result.reason().find("Only PRECOMMIT") != std::string::npos,
        "QC rejection should explain that only PRECOMMIT votes are accepted."
    );
}

} // namespace

int main() {
    try {
        testPrecommitVotesBuildQc();
        testPrevoteVotesCannotBuildQc();
        std::cout << "Nodo quorum certificate PRECOMMIT-only tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo quorum certificate PRECOMMIT-only tests failed: "
                  << error.what() << "\n";
        return 1;
    }
}
