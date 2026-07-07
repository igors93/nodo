#include "core/Block.hpp"
#include "core/Blockchain.hpp"
#include "core/ChainStateRebuilder.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ValidatorProposalRegistry.hpp"
#include "economics/ValidatorPenaltyRecord.hpp"
#include "serialization/LedgerRecordCodec.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::core::Block;
using nodo::core::Blockchain;
using nodo::core::ChainStateRebuilder;
using nodo::core::LedgerRecord;
using nodo::core::LedgerRecordType;
using nodo::core::StateRebuildReport;
using nodo::core::ValidatorDoubleSignEvidence;
using nodo::core::ValidatorProposalRegistryEntry;
using nodo::economics::ValidatorPenaltyAction;
using nodo::economics::ValidatorPenaltyPolicy;
using nodo::economics::ValidatorPenaltyReason;
using nodo::economics::ValidatorPenaltyRecord;
using nodo::economics::ValidatorScoreReason;
using nodo::serialization::LedgerRecordCodec;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

ValidatorProposalRegistryEntry proposalEntry(
    const std::string& blockHash,
    const std::string& signatureDigest,
    std::int64_t proposedAt
) {
    return ValidatorProposalRegistryEntry(
        "nodo1validatorA",
        "validator-public-key-fingerprint-a",
        7,
        blockHash,
        "previous-chain-tip-hash",
        7,
        "previous-chain-tip-hash",
        proposedAt,
        signatureDigest
    );
}

ValidatorDoubleSignEvidence doubleSignEvidence() {
    return ValidatorDoubleSignEvidence(
        proposalEntry(
            "block-hash-first",
            "signature-digest-first",
            kTimestamp
        ),
        proposalEntry(
            "block-hash-conflicting",
            "signature-digest-conflicting",
            kTimestamp + 1
        )
    );
}

void testPenaltyPolicyCreatesDoubleSignPenalty() {
    const ValidatorDoubleSignEvidence evidence =
        doubleSignEvidence();

    requireCondition(
        evidence.isValid(),
        "Fixture double-sign evidence should be valid."
    );

    const ValidatorPenaltyPolicy policy =
        ValidatorPenaltyPolicy::conservativeDefaultPolicy();

    const ValidatorPenaltyRecord penalty =
        policy.createDoubleSignPenaltyRecord(
            evidence,
            3,
            80,
            kTimestamp + 2
        );

    requireCondition(
        penalty.isValid(),
        "Double-sign penalty record should be valid."
    );

    requireCondition(
        penalty.validatorAddress() == "nodo1validatorA",
        "Penalty validator mismatch."
    );

    requireCondition(
        penalty.previousScore() == 80,
        "Penalty previous score mismatch."
    );

    requireCondition(
        penalty.newScore() == 40,
        "Penalty new score should reduce by default policy amount."
    );

    requireCondition(
        penalty.reason() == ValidatorPenaltyReason::DOUBLE_SIGN,
        "Penalty reason should be DOUBLE_SIGN."
    );

    requireCondition(
        penalty.action() == ValidatorPenaltyAction::SCORE_REDUCTION,
        "Penalty action should be SCORE_REDUCTION."
    );

    requireCondition(
        penalty.firstBlockHash() != penalty.conflictingBlockHash(),
        "Penalty should commit to conflicting block hashes."
    );

    requireCondition(
        !penalty.deterministicId().empty(),
        "Penalty deterministic id should not be empty."
    );
}

void testPenaltyCreatesScoreRecord() {
    const ValidatorPenaltyPolicy policy =
        ValidatorPenaltyPolicy::conservativeDefaultPolicy();

    const ValidatorPenaltyRecord penalty =
        policy.createDoubleSignPenaltyRecord(
            doubleSignEvidence(),
            3,
            30,
            kTimestamp + 3
        );

    const auto scoreRecord =
        penalty.createScoreRecord();

    requireCondition(
        scoreRecord.isValid(),
        "Score record from penalty should be valid."
    );

    requireCondition(
        scoreRecord.previousScore() == 30 && scoreRecord.newScore() == 0,
        "Score record should clamp penalty at zero."
    );

    requireCondition(
        scoreRecord.reason() == ValidatorScoreReason::CONFLICTING_SIGNATURE,
        "Score record reason should be CONFLICTING_SIGNATURE."
    );

    requireCondition(
        scoreRecord.evidenceHash() == penalty.deterministicId(),
        "Score record evidence should point to penalty id."
    );
}

