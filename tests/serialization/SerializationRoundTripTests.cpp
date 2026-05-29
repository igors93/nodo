#include "economics/MintRecord.hpp"
#include "privacy/PrivateAccountingRecord.hpp"
#include "privacy/PrivacyCommitment.hpp"
#include "privacy/PrivacyNullifier.hpp"
#include "serialization/FieldCodec.hpp"
#include "serialization/MintRecordCodec.hpp"
#include "serialization/PrivacyCommitmentCodec.hpp"
#include "serialization/PrivacyNullifierCodec.hpp"
#include "serialization/PrivateAccountingRecordCodec.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::economics::MintReason;
using nodo::economics::MintRecord;
using nodo::privacy::PrivacyCommitment;
using nodo::privacy::PrivacyCommitmentType;
using nodo::privacy::PrivacyNullifier;
using nodo::privacy::PrivacyNullifierType;
using nodo::privacy::PrivateAccountingRecord;
using nodo::privacy::PublicSupplyEffect;
using nodo::serialization::FieldCodec;
using nodo::serialization::MintRecordCodec;
using nodo::serialization::PrivacyCommitmentCodec;
using nodo::serialization::PrivacyNullifierCodec;
using nodo::serialization::PrivateAccountingRecordCodec;
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

std::string tamperFirstHexCharacter(
    const std::string& value
) {
    if (value.empty()) {
        throw std::invalid_argument("Cannot tamper empty string.");
    }

    const std::string replacementPrefix =
        value.front() == '0' ? "1" : "0";

    return replacementPrefix + value.substr(1);
}

std::string replaceFirst(
    const std::string& input,
    const std::string& target,
    const std::string& replacement
) {
    const std::size_t position = input.find(target);

    if (position == std::string::npos) {
        throw std::runtime_error("Could not find text to replace: " + target);
    }

    std::string output = input;
    output.replace(position, target.size(), replacement);

    return output;
}

void requireRejected(
    const std::string& failureMessage,
    const std::function<void()>& action
) {
    bool rejected = false;

    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        failureMessage
    );
}

PrivacyCommitment createTestMintCommitment(
    const std::string& owner,
    const std::string& sourceReference,
    std::int64_t timestamp
) {
    return PrivacyCommitment::createDevelopmentCommitment(
        PrivacyCommitmentType::MINT_COMMITMENT,
        owner,
        Amount::fromNodo(1000),
        "test-blinding-secret-" + owner + "-" + sourceReference,
        sourceReference,
        timestamp
    );
}

