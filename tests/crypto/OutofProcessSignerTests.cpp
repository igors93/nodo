#include "crypto/OutofProcessSigner.hpp"
#include "crypto/KeyEncryptionService.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::crypto::OutofProcessSigner;
using nodo::crypto::KeyEncryptionService;
using nodo::crypto::SignatureRequest;

void requireCondition(bool condition, const std::string& failureMessage) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testSignerInitializationAndDoubleSignProtection() {
    std::string keyId = "val-key-1";
    std::string privateKeyContent = "secret-key-data-12345";
    std::string password = "strong-password-99";

    // Encrypt the key using the service
    std::string envelope = KeyEncryptionService::encrypt(keyId, privateKeyContent, password);
    requireCondition(!envelope.empty(), "Should encrypt private key content");

    // Initialize signer
    OutofProcessSigner signer(keyId, envelope, password);
    requireCondition(signer.validatorAddress() == "addr_val-key-1", "Validator address should be derived from key ID");

    // Sign a proposal
    SignatureRequest req1{10, 0, "hashA", "block-payload-A"};
    std::string sig1;
    requireCondition(signer.signBlockProposal(req1, sig1), "Should sign first proposal");
    requireCondition(!sig1.empty(), "Signature should not be empty");

    // Retry signing the exact same proposal (idempotency) should succeed
    std::string sig1_retry;
    requireCondition(signer.signBlockProposal(req1, sig1_retry), "Should succeed on identical retry");
    requireCondition(sig1 == sig1_retry, "Retry signature should be identical");

    // Attempting to sign a different proposal for the same height and round (Double signing!)
    SignatureRequest doubleSignReq{10, 0, "hashB", "block-payload-B"};
    std::string sigDouble;
    requireCondition(!signer.signBlockProposal(doubleSignReq, sigDouble), "Signer should block double-signing request!");

    // Sign proposal on a higher height should succeed
    SignatureRequest higherReq{11, 0, "hashC", "block-payload-C"};
    std::string sigHigher;
    requireCondition(signer.signBlockProposal(higherReq, sigHigher), "Should sign higher block height");
}

} // namespace

int main() {
    try {
        testSignerInitializationAndDoubleSignProtection();
        std::cout << "Nodo out-of-process signer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo out-of-process signer tests failed: " << error.what() << "\n";
        return 1;
    }
}
