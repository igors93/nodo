#include "economics/MintRecord.hpp"
#include "privacy/PrivateAccountingRecord.hpp"
#include "privacy/PrivacyCommitment.hpp"
#include "privacy/PrivacyNullifier.hpp"
#include "serialization/FieldCodec.hpp"
#include "serialization/MintRecordCodec.hpp"
#include "serialization/PrivacyCommitmentCodec.hpp"
#include "serialization/PrivacyNullifierCodec.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using nodo::economics::MintReason;
using nodo::economics::MintRecord;
using nodo::privacy::PrivacyCommitment;
using nodo::privacy::PrivacyCommitmentType;
using nodo::privacy::PrivacyNullifier;
using nodo::privacy::PrivacyNullifierType;
using nodo::privacy::PrivateAccountingRecord;
using nodo::privacy::PrivateAccountingRecordType;
using nodo::privacy::PublicSupplyEffect;
using nodo::serialization::FieldCodec;
using nodo::serialization::MintRecordCodec;
using nodo::serialization::PrivacyCommitmentCodec;
using nodo::serialization::PrivacyNullifierCodec;
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

    std::vector<PrivacyNullifier> inputNullifiers =
        PrivacyNullifierCodec::deserializeList(inputList);

    std::vector<PrivacyCommitment> outputCommitments =
        PrivacyCommitmentCodec::deserializeList(outputList);

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

void testMintRecordCodecRoundTrip() {
    MintRecord original(
        "mint_test_codec_001",
        "igor",
        Amount::fromNodo(1000),
        MintReason::GENESIS_ALLOCATION,
        0,
        0,
        "GENESIS",
        kTestTimestamp
    );

    const std::string serialized = original.serialize();

    MintRecord rebuilt =
        MintRecordCodec::deserialize(serialized);

    requireCondition(
        rebuilt.isValid(),
        "MintRecordCodec round-trip produced an invalid object."
    );

    requireCondition(
        rebuilt.serialize() == serialized,
        "MintRecordCodec round-trip changed serialization."
    );
}

void testMintRecordLegacyDeserializeDelegatesToCodec() {
    MintRecord original(
        "mint_test_legacy_delegate_001",
        "ana",
        Amount::fromNodo(25),
        MintReason::NETWORK_DEFENSE_REWARD,
        7,
        3,
        "abc123",
        kTestTimestamp + 1
    );

    const std::string serialized = original.serialize();

    MintRecord rebuilt =
        MintRecord::deserialize(serialized);

    requireCondition(
        rebuilt.isValid(),
        "MintRecord::deserialize produced an invalid object."
    );

    requireCondition(
        rebuilt.serialize() == serialized,
        "MintRecord::deserialize changed serialization."
    );
}

void testMintRecordCodecRejectsTamperedAmount() {
    const std::string tampered =
        "MintRecord{id=mint_bad_001;recipient=igor;amountRaw=-1;"
        "reason=GENESIS_ALLOCATION;epoch=0;sourceBlockIndex=0;"
        "sourceBlockHash=GENESIS;timestamp=1700000000}";

    bool rejected = false;

    try {
        (void)MintRecordCodec::deserialize(tampered);
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "MintRecordCodec accepted a tampered negative mint amount."
    );
}

void testPrivacyCommitmentCodecRoundTrip() {
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
        PrivacyCommitmentCodec::deserialize(serialized);

    requireCondition(
        rebuilt.isValid(),
        "PrivacyCommitmentCodec round-trip produced an invalid object."
    );

    requireCondition(
        rebuilt.serialize() == serialized,
        "PrivacyCommitmentCodec round-trip changed serialization."
    );
}

void testPrivacyCommitmentCodecListRoundTrip() {
    PrivacyCommitment first =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::MINT_COMMITMENT,
            "igor",
            Amount::fromNodo(1000),
            "test-blinding-secret-list-001",
            "mint_test_list_001",
            kTestTimestamp
        );

    PrivacyCommitment second =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT,
            "ana",
            Amount::fromNodo(25),
            "test-blinding-secret-list-002",
            first.id(),
            kTestTimestamp + 1
        );

    const std::string serializedList =
        first.serialize() + "," + second.serialize();

    const std::vector<PrivacyCommitment> commitments =
        PrivacyCommitmentCodec::deserializeList(serializedList);

    requireCondition(
        commitments.size() == 2,
        "PrivacyCommitmentCodec list round-trip returned an invalid size."
    );

    requireCondition(
        commitments[0].serialize() == first.serialize(),
        "PrivacyCommitmentCodec list round-trip changed first commitment."
    );

    requireCondition(
        commitments[1].serialize() == second.serialize(),
        "PrivacyCommitmentCodec list round-trip changed second commitment."
    );
}

