#include "core/CoinLot.hpp"
#include "core/CoinLotRegistry.hpp"
#include "core/CoinLotTransactionValidator.hpp"
#include "core/CoinLotTransferPlan.hpp"
#include "core/State.hpp"
#include "core/Transaction.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::CoinLot;
using nodo::core::CoinLotRegistry;
using nodo::core::CoinLotStatus;
using nodo::core::CoinLotTransactionValidator;
using nodo::core::CoinLotTransferPlan;
using nodo::core::State;
using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

CoinLot makeLot(
    const std::string& id,
    const std::string& origin,
    const std::string& owner,
    std::int64_t wholeNodo,
    CoinLotStatus status = CoinLotStatus::AVAILABLE
) {
    return CoinLot(
        id,
        origin,
        owner,
        Amount::fromNodo(wholeNodo),
        status,
        1,
        0,
        kTimestamp
    );
}

Transaction makeExplicitTransfer(
    const std::vector<std::string>& inputLotIds
) {
    return Transaction(
        TransactionType::TRANSFER,
        "nodo1igor",
        "nodo1ana",
        Amount::fromNodo(30),
        Amount::fromNodo(2),
        1,
        kTimestamp,
        inputLotIds
    );
}

CoinLotRegistry sampleRegistry() {
    CoinLotRegistry registry;

    registry.addLot(
        makeLot(
            "lot-a",
            "origin-a",
            "nodo1igor",
            20
        )
    );

    registry.addLot(
        makeLot(
            "lot-b",
            "origin-b",
            "nodo1igor",
            20
        )
    );

    registry.addLot(
        makeLot(
            "lot-c",
            "origin-c",
            "nodo1igor",
            100
        )
    );

    registry.addLot(
        makeLot(
            "lot-wrong-owner",
            "origin-d",
            "nodo1other",
            50
        )
    );

    return registry;
}

void testExplicitInputsArePartOfTransactionIdentity() {
    const Transaction first =
        makeExplicitTransfer(
            {"lot-a", "lot-b"}
        );

    const Transaction second =
        makeExplicitTransfer(
            {"lot-b", "lot-a"}
        );

    requireCondition(
        first.hasExplicitInputCoinLotIds(),
        "Explicit transaction should report input lots."
    );

    requireCondition(
        first.id() != second.id(),
        "Changing explicit input lot order should change transaction id."
    );

    requireCondition(
        first.signingPayload().find("inputLots=[lot-a,lot-b]") != std::string::npos,
        "Signing payload should commit explicit input lots."
    );

    const Transaction loaded =
        Transaction::deserializeForStateReplay(
            first.serialize()
        );

    requireCondition(
        loaded.id() == first.id(),
        "Deserialized explicit-input transaction id mismatch."
    );

    requireCondition(
        loaded.inputCoinLotIds().size() == 2U,
        "Deserialized explicit-input transaction input count mismatch."
    );

    requireCondition(
        loaded.inputCoinLotIds().front() == "lot-a",
        "Deserialized explicit-input transaction input order mismatch."
    );
}

void testLegacyTransactionPayloadRemainsBackwardCompatible() {
    const Transaction legacy(
        TransactionType::TRANSFER,
        "nodo1igor",
        "nodo1ana",
        Amount::fromNodo(10),
        Amount::fromNodo(1),
        1,
        kTimestamp
    );

    requireCondition(
        !legacy.hasExplicitInputCoinLotIds(),
        "Legacy transaction should not have explicit input lots."
    );

    requireCondition(
        legacy.signingPayload().find("inputLots=") == std::string::npos,
        "Legacy transaction payload should not add empty inputLots field."
    );

    const Transaction loaded =
        Transaction::deserializeForStateReplay(
            legacy.serialize()
        );

    requireCondition(
        loaded.id() == legacy.id(),
        "Legacy transaction id changed after round-trip."
    );

    requireCondition(
        loaded.signingPayload() == legacy.signingPayload(),
        "Legacy transaction payload changed after round-trip."
    );
}

