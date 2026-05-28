#include "privacy/PrivateAccountingRecord.hpp"
#include "privacy/PrivacyCommitment.hpp"
#include "privacy/PrivacyNullifier.hpp"
#include "serialization/FieldCodec.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using nodo::privacy::PrivacyCommitment;
using nodo::privacy::PrivacyCommitmentType;
using nodo::privacy::PrivacyNullifier;
using nodo::privacy::PrivacyNullifierType;
using nodo::privacy::PrivateAccountingRecord;
using nodo::privacy::PrivateAccountingRecordType;
using nodo::privacy::PublicSupplyEffect;
using nodo::serialization::FieldCodec;
using nodo::utils::Amount;

constexpr std::int64_t kTestTimestamp = 1700000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

PrivacyCommitmentType parsePrivacyCommitmentType(
    const std::string& value
) {
    if (value == "MINT_COMMITMENT") {
        return PrivacyCommitmentType::MINT_COMMITMENT;
    }

    if (value == "TRANSFER_OUTPUT_COMMITMENT") {
        return PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT;
    }

    if (value == "BURN_COMMITMENT") {
        return PrivacyCommitmentType::BURN_COMMITMENT;
    }

    throw std::invalid_argument("Unknown PrivacyCommitmentType: " + value);
}

PrivacyNullifierType parsePrivacyNullifierType(
    const std::string& value
) {
    if (value == "SPEND_NULLIFIER") {
        return PrivacyNullifierType::SPEND_NULLIFIER;
    }

    if (value == "BURN_NULLIFIER") {
        return PrivacyNullifierType::BURN_NULLIFIER;
    }

    throw std::invalid_argument("Unknown PrivacyNullifierType: " + value);
}

PrivateAccountingRecordType parsePrivateAccountingRecordType(
    const std::string& value
) {
    if (value == "PRIVATE_MINT") {
        return PrivateAccountingRecordType::PRIVATE_MINT;
    }

    if (value == "PRIVATE_TRANSFER") {
        return PrivateAccountingRecordType::PRIVATE_TRANSFER;
    }

    if (value == "PRIVATE_BURN") {
        return PrivateAccountingRecordType::PRIVATE_BURN;
    }

    throw std::invalid_argument("Unknown PrivateAccountingRecordType: " + value);
}

PublicSupplyEffect parsePublicSupplyEffect(
    const std::string& value
) {
    if (value == "NO_SUPPLY_CHANGE") {
        return PublicSupplyEffect::NO_SUPPLY_CHANGE;
    }

    if (value == "SUPPLY_INCREASE") {
        return PublicSupplyEffect::SUPPLY_INCREASE;
    }

    if (value == "SUPPLY_DECREASE") {
        return PublicSupplyEffect::SUPPLY_DECREASE;
    }

    throw std::invalid_argument("Unknown PublicSupplyEffect: " + value);
}

PrivacyCommitment deserializePrivacyCommitmentForTest(
    const std::string& serialized
) {
    if (serialized.rfind("PrivacyCommitment{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a PrivacyCommitment.");
    }

    PrivacyCommitment commitment(
        FieldCodec::extractField(serialized, "id"),
        parsePrivacyCommitmentType(
            FieldCodec::extractField(serialized, "type")
        ),
        FieldCodec::extractField(serialized, "commitmentHash"),
        FieldCodec::extractField(serialized, "ownerHint"),
        FieldCodec::extractField(serialized, "sourceReference"),
        std::stoll(FieldCodec::extractField(serialized, "timestamp"))
    );

    requireCondition(
        commitment.isValid(),
        "Deserialized PrivacyCommitment is invalid."
    );

    return commitment;
}

PrivacyNullifier deserializePrivacyNullifierForTest(
    const std::string& serialized
) {
    if (serialized.rfind("PrivacyNullifier{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a PrivacyNullifier.");
    }

    PrivacyNullifier nullifier(
        FieldCodec::extractField(serialized, "id"),
        parsePrivacyNullifierType(
            FieldCodec::extractField(serialized, "type")
        ),
        FieldCodec::extractField(serialized, "nullifierHash"),
        FieldCodec::extractField(serialized, "contextHash"),
        std::stoll(FieldCodec::extractField(serialized, "createdAt"))
    );

    requireCondition(
        nullifier.isValid(),
        "Deserialized PrivacyNullifier is invalid."
    );

    return nullifier;
}