PrivacyNullifier createTestSpendNullifier(
    const std::string& commitmentId,
    const std::string& ownerSecret,
    const std::string& context,
    std::int64_t timestamp
) {
    return PrivacyNullifier::createDevelopmentNullifier(
        PrivacyNullifierType::SPEND_NULLIFIER,
        commitmentId,
        ownerSecret,
        context,
        timestamp
    );
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
    const std::string tamperedId =
        tamperFirstHexCharacter(originalId);

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
        createTestSpendNullifier(
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
        createTestSpendNullifier(
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
        createTestSpendNullifier(
            "commitment_test_tamper_001",
            "owner-secret-tamper-001",
            "context-tamper-001",
            kTestTimestamp
        );

    std::string tampered = original.serialize();

    const std::string originalId = original.id();
    const std::string tamperedId =
        tamperFirstHexCharacter(originalId);

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
        createTestSpendNullifier(
            "commitment_test_001",
            "owner-secret-001",
            "context-a",
            kTestTimestamp
        );

    PrivacyNullifier second =
        createTestSpendNullifier(
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

PrivateAccountingRecord createTestPrivateTransferRecord() {
    PrivacyCommitment inputCommitment =
        createTestMintCommitment(
            "igor",
            "mint_test_private_accounting_001",
            kTestTimestamp
        );

    PrivacyNullifier inputNullifier =
        createTestSpendNullifier(
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

    return PrivateAccountingRecord::createPrivateTransferRecord(
        std::vector<PrivacyNullifier>{inputNullifier},
        std::vector<PrivacyCommitment>{
            outputToAna,
            changeToIgor
        },
        "test-private-transfer-audit-reference",
        kTestTimestamp
    );
}

void testPrivateAccountingRecordCodecRoundTrip() {
    PrivateAccountingRecord original =
        createTestPrivateTransferRecord();

    const std::string serialized = original.serialize();

    PrivateAccountingRecord rebuilt =
        PrivateAccountingRecordCodec::deserialize(serialized);

    requireCondition(
        rebuilt.isValid(),
        "PrivateAccountingRecordCodec round-trip produced an invalid object."
    );

    requireCondition(
        rebuilt.serialize() == serialized,
        "PrivateAccountingRecordCodec round-trip changed serialization."
    );

    requireCondition(
        rebuilt.inputNullifiers().size() == 1,
        "PrivateAccountingRecordCodec round-trip lost input nullifiers."
    );

    requireCondition(
        rebuilt.outputCommitments().size() == 2,
        "PrivateAccountingRecordCodec round-trip lost output commitments."
    );
}

void testPrivateAccountingRecordCodecListRoundTrip() {
    PrivacyCommitment mintCommitment =
        createTestMintCommitment(
            "igor",
            "mint_test_private_list_001",
            kTestTimestamp
        );

    PrivateAccountingRecord mintRecord =
        PrivateAccountingRecord::createPrivateMintRecord(
            std::vector<PrivacyCommitment>{mintCommitment},
            Amount::fromNodo(1000),
            "test-private-mint-audit-reference",
            kTestTimestamp
        );

    PrivateAccountingRecord transferRecord =
        createTestPrivateTransferRecord();

    const std::string serializedList =
        mintRecord.serialize() + "," + transferRecord.serialize();

    const std::vector<PrivateAccountingRecord> records =
        PrivateAccountingRecordCodec::deserializeList(serializedList);

    requireCondition(
        records.size() == 2,
        "PrivateAccountingRecordCodec list round-trip returned an invalid size."
    );

    requireCondition(
        records[0].serialize() == mintRecord.serialize(),
        "PrivateAccountingRecordCodec list round-trip changed first record."
    );

    requireCondition(
        records[1].serialize() == transferRecord.serialize(),
        "PrivateAccountingRecordCodec list round-trip changed second record."
    );
}

void testPrivateAccountingRecordCodecRejectsTamperedProofHash() {
    PrivateAccountingRecord original =
        createTestPrivateTransferRecord();

    std::string tampered = original.serialize();

    const std::string originalProofHash = original.proofHash();
    const std::string tamperedProofHash =
        tamperFirstHexCharacter(originalProofHash);

    tampered.replace(
        tampered.find(originalProofHash),
        originalProofHash.size(),
        tamperedProofHash
    );

    bool rejected = false;

    try {
        (void)PrivateAccountingRecordCodec::deserialize(tampered);
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "PrivateAccountingRecordCodec accepted a tampered proof hash."
    );
}

void testPrivateAccountingRecordCodecRejectsInvalidShape() {
    PrivacyCommitment output =
        createTestMintCommitment(
            "ana",
            "mint_test_invalid_shape_001",
            kTestTimestamp
        );

    PrivateAccountingRecord mintRecord =
        PrivateAccountingRecord::createPrivateMintRecord(
            std::vector<PrivacyCommitment>{output},
            Amount::fromNodo(1000),
            "test-invalid-shape-reference",
            kTestTimestamp
        );

    std::string tampered = mintRecord.serialize();

    const std::string oldType = "type=PRIVATE_MINT";
    const std::string newType = "type=PRIVATE_TRANSFER";

    tampered.replace(
        tampered.find(oldType),
        oldType.size(),
        newType
    );

    bool rejected = false;

    try {
        (void)PrivateAccountingRecordCodec::deserialize(tampered);
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "PrivateAccountingRecordCodec accepted an invalid record shape."
    );
}


void testCanonicalSerializationRejectsMintRecordFieldReordering() {
    MintRecord original(
        "mint_test_canonical_order_001",
        "igor",
        Amount::fromNodo(1000),
        MintReason::GENESIS_ALLOCATION,
        0,
        0,
        "GENESIS",
        kTestTimestamp
    );

    const std::string nonCanonical =
        "MintRecord{recipient=igor;id=mint_test_canonical_order_001;"
        "amountRaw=100000000000;reason=GENESIS_ALLOCATION;epoch=0;"
        "sourceBlockIndex=0;sourceBlockHash=GENESIS;timestamp=1700000000}";

    requireRejected(
        "MintRecordCodec accepted reordered fields.",
        [&]() {
            (void)MintRecordCodec::deserialize(nonCanonical);
        }
    );
}

void testCanonicalSerializationRejectsMintRecordUnknownField() {
    MintRecord original(
        "mint_test_canonical_unknown_001",
        "igor",
        Amount::fromNodo(1000),
        MintReason::GENESIS_ALLOCATION,
        0,
        0,
        "GENESIS",
        kTestTimestamp
    );

    const std::string nonCanonical = replaceFirst(
        original.serialize(),
        ";timestamp=1700000000}",
        ";timestamp=1700000000;unknownField=forbidden}"
    );

    requireRejected(
        "MintRecordCodec accepted an unknown field.",
        [&]() {
            (void)MintRecordCodec::deserialize(nonCanonical);
        }
    );
}

void testCanonicalSerializationRejectsMintRecordLeadingZeroAmount() {
    MintRecord original(
        "mint_test_canonical_leading_zero_001",
        "igor",
        Amount::fromNodo(1000),
        MintReason::GENESIS_ALLOCATION,
        0,
        0,
        "GENESIS",
        kTestTimestamp
    );

    const std::string nonCanonical = replaceFirst(
        original.serialize(),
        "amountRaw=100000000000",
        "amountRaw=0100000000000"
    );

    requireRejected(
        "MintRecordCodec accepted a non-canonical leading-zero amount.",
        [&]() {
            (void)MintRecordCodec::deserialize(nonCanonical);
        }
    );
}

void testCanonicalSerializationRejectsPrivacyCommitmentWhitespace() {
    PrivacyCommitment original =
        PrivacyCommitment::createDevelopmentCommitment(
            PrivacyCommitmentType::MINT_COMMITMENT,
            "igor",
            Amount::fromNodo(1000),
            "test-blinding-secret-canonical-whitespace",
            "mint_test_canonical_whitespace_001",
            kTestTimestamp
        );

    const std::string nonCanonical = replaceFirst(
        original.serialize(),
        ";type=",
        "; type="
    );

    requireRejected(
        "PrivacyCommitmentCodec accepted non-canonical whitespace.",
        [&]() {
            (void)PrivacyCommitmentCodec::deserialize(nonCanonical);
        }
    );
}

void testCanonicalSerializationRejectsPrivacyNullifierTrailingData() {
    PrivacyNullifier original =
        createTestSpendNullifier(
            "commitment_test_canonical_trailing_001",
            "owner-secret-canonical-trailing-001",
            "context-canonical-trailing-001",
            kTestTimestamp
        );

    const std::string nonCanonical =
        original.serialize() + " ";

    requireRejected(
        "PrivacyNullifierCodec accepted trailing data.",
        [&]() {
            (void)PrivacyNullifierCodec::deserialize(nonCanonical);
        }
    );
}

void testCanonicalSerializationRejectsPrivateAccountingRecordListWhitespace() {
    PrivateAccountingRecord original =
        createTestPrivateTransferRecord();

    const std::string nonCanonical = replaceFirst(
        original.serialize(),
        "},PrivacyCommitment{",
        "}, PrivacyCommitment{"
    );

    requireRejected(
        "PrivateAccountingRecordCodec accepted non-canonical list whitespace.",
        [&]() {
            (void)PrivateAccountingRecordCodec::deserialize(nonCanonical);
        }
    );
}

void testCanonicalSerializationRejectsPrivateAccountingRecordMissingField() {
    PrivateAccountingRecord original =
        createTestPrivateTransferRecord();

    const std::string serialized = original.serialize();
    const std::string auditReferenceToken =
        ";auditReference=test-private-transfer-audit-reference";

    const std::string nonCanonical = replaceFirst(
        serialized,
        auditReferenceToken,
        ""
    );

    requireRejected(
        "PrivateAccountingRecordCodec accepted a missing required field.",
        [&]() {
            (void)PrivateAccountingRecordCodec::deserialize(nonCanonical);
        }
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
        testPrivateAccountingRecordCodecRoundTrip();
        testPrivateAccountingRecordCodecListRoundTrip();
        testPrivateAccountingRecordCodecRejectsTamperedProofHash();
        testPrivateAccountingRecordCodecRejectsInvalidShape();
        testCanonicalSerializationRejectsMintRecordFieldReordering();
        testCanonicalSerializationRejectsMintRecordUnknownField();
        testCanonicalSerializationRejectsMintRecordLeadingZeroAmount();
        testCanonicalSerializationRejectsPrivacyCommitmentWhitespace();
        testCanonicalSerializationRejectsPrivacyNullifierTrailingData();
        testCanonicalSerializationRejectsPrivateAccountingRecordListWhitespace();
        testCanonicalSerializationRejectsPrivateAccountingRecordMissingField();

        std::cout << "Nodo serialization round-trip tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo serialization round-trip tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}