void testExplicitInputsAreValidatedAndUsedOnly() {
    CoinLotRegistry registry =
        sampleRegistry();

    const Transaction transaction =
        makeExplicitTransfer(
            {"lot-a", "lot-b"}
        );

    const CoinLotTransferPlan plan =
        CoinLotTransactionValidator::buildTransferPlan(
            transaction,
            registry,
            2
        );

    requireCondition(
        plan.isValid(),
        "Explicit input transfer plan should be valid."
    );

    requireCondition(
        plan.inputLotIds().size() == 2U,
        "Explicit transfer plan should use exactly declared input lots."
    );

    requireCondition(
        plan.totalInputAmount() == Amount::fromNodo(40),
        "Explicit transfer should collect only declared inputs."
    );

    requireCondition(
        plan.changeAmount() == Amount::fromNodo(8),
        "Explicit transfer change is wrong."
    );

    CoinLotTransactionValidator::applyTransfer(
        registry,
        transaction,
        2
    );

    requireCondition(
        registry.lot("lot-a").isSpent(),
        "Explicit input lot-a should be spent."
    );

    requireCondition(
        registry.lot("lot-b").isSpent(),
        "Explicit input lot-b should be spent."
    );

    requireCondition(
        registry.lot("lot-c").isAvailable(),
        "Undeclared spendable lot-c must not be touched."
    );

    requireCondition(
        registry.availableBalanceForOwner("nodo1ana") == Amount::fromNodo(30),
        "Recipient balance is wrong after explicit transfer."
    );

    requireCondition(
        registry.availableBalanceForOwner(State::feePoolAddress()) == Amount::fromNodo(2),
        "Fee pool balance is wrong after explicit transfer."
    );

    requireCondition(
        registry.availableBalanceForOwner("nodo1igor") == Amount::fromNodo(108),
        "Sender remaining balance is wrong after explicit transfer."
    );
}

void testExplicitInputsRejectSilentInputSubstitution() {
    CoinLotRegistry registry =
        sampleRegistry();

    const Transaction transaction =
        makeExplicitTransfer(
            {"lot-a"}
        );

    requireCondition(
        !CoinLotTransactionValidator::validateTransferTransaction(
            transaction,
            registry
        ).success(),
        "Explicit transfer should reject insufficient declared inputs even when other funds exist."
    );

    requireCondition(
        registry.lot("lot-c").isAvailable(),
        "Validator must not substitute undeclared lot-c."
    );
}

void testExplicitInputsRejectUnsafeOrInvalidLots() {
    bool duplicateRejected = false;

    try {
        (void)makeExplicitTransfer(
            {"lot-a", "lot-a"}
        );
    } catch (const std::exception&) {
        duplicateRejected = true;
    }

    requireCondition(
        duplicateRejected,
        "Duplicate explicit input ids should be rejected by Transaction."
    );

    bool unsafeRejected = false;

    try {
        (void)makeExplicitTransfer(
            {"lot-a;bad"}
        );
    } catch (const std::exception&) {
        unsafeRejected = true;
    }

    requireCondition(
        unsafeRejected,
        "Unsafe explicit input id should be rejected by Transaction."
    );

    CoinLotRegistry registry =
        sampleRegistry();

    requireCondition(
        !CoinLotTransactionValidator::validateTransferTransaction(
            makeExplicitTransfer({"missing-lot"}),
            registry
        ).success(),
        "Missing explicit input lot should be rejected."
    );

    requireCondition(
        !CoinLotTransactionValidator::validateTransferTransaction(
            makeExplicitTransfer({"lot-wrong-owner"}),
            registry
        ).success(),
        "Wrong-owner explicit input lot should be rejected."
    );
}

void testAutomaticInputSelectionStillWorksForLegacyTransactions() {
    CoinLotRegistry registry =
        sampleRegistry();

    const Transaction legacy(
        TransactionType::TRANSFER,
        "nodo1igor",
        "nodo1ana",
        Amount::fromNodo(30),
        Amount::fromNodo(2),
        1,
        kTimestamp
    );

    const CoinLotTransferPlan plan =
        CoinLotTransactionValidator::buildTransferPlan(
            legacy,
            registry,
            2
        );

    requireCondition(
        plan.isValid(),
        "Legacy automatic-input transfer plan should remain valid."
    );

    requireCondition(
        plan.inputLotIds().size() == 2U,
        "Legacy transfer should deterministically pick enough input lots."
    );

    requireCondition(
        plan.totalInputAmount() == Amount::fromNodo(40),
        "Legacy automatic transfer collected wrong amount."
    );
}

} // namespace

int main() {
    try {
        testExplicitInputsArePartOfTransactionIdentity();
        testLegacyTransactionPayloadRemainsBackwardCompatible();
        testExplicitInputsAreValidatedAndUsedOnly();
        testExplicitInputsRejectSilentInputSubstitution();
        testExplicitInputsRejectUnsafeOrInvalidLots();
        testAutomaticInputSelectionStillWorksForLegacyTransactions();

        std::cout << "Nodo explicit transaction input tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo explicit transaction input tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
