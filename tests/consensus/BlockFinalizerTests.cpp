#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "economics/ValidationWorkRecord.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::consensus::BlockFinalizationRegistry;
using nodo::consensus::BlockFinalizationStatus;
using nodo::consensus::BlockFinalizer;
using nodo::consensus::QuorumCertificate;
using nodo::consensus::QuorumCertificateBuilder;
using nodo::consensus::ValidatorVoteDecision;
using nodo::consensus::ValidatorVoteRecord;
using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::LedgerRecord;
using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorRegistry;
using nodo::crypto::AddressDerivation;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::KeyPair;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::economics::ValidationWorkRecord;
using nodo::economics::ValidationWorkResult;
using nodo::economics::ValidationWorkType;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

KeyPair keyPair(
    const std::string& suffix
) {
    return KeyPair::createDeterministicBls12381KeyPair(
        "block-finalizer-validator-key-" + suffix
    );
}

PublicKey publicKey(
    const std::string& suffix
) {
    return keyPair(suffix).publicKey();
}

PrivateKey privateKey(
    const std::string& suffix
) {
    return keyPair(suffix).privateKeyForSigningOnly();
}

std::string addressFor(
    const PublicKey& key
) {
    return AddressDerivation::deriveFromPublicKey(key).value();
}

ValidatorRegistrationRecord registrationFor(
    const PublicKey& key,
    std::int64_t timestamp
) {
    return ValidatorRegistrationRecord(
        addressFor(key),
        key,
        1,
        "block-finalizer-metadata-" + key.fingerprint().substr(0, 12),
        timestamp
    );
}

ValidatorRegistry validatorRegistryWithThree() {
    ValidatorRegistry registry;

    requireCondition(
        registry.registerValidator(registrationFor(publicKey("a"), kTimestamp + 1)).accepted(),
        "Validator A should register."
    );

    requireCondition(
        registry.registerValidator(registrationFor(publicKey("b"), kTimestamp + 2)).accepted(),
        "Validator B should register."
    );

    requireCondition(
        registry.registerValidator(registrationFor(publicKey("c"), kTimestamp + 3)).accepted(),
        "Validator C should register."
    );

    return registry;
}

ValidationWorkRecord validationWork(
    const std::string& validator,
    const std::string& evidence
) {
    return ValidationWorkRecord(
        validator,
        1,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "block-finalizer-target-" + evidence,
        evidence,
        1,
        kTimestamp
    );
}

Blockchain blockchainWithGenesis() {
    const Block genesis =
        Block::createGenesisBlock(
            {
                LedgerRecord::fromValidationWorkRecord(
                    validationWork("bootstrap", "genesis"),
                    kTimestamp + 4
                )
            },
            kTimestamp + 5
        );

    Blockchain blockchain;
    blockchain.addGenesisBlock(genesis);

    return blockchain;
}

Block candidateBlock(
    const Blockchain& blockchain,
    const std::string& evidence,
    std::int64_t timestamp
) {
    return Block(
        blockchain.size(),
        blockchain.latestBlock().hash(),
        {
            LedgerRecord::fromValidationWorkRecord(
                validationWork("validator-work", evidence),
                timestamp
            )
        },
        timestamp
    );
}

ValidatorVoteRecord precommitVoteFor(
    const std::string& suffix,
    const Block& block,
    std::uint64_t round,
    std::int64_t timestamp
) {
    const PublicKey key =
        publicKey(suffix);

    const Bls12381SignatureProvider provider;

    return ValidatorVoteRecord::createVote(
        addressFor(key),
        key,
        privateKey(suffix),
        block.index(),
        block.hash(),
        block.previousHash(),
        round,
        ValidatorVoteDecision::PRECOMMIT,
        "NONE",
        timestamp,
        provider
    );
}

