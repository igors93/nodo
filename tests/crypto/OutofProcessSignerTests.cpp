#include "crypto/OutofProcessSigner.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoSuiteId.hpp"
#include "crypto/KeyEncryptionService.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signature.hpp"
#include "crypto/SigningDomain.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::OutofProcessSigner;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoSuiteId;
using nodo::crypto::KeyPair;
using nodo::crypto::KeyEncryptionService;
using nodo::crypto::Signature;
using nodo::crypto::SignatureRequest;
using nodo::crypto::SigningDomain;

void requireCondition(bool condition, const std::string& failureMessage) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

std::filesystem::path uniqueStatePath(
    const std::string& name
) {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch();

    return std::filesystem::temp_directory_path() /
        (
            "nodo-oop-signer-" +
            name +
            "-" +
            std::to_string(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()
            ) +
            ".state"
        );
}

std::string encryptedValidatorPrivateKey(
    const std::string& keyId,
    const KeyPair& keyPair,
    const std::string& password
) {
    const std::string envelope =
        KeyEncryptionService::encrypt(
            keyId,
            keyPair.privateKeyForSigningOnly().keyMaterialForSigningOnly(),
            password
        );
    requireCondition(!envelope.empty(), "Should encrypt private key content");
    return envelope;
}

bool verifySignature(
    const OutofProcessSigner& signer,
    const SignatureRequest& request,
    const std::string& signatureHex,
    SigningDomain domain
) {
    const Bls12381SignatureProvider provider;
    const Signature signature(
        CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        domain,
        CryptoAlgorithm::BLS12_381,
        signer.validatorPublicKey(),
        signatureHex,
        1
    );

    return provider.verify(
        request.payloadToSign,
        signature
    ).success();
}

void testSignerInitializationAndRealSignatures() {
    const std::string keyId = "val-key-1";
    const std::string password = "strong-password-99";
    const KeyPair keyPair =
        KeyPair::createDeterministicBls12381KeyPair("oop-signer-real-signatures");
    const std::string envelope =
        encryptedValidatorPrivateKey(keyId, keyPair, password);

    OutofProcessSigner signer(keyId, envelope, password, {}, true);
    requireCondition(
        signer.validatorAddress() == keyPair.address().value(),
        "Validator address should be derived from the decrypted validator public key"
    );

    SignatureRequest req1{10, 0, "hashA", "block-payload-A"};
    std::string sig1;
    requireCondition(signer.signBlockProposal(req1, sig1), "Should sign first proposal");
    requireCondition(!sig1.empty(), "Signature should not be empty");
    requireCondition(
        sig1 != "sig_proposal_hashA_val-key-1",
        "Proposal signature must not be predictable string concatenation"
    );
    requireCondition(
        verifySignature(
            signer,
            req1,
            sig1,
            SigningDomain::VALIDATOR_BLOCK_PROPOSAL
        ),
        "Proposal signature should verify with signer public key"
    );

    SignatureRequest req2{10, 1, "hashB", "block-payload-B"};
    std::string sig2;
    requireCondition(signer.signBlockProposal(req2, sig2), "Should sign higher-round proposal");
    requireCondition(
        sig1 != sig2,
        "Different proposal payloads should produce different signatures"
    );
    requireCondition(
        verifySignature(
            signer,
            req2,
            sig2,
            SigningDomain::VALIDATOR_BLOCK_PROPOSAL
        ),
        "Second proposal signature should verify"
    );

    SignatureRequest voteReq{11, 0, "voteHashA", "vote-payload-A"};
    std::string voteSig;
    requireCondition(signer.signVote(voteReq, voteSig), "Should sign first vote");
    requireCondition(
        voteSig != "sig_vote_voteHashA_val-key-1",
        "Vote signature must not be predictable string concatenation"
    );
    requireCondition(
        verifySignature(
            signer,
            voteReq,
            voteSig,
            SigningDomain::VALIDATOR_VOTE
        ),
        "Vote signature should verify with signer public key"
    );
}

