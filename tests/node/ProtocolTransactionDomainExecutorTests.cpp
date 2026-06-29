#include "core/TransactionExecutionRouter.hpp"
#include "core/TransactionPayload.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"
#include "node/ProtocolTransactionDomainExecutor.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace nodo;
constexpr std::int64_t kTimestamp = 1900000000;
const std::string kOwner = "nodo-owner-account";

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    node::ProtocolExecutionState state;
    std::string validatorAddress;
};

Fixture fixture() {
    const crypto::KeyPair validator =
        crypto::KeyPair::createDeterministicBls12381KeyPair("router-domain-validator");
    const std::string address =
        crypto::AddressDerivation::deriveFromPublicKey(validator.publicKey()).value();
    core::ValidatorRegistry validators;
    const core::ValidatorRegistrationRecord record(
        address, validator.publicKey(), 1, "domain-validator-meta", kTimestamp);
    require(validators.registerPendingValidator(
        record, core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS, kOwner).accepted(),
        "validator fixture registration failed");
    require(validators.activateValidator(address, 1, kTimestamp + 1).success(),
        "validator fixture activation failed");

    node::StakingRegistry staking;
    staking.deposit(
        kOwner, address,
        utils::Amount::fromRawUnits(core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS),
        1, false
    );

    return {
        node::ProtocolExecutionState{
            node::GovernanceExecutor(), validators,
            consensus::ValidatorPenaltyLedger(), staking,
            utils::Amount::fromRawUnits(100'000'000), {}
        },
        address
    };
}

core::AccountStateView accounts() {
    core::AccountStateView view;
    require(view.putAccount(core::AccountState(
        kOwner, utils::Amount::fromRawUnits(10'000'000), 0)),
        "owner account fixture failed");
    return view;
}

core::Transaction tx(
    core::TransactionType type,
    const std::string& target,
    std::int64_t amount,
    std::uint64_t nonce,
    std::string data = {}
) {
    return data.empty()
        ? core::Transaction(
              type, kOwner, target, utils::Amount::fromRawUnits(amount),
              utils::Amount::fromRawUnits(10), nonce, kTimestamp + nonce)
        : core::Transaction(
              type, kOwner, target, utils::Amount::fromRawUnits(amount),
              utils::Amount::fromRawUnits(10), nonce, kTimestamp + nonce,
              std::move(data));
}

core::TransactionExecutionResult execute(
    const core::Transaction& transaction,
    const core::AccountStateView& current,
    core::TransactionDomainExecutor& executor,
    std::uint64_t height
) {
    return core::TransactionExecutionRouter::execute(
        transaction,
        core::TransactionExecutionContext(
            current, height, kTimestamp + static_cast<std::int64_t>(height),
            true, false, true, "", nullptr, &executor
        )
    );
}

void testCanonicalDomainHandlersAndDeterministicReplay() {
    const Fixture base = fixture();
    auto run = [&](std::shared_ptr<node::ProtocolExecutionState> tracker) {
        auto factory = node::makeProtocolDomainExecutorFactory(
            base.state, core::ValidatorSetHistory(), "localnet", tracker);
        auto executor = factory();
        core::AccountStateView view = accounts();

        const std::vector<core::Transaction> transactions = {
            tx(core::TransactionType::BURN, "nodo_burn", 100, 1),
            tx(core::TransactionType::STAKE_DEPOSIT, base.validatorAddress, 1000, 2),
            tx(core::TransactionType::STAKE_TOP_UP, base.validatorAddress, 1000, 3),
            tx(core::TransactionType::STAKE_WITHDRAW, base.validatorAddress, 500, 4),
            tx(core::TransactionType::GOVERNANCE_PROPOSE, "nodo_governance", 0, 5,
               "target=MINIMUM_FEE_RAW,value=250,effectiveHeight=100")
        };

        std::string proposalId;
        std::vector<std::string> receiptHashes;
        for (const auto& transaction : transactions) {
            const std::uint64_t executionHeight =
                transaction.nonce() >= 4 ? 200 : 100;
            const auto result = execute(transaction, view, *executor, executionHeight);
            require(result.success(), "protocol handler rejected valid transaction: " + result.reason());
            require(result.receipt().isValid(), "protocol handler generated invalid receipt");
            view = result.accounts();
            receiptHashes.push_back(result.receipt().receiptHash());
            if (transaction.type() == core::TransactionType::GOVERNANCE_PROPOSE) {
                proposalId = transaction.id();
            }
        }

        const core::GovernanceVotePayload vote(
            base.validatorAddress, core::GovernanceVoteChoice::APPROVE);
        const auto voteResult = execute(
            tx(core::TransactionType::GOVERNANCE_VOTE, proposalId, 0, 6, vote.serialize()),
            view, *executor, 200);
        require(voteResult.success(), "authorized governance vote must execute: " + voteResult.reason());
        view = voteResult.accounts();
        receiptHashes.push_back(voteResult.receipt().receiptHash());

        require(tracker->supply.rawUnits() == 99'999'900,
            "voluntary burn must reduce canonical supply");
        require(tracker->staking.ownedStake(kOwner, base.validatorAddress).rawUnits() ==
            core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS + 1500,
            "deposit, top-up and withdrawal must update the owned stake position");
        require(tracker->governance.proposalApproved(proposalId) &&
                tracker->governance.currentValueForTarget(
                    node::GovernanceParameterTarget::MINIMUM_FEE_RAW) == "250",
            "governance change must apply only after the authorized weighted vote");
        return std::make_pair(view.serialize(),
            tracker->governance.serialize() + tracker->staking.serialize() +
            tracker->validators.serialize() + std::to_string(tracker->supply.rawUnits()) +
            receiptHashes.front() + receiptHashes.back());
    };

    auto firstTracker = std::make_shared<node::ProtocolExecutionState>(base.state);
    auto secondTracker = std::make_shared<node::ProtocolExecutionState>(base.state);
    require(run(firstTracker) == run(secondTracker),
        "replaying the same mixed transaction sequence must produce identical state and receipts");
}