QuorumCertificate certificateFor(
    const Block& block,
    const ValidatorRegistry& registry,
    std::uint64_t round = 1
) {
    const std::vector<ValidatorVoteRecord> votes = {
        precommitVoteFor("a", block, round, kTimestamp + 20),
        precommitVoteFor("b", block, round, kTimestamp + 21)
    };

    const Bls12381SignatureProvider provider;

    const auto result =
        QuorumCertificateBuilder::buildFromVotes(
            block.index(),
            block.hash(),
            block.previousHash(),
            round,
            votes,
            registry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(
        result.certified(),
        "Quorum certificate fixture should certify."
    );

    return result.certificate();
}

void testFinalizesBlockWithValidCertificate() {
    Blockchain blockchain =
        blockchainWithGenesis();

    const ValidatorRegistry registry =
        validatorRegistryWithThree();

    BlockFinalizationRegistry finalizationRegistry;

    const Block block =
        candidateBlock(
            blockchain,
            "finalize-valid",
            kTimestamp + 30
        );

    const QuorumCertificate certificate =
        certificateFor(
            block,
            registry
        );

    const Bls12381SignatureProvider provider;

    const auto result =
        BlockFinalizer::finalizeBlock(
            blockchain,
            block,
            certificate,
            registry,
            finalizationRegistry,
            CryptoPolicy::developmentPolicy(),
            provider,
            kTimestamp + 40
        );

    requireCondition(
        result.finalized(),
        "Valid block with valid quorum certificate should finalize."
    );

    requireCondition(
        blockchain.size() == 2U,
        "Finalization should append block to blockchain."
    );

    requireCondition(
        finalizationRegistry.isFinalizedBlock(
            block.index(),
            block.hash()
        ),
        "Finalized block should be registered."
    );
}

void testRejectsCertificateForDifferentBlock() {
    Blockchain blockchain =
        blockchainWithGenesis();

    const ValidatorRegistry registry =
        validatorRegistryWithThree();

    BlockFinalizationRegistry finalizationRegistry;

    const Block blockA =
        candidateBlock(
            blockchain,
            "block-a",
            kTimestamp + 50
        );

    const Block blockB =
        candidateBlock(
            blockchain,
            "block-b",
            kTimestamp + 51
        );

    const QuorumCertificate certificateForA =
        certificateFor(
            blockA,
            registry
        );

    const Bls12381SignatureProvider provider;

    const auto result =
        BlockFinalizer::finalizeBlock(
            blockchain,
            blockB,
            certificateForA,
            registry,
            finalizationRegistry,
            CryptoPolicy::developmentPolicy(),
            provider,
            kTimestamp + 60
        );

    requireCondition(
        result.status() == BlockFinalizationStatus::CERTIFICATE_BLOCK_MISMATCH ||
        result.status() == BlockFinalizationStatus::INVALID_CERTIFICATE,
        "Certificate for another block must be rejected."
    );

    requireCondition(
        blockchain.size() == 1U,
        "Rejected finalization must not append block."
    );
}

void testRejectsConflictingFinalizationAtSameHeight() {
    Blockchain blockchain =
        blockchainWithGenesis();

    const ValidatorRegistry registry =
        validatorRegistryWithThree();

    BlockFinalizationRegistry finalizationRegistry;

    const Block blockA =
        candidateBlock(
            blockchain,
            "first-block",
            kTimestamp + 70
        );

    const QuorumCertificate certificateA =
        certificateFor(
            blockA,
            registry
        );

    const Bls12381SignatureProvider provider;

    requireCondition(
        BlockFinalizer::finalizeBlock(
            blockchain,
            blockA,
            certificateA,
            registry,
            finalizationRegistry,
            CryptoPolicy::developmentPolicy(),
            provider,
            kTimestamp + 80
        ).finalized(),
        "First block should finalize."
    );

    /*
     * Create a different block for the same height by using a copy of the chain
     * before the first finalization.
     */
    Blockchain forkBase =
        blockchainWithGenesis();

    const Block conflictingBlock =
        candidateBlock(
            forkBase,
            "conflicting-block",
            kTimestamp + 71
        );

    const QuorumCertificate conflictingCertificate =
        certificateFor(
            conflictingBlock,
            registry
        );

    const auto result =
        BlockFinalizer::finalizeBlock(
            forkBase,
            conflictingBlock,
            conflictingCertificate,
            registry,
            finalizationRegistry,
            CryptoPolicy::developmentPolicy(),
            provider,
            kTimestamp + 90
        );

    requireCondition(
        result.status() == BlockFinalizationStatus::ALREADY_FINALIZED_CONFLICT,
        "Conflicting finalization at same height should be rejected."
    );
}

void testRejectsBlockForWrongTip() {
    Blockchain original =
        blockchainWithGenesis();

    Blockchain differentChain =
        blockchainWithGenesis();

    const ValidatorRegistry registry =
        validatorRegistryWithThree();

    BlockFinalizationRegistry finalizationRegistry;

    const Block blockForOriginal =
        candidateBlock(
            original,
            "original-tip",
            kTimestamp + 100
        );

    const QuorumCertificate certificate =
        certificateFor(
            blockForOriginal,
            registry
        );

    /*
     * Move differentChain tip so blockForOriginal no longer points to it.
     */
    differentChain.addBlock(
        candidateBlock(
            differentChain,
            "different-chain-first",
            kTimestamp + 101
        )
    );

    const Bls12381SignatureProvider provider;

    const auto result =
        BlockFinalizer::finalizeBlock(
            differentChain,
            blockForOriginal,
            certificate,
            registry,
            finalizationRegistry,
            CryptoPolicy::developmentPolicy(),
            provider,
            kTimestamp + 110
        );

    requireCondition(
        result.status() == BlockFinalizationStatus::APPEND_REJECTED,
        "Block for wrong chain tip should be rejected."
    );
}

} // namespace

int main() {
    try {
        testFinalizesBlockWithValidCertificate();
        testRejectsCertificateForDifferentBlock();
        testRejectsConflictingFinalizationAtSameHeight();
        testRejectsBlockForWrongTip();

        std::cout << "Nodo block finalizer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo block finalizer tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