void testSignerBlocksDoubleSignInMemory() {
    const std::string keyId = "val-key-memory";
    const std::string password = "strong-password-99";
    const KeyPair keyPair =
        KeyPair::createDeterministicBls12381KeyPair("oop-signer-memory");
    const std::string envelope =
        encryptedValidatorPrivateKey(keyId, keyPair, password);

    OutofProcessSigner signer(keyId, envelope, password, {}, true);

    SignatureRequest req1{20, 0, "hashA", "block-payload-A"};
    std::string sig1;
    requireCondition(signer.signBlockProposal(req1, sig1), "Should sign first proposal");

    std::string sig1_retry;
    requireCondition(signer.signBlockProposal(req1, sig1_retry), "Should succeed on identical retry");
    requireCondition(sig1 == sig1_retry, "Retry signature should be identical");

    SignatureRequest doubleSignReq{20, 0, "hashB", "block-payload-B"};
    std::string sigDouble;
    requireCondition(!signer.signBlockProposal(doubleSignReq, sigDouble), "Signer should block double-signing request!");

    SignatureRequest higherReq{21, 0, "hashC", "block-payload-C"};
    std::string sigHigher;
    requireCondition(signer.signBlockProposal(higherReq, sigHigher), "Should sign higher block height");
}

void testSignerPersistsDoubleSignProtectionAcrossRestart() {
    const std::string keyId = "val-key-persist";
    const std::string password = "strong-password-99";
    const KeyPair keyPair =
        KeyPair::createDeterministicBls12381KeyPair("oop-signer-persistent");
    const std::string envelope =
        encryptedValidatorPrivateKey(keyId, keyPair, password);
    const std::filesystem::path statePath =
        uniqueStatePath("persist");

    std::filesystem::remove(statePath);

    SignatureRequest proposalReq{30, 2, "proposalHashA", "proposal-payload-A"};
    SignatureRequest voteReq{31, 1, "voteHashA", "vote-payload-A"};
    std::string proposalSignature;
    std::string voteSignature;

    {
        OutofProcessSigner signer(keyId, envelope, password, statePath);
        requireCondition(
            signer.signBlockProposal(proposalReq, proposalSignature),
            "Initial signer should sign proposal"
        );
        requireCondition(
            signer.signVote(voteReq, voteSignature),
            "Initial signer should sign vote"
        );
    }

    OutofProcessSigner restartedSigner(keyId, envelope, password, statePath);

    SignatureRequest conflictingProposal{30, 2, "proposalHashB", "proposal-payload-B"};
    std::string conflictingSignature;
    requireCondition(
        !restartedSigner.signBlockProposal(conflictingProposal, conflictingSignature),
        "Restarted signer should reject conflicting proposal at same height/round"
    );

    SignatureRequest conflictingVote{31, 1, "voteHashB", "vote-payload-B"};
    requireCondition(
        !restartedSigner.signVote(conflictingVote, conflictingSignature),
        "Restarted signer should reject conflicting vote at same height/round"
    );

    std::string proposalRetry;
    requireCondition(
        restartedSigner.signBlockProposal(proposalReq, proposalRetry),
        "Restarted signer should allow identical proposal retry"
    );
    requireCondition(
        proposalRetry == proposalSignature,
        "Identical proposal retry should reproduce the same signature"
    );

    std::string voteRetry;
    requireCondition(
        restartedSigner.signVote(voteReq, voteRetry),
        "Restarted signer should allow identical vote retry"
    );
    requireCondition(
        voteRetry == voteSignature,
        "Identical vote retry should reproduce the same signature"
    );

    std::filesystem::remove(statePath);
}

} // namespace

int main() {
    try {
        testSignerInitializationAndRealSignatures();
        testSignerBlocksDoubleSignInMemory();
        testSignerPersistsDoubleSignProtectionAcrossRestart();
        std::cout << "Nodo out-of-process signer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo out-of-process signer tests failed: " << error.what() << "\n";
        return 1;
    }
}