void testValidatorRegistrationExitAndUnjail() {
    Fixture base = fixture();
    auto tracker = std::make_shared<node::ProtocolExecutionState>(base.state);
    auto factory = node::makeProtocolDomainExecutorFactory(
        base.state, core::ValidatorSetHistory(), "localnet", tracker);
    auto executor = factory();
    core::AccountStateView view = accounts();

    const crypto::KeyPair newValidator =
        crypto::KeyPair::createDeterministicBls12381KeyPair("new-domain-validator");
    const std::string newAddress =
        crypto::AddressDerivation::deriveFromPublicKey(newValidator.publicKey()).value();
    const core::ValidatorRegistrationPayload registration(
        newValidator.publicKey(), "new-validator-meta");
    const auto registered = execute(
        tx(core::TransactionType::VALIDATOR_REGISTER, newAddress,
           core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS, 1,
           registration.serialize()),
        view, *executor, 100);
    require(registered.success(), "validator registration must execute: " + registered.reason());
    require(tracker->validators.entryForAddress(newAddress) != nullptr &&
            tracker->validators.entryForAddress(newAddress)->ownerAddress() == kOwner,
        "validator registration must commit identity, owner and pending lifecycle state");

    const auto exit = execute(
        tx(core::TransactionType::VALIDATOR_EXIT_REQUEST, base.validatorAddress, 0, 2),
        registered.accounts(), *executor, 100);
    require(exit.success() &&
            tracker->validators.entryForAddress(base.validatorAddress)->status() ==
                core::ValidatorRegistrationStatus::EXIT_REQUESTED,
        "validator owner must be able to request deterministic exit");

    Fixture jailed = fixture();
    require(jailed.state.validators.jailValidator(
        jailed.validatorAddress, 1, kTimestamp + 2).success(),
        "jailed fixture registry update failed");
    auto stake = jailed.state.staking.accountOrDefault(jailed.validatorAddress);
    stake.jail();
    jailed.state.staking.setAccount(jailed.validatorAddress, stake);
    auto jailedTracker = std::make_shared<node::ProtocolExecutionState>(jailed.state);
    auto jailedFactory = node::makeProtocolDomainExecutorFactory(
        jailed.state, core::ValidatorSetHistory(), "localnet", jailedTracker);
    auto jailedExecutor = jailedFactory();
    const auto unjailed = execute(
        tx(core::TransactionType::VALIDATOR_UNJAIL_REQUEST, jailed.validatorAddress, 0, 1),
        accounts(), *jailedExecutor, 100);
    require(unjailed.success() &&
            !jailedTracker->validators.entryForAddress(jailed.validatorAddress)->jailed() &&
            !jailedTracker->staking.accountOrDefault(jailed.validatorAddress).jailed(),
        "authorized unjail must update validator registry and staking state together");
}

} // namespace

int main() {
    try {
        testCanonicalDomainHandlersAndDeterministicReplay();
        testValidatorRegistrationExitAndUnjail();
        std::cout << "Protocol transaction domain executor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Protocol transaction domain executor tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
