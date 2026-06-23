#include "core/EventLog.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::EventLog;
using nodo::core::EventType;
using nodo::core::eventTypeToString;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000LL;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testTransferFactoryMethod() {
    const EventLog event = EventLog::transfer(
        "tx-001", "alice", "bob",
        Amount::fromRawUnits(500), 42, kTimestamp
    );

    requireCondition(
        event.type() == EventType::TRANSFER,
        "transfer() factory should produce TRANSFER event type."
    );
    requireCondition(
        event.transactionId() == "tx-001",
        "transaction id should match."
    );
    requireCondition(
        event.primaryAddress() == "alice",
        "primaryAddress should be from address."
    );
    requireCondition(
        event.secondaryAddress() == "bob",
        "secondaryAddress should be to address."
    );
    requireCondition(
        event.amount().rawUnits() == 500,
        "amount should match."
    );
    requireCondition(
        event.blockHeight() == 42,
        "blockHeight should match."
    );
    requireCondition(
        event.timestamp() == kTimestamp,
        "timestamp should match."
    );
}

void testStakeDepositedFactoryMethod() {
    const EventLog event = EventLog::stakeDeposited(
        "tx-002", "carol",
        Amount::fromRawUnits(1000), 100, kTimestamp + 1
    );

    requireCondition(
        event.type() == EventType::STAKE_DEPOSITED,
        "stakeDeposited() should produce STAKE_DEPOSITED type."
    );
    requireCondition(
        event.primaryAddress() == "carol",
        "primaryAddress should be the staker."
    );
    requireCondition(
        event.secondaryAddress().empty(),
        "secondaryAddress should be empty for stake deposit."
    );
}

void testValidatorRegisteredFactoryMethod() {
    const EventLog event = EventLog::validatorRegistered(
        "tx-003", "dave", 200, kTimestamp + 2
    );

    requireCondition(
        event.type() == EventType::VALIDATOR_REGISTERED,
        "validatorRegistered() should produce VALIDATOR_REGISTERED type."
    );
    requireCondition(
        event.amount().isZero(),
        "amount should be zero for validator registration."
    );
}

void testGovernanceProposedFactoryMethod() {
    const EventLog event = EventLog::governanceProposed(
        "tx-004", "erin", "proposal-99", 300, kTimestamp + 3
    );

    requireCondition(
        event.type() == EventType::GOVERNANCE_PROPOSED,
        "governanceProposed() should produce GOVERNANCE_PROPOSED type."
    );
    requireCondition(
        event.primaryAddress() == "erin",
        "primaryAddress should be the proposer."
    );
    requireCondition(
        event.secondaryAddress() == "proposal-99",
        "secondaryAddress should hold the proposal id."
    );
}

void testGovernanceExecutedFactoryMethod() {
    const EventLog event = EventLog::governanceExecuted(
        "tx-005", "proposal-99", 400, kTimestamp + 4
    );

    requireCondition(
        event.type() == EventType::GOVERNANCE_EXECUTED,
        "governanceExecuted() should produce GOVERNANCE_EXECUTED type."
    );
    requireCondition(
        event.primaryAddress() == "proposal-99",
        "primaryAddress should hold the proposal id for execution."
    );
}

void testTokenBurnedFactoryMethod() {
    const EventLog event = EventLog::tokenBurned(
        "tx-006", "frank",
        Amount::fromRawUnits(200), 500, kTimestamp + 5
    );

    requireCondition(
        event.type() == EventType::TOKEN_BURNED,
        "tokenBurned() should produce TOKEN_BURNED type."
    );
    requireCondition(
        event.amount().rawUnits() == 200,
        "burn amount should match."
    );
}

void testIsValidRejectsDefaultConstructed() {
    const EventLog empty;
    requireCondition(
        !empty.isValid(),
        "Default-constructed EventLog should not be valid."
    );
}

void testIsValidAcceptsWellFormedEvent() {
    const EventLog event = EventLog::transfer(
        "tx-valid", "addr-a", "addr-b",
        Amount::fromRawUnits(100), 1, kTimestamp
    );
    requireCondition(
        event.isValid(),
        "Well-formed EventLog should be valid."
    );
}

void testSerializeAndEventHash() {
    const EventLog event = EventLog::transfer(
        "tx-hash-test", "sender", "receiver",
        Amount::fromRawUnits(999), 7, kTimestamp + 10
    );

    const std::string serialized = event.serialize();
    requireCondition(
        !serialized.empty(),
        "serialize() should produce a non-empty string."
    );
    requireCondition(
        serialized.find("tx-hash-test") != std::string::npos,
        "Serialized form should contain the transaction id."
    );
    requireCondition(
        serialized.find("TRANSFER") != std::string::npos,
        "Serialized form should contain the event type string."
    );

    const std::string hash1 = event.eventHash();
    const std::string hash2 = event.eventHash();

    requireCondition(
        !hash1.empty(),
        "eventHash() should produce a non-empty string."
    );
    requireCondition(
        hash1 == hash2,
        "eventHash() must be deterministic."
    );
}

void testEventTypeToStringCoversAllTypes() {
    requireCondition(eventTypeToString(EventType::TRANSFER) == "TRANSFER", "TRANSFER");
    requireCondition(eventTypeToString(EventType::STAKE_DEPOSITED) == "STAKE_DEPOSITED", "STAKE_DEPOSITED");
    requireCondition(eventTypeToString(EventType::STAKE_WITHDRAWN) == "STAKE_WITHDRAWN", "STAKE_WITHDRAWN");
    requireCondition(eventTypeToString(EventType::VALIDATOR_REGISTERED) == "VALIDATOR_REGISTERED", "VALIDATOR_REGISTERED");
    requireCondition(eventTypeToString(EventType::VALIDATOR_DEREGISTERED) == "VALIDATOR_DEREGISTERED", "VALIDATOR_DEREGISTERED");
    requireCondition(eventTypeToString(EventType::VALIDATOR_PENALIZED) == "VALIDATOR_PENALIZED", "VALIDATOR_PENALIZED");
    requireCondition(eventTypeToString(EventType::GOVERNANCE_PROPOSED) == "GOVERNANCE_PROPOSED", "GOVERNANCE_PROPOSED");
    requireCondition(eventTypeToString(EventType::GOVERNANCE_VOTED) == "GOVERNANCE_VOTED", "GOVERNANCE_VOTED");
    requireCondition(eventTypeToString(EventType::GOVERNANCE_EXECUTED) == "GOVERNANCE_EXECUTED", "GOVERNANCE_EXECUTED");
    requireCondition(eventTypeToString(EventType::TOKEN_BURNED) == "TOKEN_BURNED", "TOKEN_BURNED");
    requireCondition(eventTypeToString(EventType::TOKEN_MINTED) == "TOKEN_MINTED", "TOKEN_MINTED");
    requireCondition(eventTypeToString(EventType::FEE_BURNED) == "FEE_BURNED", "FEE_BURNED");
}

} // namespace

int main() {
    try {
        testTransferFactoryMethod();
        testStakeDepositedFactoryMethod();
        testValidatorRegisteredFactoryMethod();
        testGovernanceProposedFactoryMethod();
        testGovernanceExecutedFactoryMethod();
        testTokenBurnedFactoryMethod();
        testIsValidRejectsDefaultConstructed();
        testIsValidAcceptsWellFormedEvent();
        testSerializeAndEventHash();
        testEventTypeToStringCoversAllTypes();

        std::cout << "Nodo EventLog tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo EventLog tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
