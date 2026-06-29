#include "core/Transaction.hpp"
#include "core/TransactionPayload.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"

#include <iostream>
#include <stdexcept>

namespace {

using namespace nodo;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void testValidatorRegistrationPayloadIsCanonical() {
    const crypto::KeyPair validator =
        crypto::KeyPair::createDeterministicBls12381KeyPair("payload-validator");
    const core::ValidatorRegistrationPayload payload(
        validator.publicKey(), "metadata-hash-1");
    const auto decoded = core::ValidatorRegistrationPayload::deserialize(payload.serialize());
    require(decoded.validatorPublicKey().serialize() == validator.publicKey().serialize() &&
            decoded.metadataHash() == "metadata-hash-1",
        "Validator registration payload must round-trip all required identity data.");

    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(validator.publicKey()).value();
    const core::Transaction transaction(
        core::TransactionType::VALIDATOR_REGISTER, "owner-address", address,
        utils::Amount::fromRawUnits(1'000'000), utils::Amount::fromRawUnits(1),
        1, 1900000000, payload.serialize());
    const core::Transaction restored = core::Transaction::deserialize(transaction.serialize());
    require(restored.data() == payload.serialize() &&
            restored.signingPayload() == transaction.signingPayload(),
        "Transaction codec must preserve binary canonical payload data.");
}

void testGovernanceVotePayloadIsCanonical() {
    const core::GovernanceVotePayload payload(
        "validator-address", core::GovernanceVoteChoice::APPROVE);
    const auto decoded = core::GovernanceVotePayload::deserialize(payload.serialize());
    require(decoded.validatorAddress() == "validator-address" &&
            decoded.choice() == core::GovernanceVoteChoice::APPROVE,
        "Governance vote payload must round-trip validator identity and choice.");
}

} // namespace

int main() {
    try {
        testValidatorRegistrationPayloadIsCanonical();
        testGovernanceVotePayloadIsCanonical();
        std::cout << "Transaction payload tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Transaction payload tests failed: " << error.what() << '\n';
        return 1;
    }
}
