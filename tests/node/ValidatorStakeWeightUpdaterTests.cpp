#include "node/ValidatorStakeWeightUpdater.hpp"

#include "consensus/ProposerSchedule.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/KeyPair.hpp"
#include "economics/StakeAccount.hpp"
#include "node/StakingRegistry.hpp"
#include "node/ValidatorLifecycle.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900900000;

crypto::KeyPair validatorKey(const std::string& suffix) {
    return crypto::KeyPair::createDeterministicBls12381KeyPair(
        "epoch-weight-validator-" + suffix
    );
}

std::string registerValidator(
    core::ValidatorRegistry& validators,
    const std::string& suffix,
    std::uint64_t stake
) {
    const crypto::KeyPair key = validatorKey(suffix);
    const std::string address = key.address().value();
    const core::ValidatorRegistrationRecord registration(
        address,
        key.publicKey(),
        1,
        "epoch-weight-" + suffix,
        kTimestamp
    );
    assert(validators.registerValidator(
        registration,
        stake,
        address
    ).accepted());
    return address;
}

void testWeightsChangeOnlyAtEpochBoundaryAndHistoryRemainsImmutable() {
    core::ValidatorRegistry validators;
    const std::string first = registerValidator(
        validators,
        "first",
        1'000'000
    );
    const std::string second = registerValidator(
        validators,
        "second",
        4'000'000
    );

    core::ValidatorSetHistory history;
    assert(history.recordSet(1, validators));

    node::StakingRegistry staking;
    staking.setAccount(
        first,
        economics::StakeAccount(
            first,
            utils::Amount::fromRawUnits(9'000'000)
        )
    );
    staking.setAccount(
        second,
        economics::StakeAccount(
            second,
            utils::Amount::fromRawUnits(16'000'000)
        )
    );

    assert(!node::ValidatorStakeWeightUpdater::synchronizeAtEpochBoundary(
        node::NODO_VALIDATOR_EPOCH_BLOCKS - 1,
        kTimestamp + 1,
        staking,
        validators
    ));
    assert(validators.consensusWeightFor(first) == 1'000);
    assert(validators.consensusWeightFor(second) == 2'000);

    assert(node::ValidatorStakeWeightUpdater::synchronizeAtEpochBoundary(
        node::NODO_VALIDATOR_EPOCH_BLOCKS,
        kTimestamp + 2,
        staking,
        validators
    ));
    assert(
        node::ValidatorStakeWeightUpdater::effectiveEpochForBoundary(
            node::NODO_VALIDATOR_EPOCH_BLOCKS
        ) == 2
    );
    assert(validators.consensusWeightFor(first) == 3'000);
    assert(validators.consensusWeightFor(second) == 4'000);
    assert(validators.totalConsensusWeight() == 7'000);
    assert(consensus::QuorumCertificateBuilder::requiredVotingWeight(
        validators.totalConsensusWeight(),
        2,
        3
    ) == 4'667);
    assert(history.recordSet(2, validators));

    assert(history.setAt(1).consensusWeightFor(first) == 1'000);
    assert(history.setAt(1).consensusWeightFor(second) == 2'000);
    assert(history.setAt(2).consensusWeightFor(first) == 3'000);
    assert(history.setAt(2).consensusWeightFor(second) == 4'000);
    assert(
        history.setAt(1).validatorSetRoot() !=
        history.setAt(2).validatorSetRoot()
    );
}

void testZeroActiveStakeIsExcludedFromConsensus() {
    core::ValidatorRegistry validators;
    const std::string address = registerValidator(
        validators,
        "zero-stake",
        1'000'000
    );

    node::StakingRegistry staking;
    staking.setAccount(
        address,
        economics::StakeAccount(address, utils::Amount())
    );

    assert(node::ValidatorStakeWeightUpdater::synchronizeAtEpochBoundary(
        node::NODO_VALIDATOR_EPOCH_BLOCKS,
        kTimestamp + 1,
        staking,
        validators
    ));
    assert(validators.isValid());
    assert(!validators.isEligibleForConsensus(address));
    assert(validators.consensusWeightFor(address) == 0);
    assert(validators.totalConsensusWeight() == 0);
    assert(validators.eligibleValidatorAddresses().empty());
    assert(consensus::ProposerSchedule::selectProposer(
        validators,
        "epoch-weight-chain",
        node::NODO_VALIDATOR_EPOCH_BLOCKS + 1,
        1
    ).empty());
}

} // namespace

int main() {
    testWeightsChangeOnlyAtEpochBoundaryAndHistoryRemainsImmutable();
    testZeroActiveStakeIsExcludedFromConsensus();
    return 0;
}
