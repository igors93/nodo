#include "consensus/BlockFinalizer.hpp"
#include "consensus/ForkChoice.hpp"
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
using nodo::consensus::FinalizedBlockRecord;
using nodo::consensus::ForkChoiceDecision;
using nodo::consensus::ForkChoicePolicy;
using nodo::consensus::ForkChoiceRejectReason;
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

KeyPair keyPair(const std::string& suffix) {
    return KeyPair::createDeterministicBls12381KeyPair(
        "fork-choice-validator-key-" + suffix
    );
}

PublicKey publicKey(const std::string& suffix) {
    return keyPair(suffix).publicKey();
}

PrivateKey privateKey(const std::string& suffix) {
    return keyPair(suffix).privateKeyForSigningOnly();
}

std::string addressFor(const PublicKey& key) {
    return AddressDerivation::deriveFromPublicKey(key).value();
}

ValidatorRegistry validatorRegistryWithThree() {
    ValidatorRegistry registry;

    for (const std::string& suffix : {"a", "b", "c"}) {
        const PublicKey key = publicKey(suffix);

        requireCondition(
            registry.registerValidator(
                ValidatorRegistrationRecord(
                    addressFor(key),
                    key,
                    1,
                    "fork-choice-metadata-" + suffix,
                    kTimestamp + static_cast<std::int64_t>(suffix[0])
                )
            ).accepted(),
            "Validator registration should be accepted."
        );
    }

    return registry;
}

ValidationWorkRecord work(const std::string& evidence) {
    return ValidationWorkRecord(
        "fork-choice-validator",
        1,
        ValidationWorkType::VALIDATE_BLOCK,
        ValidationWorkResult::ACCEPTED,
        "fork-choice-target-" + evidence,
        evidence,
        1,
        kTimestamp
    );
}

Blockchain genesisChain() {
    const Block genesis =
        Block::createGenesisBlock(
            {
                LedgerRecord::fromValidationWorkRecord(
                    work("genesis"),
                    kTimestamp + 1
                )
            },
            kTimestamp + 2
        );

    Blockchain chain;
    chain.addGenesisBlock(genesis);
    return chain;
}

Block nextBlock(
    const Blockchain& chain,
    const std::string& evidence,
    std::int64_t timestamp
) {
    return Block(
        chain.size(),
        chain.latestBlock().hash(),
        {
            LedgerRecord::fromValidationWorkRecord(
                work(evidence),
                timestamp
            )
        },
        timestamp
    );
}

ValidatorVoteRecord voteFor(
    const std::string& suffix,
    const Block& block,
    std::int64_t timestamp
) {
    const PublicKey key = publicKey(suffix);

    const Bls12381SignatureProvider provider;

    return ValidatorVoteRecord::createVote(
        addressFor(key),
        key,
        privateKey(suffix),
        block.index(),
        block.hash(),
        block.previousHash(),
        1,
        ValidatorVoteDecision::PRECOMMIT,
        "NONE",
        timestamp,
        provider
    );
}

QuorumCertificate certificateFor(
    const Block& block,
    const ValidatorRegistry& registry
) {
    const std::vector<ValidatorVoteRecord> votes = {
        voteFor("a", block, kTimestamp + 10),
        voteFor("b", block, kTimestamp + 11)
    };

    const Bls12381SignatureProvider provider;

    const auto result =
        QuorumCertificateBuilder::buildFromVotes(
            block.index(),
            block.hash(),
            block.previousHash(),
            1,
            votes,
            registry,
            CryptoPolicy::developmentPolicy(),
            provider
        );

    requireCondition(result.certified(), "QC fixture should certify.");
    return result.certificate();
}

void finalizeInRegistryOnly(
    BlockFinalizationRegistry& registry,
    const Block& block,
    const ValidatorRegistry& validatorRegistry,
    std::int64_t finalizedAt
) {
    const QuorumCertificate certificate =
        certificateFor(block, validatorRegistry);

    const FinalizedBlockRecord record(
        block.index(),
        block.hash(),
        block.previousHash(),
        certificate.round(),
        finalizedAt,
        certificate
    );

    const Bls12381SignatureProvider provider;

    requireCondition(
        record.verify(
            validatorRegistry,
            CryptoPolicy::developmentPolicy(),
            provider
        ),
        "Finalized record fixture should verify."
    );

    requireCondition(
        registry.registerFinalizedBlock(record).registered(),
        "Finalized checkpoint should register."
    );
}

