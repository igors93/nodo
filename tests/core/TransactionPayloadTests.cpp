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
        "proposal-1", "validator-address", core::GovernanceVoteChoice::YES);
    const auto decoded = core::GovernanceVotePayload::deserialize(payload.serialize());
    require(decoded.proposalId() == "proposal-1" &&
            decoded.validatorAddress() == "validator-address" &&
            decoded.choice() == core::GovernanceVoteChoice::YES,
        "Governance vote payload must round-trip validator identity and choice.");
}

void testGovernanceProposalPayloadIsCanonical() {
    const core::GovernanceProposalPayload payload =
        core::GovernanceProposalPayload::parameterChange(
            "Minimum fee",
            "Raise minimum fee after approval",
            "MINIMUM_FEE_RAW",
            "250",
            10,
            0,
            2
        );
    const auto decoded = core::GovernanceProposalPayload::deserialize(payload.serialize());
    require(decoded.type() == core::GovernanceProposalType::PARAMETER_CHANGE &&
            decoded.parameterTarget() == "MINIMUM_FEE_RAW" &&
            decoded.parameterValue() == "250" &&
            decoded.parameterEffectiveHeight() == 10 &&
            decoded.votingStartDelayBlocks() == 0 &&
            decoded.votingPeriodBlocks() == 2,
        "Governance proposal payload must round-trip explicit proposal fields.");
}

} // namespace

int main() {
    try {
        testValidatorRegistrationPayloadIsCanonical();
        testGovernanceProposalPayloadIsCanonical();
        testGovernanceVotePayloadIsCanonical();
        std::cout << "Transaction payload tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Transaction payload tests failed: " << error.what() << '\n';
        return 1;
    }
}