void testPrivacyCommitmentCodecRejectsTamperedId() {
    PrivacyCommitment original =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::MINT_COMMITMENT,
            "igor",
            Amount::fromNodo(1000),
            "test-blinding-secret-tamper-001",
            "mint_test_tamper_001",
            kTestTimestamp
        );

    std::string tampered = original.serialize();

    const std::string originalId = original.id();
    const std::string replacementPrefix =
        originalId.front() == '0' ? "1" : "0";
    const std::string tamperedId =
        replacementPrefix + originalId.substr(1);

    tampered.replace(
        tampered.find(originalId),
        originalId.size(),
        tamperedId
    );

    bool rejected = false;

    try {
        (void)PrivacyCommitmentCodec::deserialize(tampered);
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "PrivacyCommitmentCodec accepted a tampered commitment id."
    );
}

void testPrivacyNullifierCodecRoundTrip() {
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
        PrivacyNullifierCodec::deserialize(serialized);

    requireCondition(
        rebuilt.isValid(),
        "PrivacyNullifierCodec round-trip produced an invalid object."
    );

    requireCondition(
        rebuilt.serialize() == serialized,
        "PrivacyNullifierCodec round-trip changed serialization."
    );
}

void testPrivacyNullifierCodecListRoundTrip() {
    PrivacyNullifier first =
        PrivacyNullifier::createDevelopmentNullifier(
            PrivacyNullifierType::SPEND_NULLIFIER,
            "commitment_test_list_001",
            "owner-secret-list-001",
            "context-list-a",
            kTestTimestamp
        );

    PrivacyNullifier second =
        PrivacyNullifier::createDevelopmentNullifier(
            PrivacyNullifierType::BURN_NULLIFIER,
            "commitment_test_list_002",
            "owner-secret-list-002",
            "context-list-b",
            kTestTimestamp + 1
        );

    const std::string serializedList =
        first.serialize() + "," + second.serialize();

    const std::vector<PrivacyNullifier> nullifiers =
        PrivacyNullifierCodec::deserializeList(serializedList);

    requireCondition(
        nullifiers.size() == 2,
        "PrivacyNullifierCodec list round-trip returned an invalid size."
    );

    requireCondition(
        nullifiers[0].serialize() == first.serialize(),
        "PrivacyNullifierCodec list round-trip changed first nullifier."
    );

    requireCondition(
        nullifiers[1].serialize() == second.serialize(),
        "PrivacyNullifierCodec list round-trip changed second nullifier."
    );
}

void testPrivacyNullifierCodecRejectsTamperedId() {
    PrivacyNullifier original =
        PrivacyNullifier::createDevelopmentNullifier(
            PrivacyNullifierType::SPEND_NULLIFIER,
            "commitment_test_tamper_001",
            "owner-secret-tamper-001",
            "context-tamper-001",
            kTestTimestamp
        );

    std::string tampered = original.serialize();

    const std::string originalId = original.id();
    const std::string replacementPrefix =
        originalId.front() == '0' ? "1" : "0";
    const std::string tamperedId =
        replacementPrefix + originalId.substr(1);

    tampered.replace(
        tampered.find(originalId),
        originalId.size(),
        tamperedId
    );

    bool rejected = false;

    try {
        (void)PrivacyNullifierCodec::deserialize(tampered);
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "PrivacyNullifierCodec accepted a tampered nullifier id."
    );
}

void testPrivacyNullifierCodecRejectsNegativeTimestamp() {
    const std::string tampered =
        "PrivacyNullifier{id=abc123;type=SPEND_NULLIFIER;"
        "nullifierHash=abc123;contextHash=abc123;createdAt=-1}";

    bool rejected = false;

    try {
        (void)PrivacyNullifierCodec::deserialize(tampered);
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "PrivacyNullifierCodec accepted a negative timestamp."
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
        testMintRecordCodecRoundTrip();
        testMintRecordLegacyDeserializeDelegatesToCodec();
        testMintRecordCodecRejectsTamperedAmount();
        testPrivacyCommitmentCodecRoundTrip();
        testPrivacyCommitmentCodecListRoundTrip();
        testPrivacyCommitmentCodecRejectsTamperedId();
        testPrivacyNullifierCodecRoundTrip();
        testPrivacyNullifierCodecListRoundTrip();
        testPrivacyNullifierCodecRejectsTamperedId();
        testPrivacyNullifierCodecRejectsNegativeTimestamp();
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