PrivateAccountingRecord deserializePrivateAccountingRecordForTest(
    const std::string& serialized
) {
    if (serialized.rfind("PrivateAccountingRecord{", 0) != 0) {
        throw std::invalid_argument("Serialized object is not a PrivateAccountingRecord.");
    }

    const std::string inputList = FieldCodec::extractBetween(
        serialized,
        ";inputNullifiers=[",
        "];outputCommitments=["
    );

    const std::string outputList = FieldCodec::extractTrailingSection(
        serialized,
        "];outputCommitments=[",
        "]}"
    );

    std::vector<PrivacyNullifier> inputNullifiers;
    std::vector<PrivacyCommitment> outputCommitments;

    for (const auto& serializedNullifier :
         FieldCodec::splitTopLevelObjects(inputList, "PrivacyNullifier{")) {
        inputNullifiers.push_back(
            deserializePrivacyNullifierForTest(serializedNullifier)
        );
    }

    for (const auto& serializedCommitment :
         FieldCodec::splitTopLevelObjects(outputList, "PrivacyCommitment{")) {
        outputCommitments.push_back(
            deserializePrivacyCommitmentForTest(serializedCommitment)
        );
    }

    PrivateAccountingRecord record(
        FieldCodec::extractField(serialized, "id"),
        parsePrivateAccountingRecordType(
            FieldCodec::extractField(serialized, "type")
        ),
        parsePublicSupplyEffect(
            FieldCodec::extractField(serialized, "supplyEffect")
        ),
        Amount::fromRawUnits(
            std::stoll(FieldCodec::extractField(serialized, "publicSupplyAmountRaw"))
        ),
        std::move(inputNullifiers),
        std::move(outputCommitments),
        FieldCodec::extractField(serialized, "auditReference"),
        FieldCodec::extractField(serialized, "proofHash"),
        std::stoll(FieldCodec::extractField(serialized, "timestamp"))
    );

    requireCondition(
        record.isValid(),
        "Deserialized PrivateAccountingRecord is invalid."
    );

    return record;
}

void testFieldCodecBasicExtraction() {
    const std::string serialized =
        "Example{id=abc123;type=TEST;payload=[Object{id=inner}]};";

    requireCondition(
        FieldCodec::extractField(serialized, "id") == "abc123",
        "FieldCodec failed to extract id."
    );

    requireCondition(
        FieldCodec::extractField(serialized, "type") == "TEST",
        "FieldCodec failed to extract type."
    );
}

void testFieldCodecTopLevelObjectSplit() {
    const std::string serializedList =
        "PrivacyCommitment{id=a;type=MINT_COMMITMENT},"
        "PrivacyCommitment{id=b;type=MINT_COMMITMENT}";

    const std::vector<std::string> objects =
        FieldCodec::splitTopLevelObjects(
            serializedList,
            "PrivacyCommitment{"
        );

    requireCondition(
        objects.size() == 2,
        "FieldCodec failed to split two top-level objects."
    );

    requireCondition(
        objects[0].find("id=a") != std::string::npos,
        "First split object is incorrect."
    );

    requireCondition(
        objects[1].find("id=b") != std::string::npos,
        "Second split object is incorrect."
    );
}

void testPrivacyCommitmentRoundTrip() {
    PrivacyCommitment original =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::MINT_COMMITMENT,
            "igor",
            Amount::fromNodo(1000),
            "test-blinding-secret-001",
            "mint_test_001",
            kTestTimestamp
        );

    const std::string serialized = original.serialize();

    PrivacyCommitment rebuilt =
        deserializePrivacyCommitmentForTest(serialized);

    requireCondition(
        rebuilt.isValid(),
        "PrivacyCommitment round-trip produced an invalid object."
    );

    requireCondition(
        rebuilt.serialize() == serialized,
        "PrivacyCommitment round-trip changed serialization."
    );
}