void testAdoptsLongerCandidateWithoutFinalityConflict() {
    Blockchain local = genesisChain();

    Blockchain candidate = genesisChain();
    candidate.addBlock(nextBlock(candidate, "candidate-1", kTimestamp + 20));
    candidate.addBlock(nextBlock(candidate, "candidate-2", kTimestamp + 21));

    BlockFinalizationRegistry localRegistry;
    BlockFinalizationRegistry candidateRegistry;

    const auto result =
        ForkChoicePolicy::chooseBestChain(
            local,
            localRegistry,
            candidate,
            candidateRegistry
        );

    requireCondition(
        result.decision() == ForkChoiceDecision::ADOPT_CANDIDATE,
        "Longer candidate should be adopted when no finality conflict exists."
    );
}

void testRejectsCandidateConflictingWithLocalFinality() {
    const ValidatorRegistry validatorRegistry =
        validatorRegistryWithThree();

    Blockchain local = genesisChain();
    const Block finalizedLocalBlock =
        nextBlock(local, "local-final", kTimestamp + 30);
    local.addBlock(finalizedLocalBlock);

    BlockFinalizationRegistry localRegistry;
    finalizeInRegistryOnly(
        localRegistry,
        finalizedLocalBlock,
        validatorRegistry,
        kTimestamp + 40
    );

    Blockchain candidate = genesisChain();
    candidate.addBlock(nextBlock(candidate, "candidate-conflict", kTimestamp + 31));
    candidate.addBlock(nextBlock(candidate, "candidate-extension", kTimestamp + 32));

    BlockFinalizationRegistry candidateRegistry;

    const auto result =
        ForkChoicePolicy::chooseBestChain(
            local,
            localRegistry,
            candidate,
            candidateRegistry
        );

    requireCondition(
        result.rejectReason() == ForkChoiceRejectReason::CANDIDATE_CONFLICTS_WITH_LOCAL_FINALITY,
        "Candidate conflicting with local finality must be rejected."
    );
}

void testAdoptsCandidateWithHigherFinalizedCheckpoint() {
    const ValidatorRegistry validatorRegistry =
        validatorRegistryWithThree();

    Blockchain local = genesisChain();
    const Block sharedFinal =
        nextBlock(local, "shared-final", kTimestamp + 50);
    local.addBlock(sharedFinal);

    BlockFinalizationRegistry localRegistry;
    finalizeInRegistryOnly(
        localRegistry,
        sharedFinal,
        validatorRegistry,
        kTimestamp + 60
    );

    Blockchain candidate = genesisChain();
    candidate.addBlock(sharedFinal);
    const Block candidateSecond =
        nextBlock(candidate, "candidate-second-final", kTimestamp + 51);
    candidate.addBlock(candidateSecond);

    BlockFinalizationRegistry candidateRegistry;
    finalizeInRegistryOnly(
        candidateRegistry,
        candidateSecond,
        validatorRegistry,
        kTimestamp + 61
    );

    const auto result =
        ForkChoicePolicy::chooseBestChain(
            local,
            localRegistry,
            candidate,
            candidateRegistry
        );

    requireCondition(
        result.shouldAdoptCandidate(),
        "Candidate with higher finalized checkpoint should be adopted."
    );
}

void testSummarizesChainWithCheckpoint() {
    const ValidatorRegistry validatorRegistry =
        validatorRegistryWithThree();

    Blockchain chain = genesisChain();
    const Block block =
        nextBlock(chain, "summary-final", kTimestamp + 70);
    chain.addBlock(block);

    BlockFinalizationRegistry registry;
    finalizeInRegistryOnly(
        registry,
        block,
        validatorRegistry,
        kTimestamp + 80
    );

    const auto summary =
        ForkChoicePolicy::summarizeChain(
            chain,
            registry
        );

    requireCondition(
        summary.isValid() &&
        summary.hasFinalizedCheckpoint() &&
        summary.finalizedCheckpoint().blockHash() == block.hash(),
        "Summary should include highest finalized checkpoint."
    );
}

} // namespace

int main() {
    try {
        testAdoptsLongerCandidateWithoutFinalityConflict();
        testRejectsCandidateConflictingWithLocalFinality();
        testAdoptsCandidateWithHigherFinalizedCheckpoint();
        testSummarizesChainWithCheckpoint();

        std::cout << "Nodo fork choice tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo fork choice tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