void testPenaltyLedgerRecordRoundTripsThroughCodec() {
    const ValidatorPenaltyRecord penalty =
        ValidatorPenaltyPolicy::conservativeDefaultPolicy()
            .createDoubleSignPenaltyRecord(
                doubleSignEvidence(),
                3,
                75,
                kTimestamp + 4
            );

    const LedgerRecord record =
        LedgerRecord::fromValidatorPenaltyRecord(penalty, kTimestamp + 4);

    requireCondition(
        record.type() == LedgerRecordType::VALIDATOR_PENALTY,
        "Penalty ledger record should be VALIDATOR_PENALTY."
    );

    requireCondition(
        record.sourceId() == penalty.deterministicId(),
        "Penalty ledger source id should match penalty id."
    );

    const LedgerRecord loaded =
        LedgerRecordCodec::deserialize(
            record.serialize()
        );

    requireCondition(
        loaded.serialize() == record.serialize(),
        "Validator penalty LedgerRecord should round-trip through codec."
    );
}

void testPenaltyRecordDeserializesRoundTrip() {
    const ValidatorPenaltyPolicy policy =
        ValidatorPenaltyPolicy::conservativeDefaultPolicy();

    const ValidatorPenaltyRecord original =
        policy.createDoubleSignPenaltyRecord(
            doubleSignEvidence(),
            3,
            90,
            kTimestamp + 5
        );

    const ValidatorPenaltyRecord loaded =
        ValidatorPenaltyRecord::deserialize(
            original.serialize()
        );

    requireCondition(
        loaded.serialize() == original.serialize(),
        "ValidatorPenaltyRecord serialization should round-trip."
    );

    requireCondition(
        loaded.deterministicId() == original.deterministicId(),
        "ValidatorPenaltyRecord deterministic id should survive round-trip."
    );
}

void testInvalidPenaltyInputsAreRejected() {
    const ValidatorPenaltyPolicy policy =
        ValidatorPenaltyPolicy::conservativeDefaultPolicy();

    bool rejected = false;

    try {
        (void)policy.createDoubleSignPenaltyRecord(
            doubleSignEvidence(),
            0,
            90,
            kTimestamp + 6
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Zero epoch should be rejected."
    );

    rejected = false;

    try {
        (void)policy.createDoubleSignPenaltyRecord(
            doubleSignEvidence(),
            3,
            120,
            kTimestamp + 6
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Out-of-range previous score should be rejected."
    );

    const ValidatorDoubleSignEvidence invalidEvidence(
        proposalEntry("same-block-hash", "signature-a", kTimestamp),
        proposalEntry("same-block-hash", "signature-b", kTimestamp + 1)
    );

    rejected = false;

    try {
        (void)policy.createDoubleSignPenaltyRecord(
            invalidEvidence,
            3,
            90,
            kTimestamp + 6
        );
    } catch (const std::exception&) {
        rejected = true;
    }

    requireCondition(
        rejected,
        "Invalid double-sign evidence should be rejected."
    );
}

void testPenaltyLedgerRecordsAreAuditedByChainStateRebuilder() {
    const ValidatorPenaltyRecord penalty =
        ValidatorPenaltyPolicy::conservativeDefaultPolicy()
            .createDoubleSignPenaltyRecord(
                doubleSignEvidence(),
                3,
                80,
                kTimestamp + 7
            );

    const std::vector<LedgerRecord> records = {
        LedgerRecord::fromValidatorPenaltyRecord(penalty, kTimestamp + 7),
        LedgerRecord::fromValidatorScoreRecord(
            penalty.createScoreRecord(),
            kTimestamp + 7
        )
    };

    const Block genesis =
        Block::createGenesisBlock(
            records,
            kTimestamp + 8
        );

    Blockchain blockchain;
    blockchain.addGenesisBlock(genesis);

    requireCondition(
        blockchain.isValid(),
        "Blockchain with penalty records should be valid."
    );

    const StateRebuildReport report =
        ChainStateRebuilder::auditBlockchain(blockchain);

    requireCondition(
        report.success(),
        "ChainStateRebuilder should audit penalty ledger records."
    );

    requireCondition(
        report.validatorPenaltyRecordCount() == 1U,
        "Penalty audit should count one ValidatorPenaltyRecord."
    );

    requireCondition(
        report.protectionMetadataRecordCount() == 1U,
        "Penalty audit should count score record as protection metadata."
    );
}

} // namespace

int main() {
    try {
        testPenaltyPolicyCreatesDoubleSignPenalty();
        testPenaltyCreatesScoreRecord();
        testPenaltyLedgerRecordRoundTripsThroughCodec();
        testPenaltyRecordDeserializesRoundTrip();
        testInvalidPenaltyInputsAreRejected();
        testPenaltyLedgerRecordsAreAuditedByChainStateRebuilder();

        std::cout << "Nodo validator penalty record tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator penalty record tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