void testPrivacyNullifierRoundTrip() {
    PrivacyNullifier original =
        PrivacyNullifier::createDevelopmentNullifier(
            PrivacyNullifierType::SPEND_NULLIFIER,
            "commitment_test_001",
            "owner-secret-001",
            "context-001",
            kTestTimestamp
        );

    const std::string serialized = original.serialize();

    PrivacyNullifier rebuilt =
        deserializePrivacyNullifierForTest(serialized);

    requireCondition(
        rebuilt.isValid(),
        "PrivacyNullifier round-trip produced an invalid object."
    );

    requireCondition(
        rebuilt.serialize() == serialized,
        "PrivacyNullifier round-trip changed serialization."
    );
}

void testNullifierDeterminismIgnoresContext() {
    PrivacyNullifier first =
        PrivacyNullifier::createDevelopmentNullifier(
            PrivacyNullifierType::SPEND_NULLIFIER,
            "commitment_test_001",
            "owner-secret-001",
            "context-a",
            kTestTimestamp
        );

    PrivacyNullifier second =
        PrivacyNullifier::createDevelopmentNullifier(
            PrivacyNullifierType::SPEND_NULLIFIER,
            "commitment_test_001",
            "owner-secret-001",
            "context-b",
            kTestTimestamp + 10
        );

    requireCondition(
        first.nullifierHash() == second.nullifierHash(),
        "Nullifier hash must not depend on spend context or timestamp."
    );

    requireCondition(
        first.id() == second.id(),
        "Nullifier id must remain stable for the same private coin and owner secret."
    );

    requireCondition(
        first.contextHash() != second.contextHash(),
        "Context hash should change when spend context changes."
    );
}

void testPrivateAccountingRecordRoundTrip() {
    PrivacyCommitment inputCommitment =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::MINT_COMMITMENT,
            "igor",
            Amount::fromNodo(1000),
            "test-blinding-secret-input",
            "mint_test_001",
            kTestTimestamp
        );

    PrivacyNullifier inputNullifier =
        PrivacyNullifier::createDevelopmentNullifier(
            PrivacyNullifierType::SPEND_NULLIFIER,
            inputCommitment.id(),
            "owner-secret-igor-001",
            "private-transfer-test-context",
            kTestTimestamp
        );

    PrivacyCommitment outputToAna =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT,
            "ana",
            Amount::fromNodo(25),
            "test-blinding-secret-ana",
            inputNullifier.id(),
            kTestTimestamp
        );

    PrivacyCommitment changeToIgor =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT,
            "igor",
            Amount::fromRawUnits(97500000000),
            "test-blinding-secret-igor-change",
            inputNullifier.id(),
            kTestTimestamp
        );

    PrivateAccountingRecord original =
        PrivateAccountingRecord::createPrivateTransferRecord(
            std::vector<PrivacyNullifier>{inputNullifier},
            std::vector<PrivacyCommitment>{
                outputToAna,
                changeToIgor
            },
            "test-private-transfer-audit-reference",
            kTestTimestamp
        );

    const std::string serialized = original.serialize();

    PrivateAccountingRecord rebuilt =
        deserializePrivateAccountingRecordForTest(serialized);

    requireCondition(
        rebuilt.isValid(),
        "PrivateAccountingRecord round-trip produced an invalid object."
    );

    requireCondition(
        rebuilt.serialize() == serialized,
        "PrivateAccountingRecord round-trip changed serialization."
    );

    requireCondition(
        rebuilt.inputNullifiers().size() == 1,
        "PrivateAccountingRecord round-trip lost input nullifiers."
    );

    requireCondition(
        rebuilt.outputCommitments().size() == 2,
        "PrivateAccountingRecord round-trip lost output commitments."
    );
}

} // namespace

int main() {
    try {
        testFieldCodecBasicExtraction();
        testFieldCodecTopLevelObjectSplit();
        testPrivacyCommitmentRoundTrip();
        testPrivacyNullifierRoundTrip();
        testNullifierDeterminismIgnoresContext();
        testPrivateAccountingRecordRoundTrip();

        std::cout << "Nodo serialization round-trip tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo serialization round-trip tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}