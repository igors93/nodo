#include "privacy/PrivateAccountingLedgerRebuilder.hpp"

#include "core/LedgerRecord.hpp"
#include "serialization/FieldCodec.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::privacy {

using nodo::serialization::FieldCodec;

namespace {

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

PrivacyCommitment deserializePrivacyCommitment(
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

    if (!commitment.isValid()) {
        throw std::invalid_argument("Deserialized PrivacyCommitment is invalid.");
    }

    return commitment;
}

PrivacyNullifier deserializePrivacyNullifier(
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

    if (!nullifier.isValid()) {
        throw std::invalid_argument("Deserialized PrivacyNullifier is invalid.");
    }

    return nullifier;
}

PrivateAccountingRecord deserializePrivateAccountingRecord(
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
            deserializePrivacyNullifier(serializedNullifier)
        );
    }

    for (const auto& serializedCommitment :
         FieldCodec::splitTopLevelObjects(outputList, "PrivacyCommitment{")) {
        outputCommitments.push_back(
            deserializePrivacyCommitment(serializedCommitment)
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
        utils::Amount::fromRawUnits(
            std::stoll(FieldCodec::extractField(serialized, "publicSupplyAmountRaw"))
        ),
        std::move(inputNullifiers),
        std::move(outputCommitments),
        FieldCodec::extractField(serialized, "auditReference"),
        FieldCodec::extractField(serialized, "proofHash"),
        std::stoll(FieldCodec::extractField(serialized, "timestamp"))
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Deserialized PrivateAccountingRecord is invalid.");
    }

    return record;
}

PrivateAccountingLedger rebuildPrivateLedgerOrThrow(
    const core::Blockchain& blockchain
) {
    if (blockchain.empty()) {
        throw std::logic_error("Blockchain is empty.");
    }

    if (!blockchain.isValid()) {
        throw std::logic_error("Blockchain validation failed.");
    }

    PrivateAccountingLedger ledger;

    for (const auto& block : blockchain.blocks()) {
        if (!block.isValid()) {
            throw std::logic_error("Invalid block found during private ledger rebuild.");
        }

        for (const auto& record : block.records()) {
            if (!record.isValid()) {
                throw std::logic_error("Invalid LedgerRecord found during private ledger rebuild.");
            }

            if (record.type() != core::LedgerRecordType::PRIVATE_ACCOUNTING) {
                continue;
            }

            PrivateAccountingRecord privateRecord =
                deserializePrivateAccountingRecord(record.payload());

            ledger.addRecord(privateRecord);
        }
    }

    if (!ledger.isValid()) {
        throw std::logic_error("Rebuilt PrivateAccountingLedger is invalid.");
    }

    return ledger;
}

} // namespace

PrivateAccountingLedgerRebuildReport::PrivateAccountingLedgerRebuildReport()
    : m_success(true),
      m_failureReason(""),
      m_blockCount(0),
      m_ledgerRecordCount(0),
      m_privateAccountingRecordCount(0),
      m_acceptedPrivateRecordCount(0),
      m_nullifierCount(0),
      m_commitmentCount(0),
      m_privateMintedSupply(utils::Amount::fromRawUnits(0)),
      m_privateBurnedSupply(utils::Amount::fromRawUnits(0)),
      m_outstandingPrivateSupply(utils::Amount::fromRawUnits(0)) {}

bool PrivateAccountingLedgerRebuildReport::success() const {
    return m_success;
}

const std::string& PrivateAccountingLedgerRebuildReport::failureReason() const {
    return m_failureReason;
}

std::size_t PrivateAccountingLedgerRebuildReport::blockCount() const {
    return m_blockCount;
}

std::size_t PrivateAccountingLedgerRebuildReport::ledgerRecordCount() const {
    return m_ledgerRecordCount;
}

std::size_t PrivateAccountingLedgerRebuildReport::privateAccountingRecordCount() const {
    return m_privateAccountingRecordCount;
}

std::size_t PrivateAccountingLedgerRebuildReport::acceptedPrivateRecordCount() const {
    return m_acceptedPrivateRecordCount;
}

std::size_t PrivateAccountingLedgerRebuildReport::nullifierCount() const {
    return m_nullifierCount;
}

std::size_t PrivateAccountingLedgerRebuildReport::commitmentCount() const {
    return m_commitmentCount;
}

utils::Amount PrivateAccountingLedgerRebuildReport::privateMintedSupply() const {
    return m_privateMintedSupply;
}

utils::Amount PrivateAccountingLedgerRebuildReport::privateBurnedSupply() const {
    return m_privateBurnedSupply;
}

utils::Amount PrivateAccountingLedgerRebuildReport::outstandingPrivateSupply() const {
    return m_outstandingPrivateSupply;
}

void PrivateAccountingLedgerRebuildReport::markFailure(
    std::string reason
) {
    m_success = false;
    m_failureReason = std::move(reason);
}

void PrivateAccountingLedgerRebuildReport::setBlockCount(
    std::size_t value
) {
    m_blockCount = value;
}

void PrivateAccountingLedgerRebuildReport::incrementLedgerRecordCount() {
    ++m_ledgerRecordCount;
}

void PrivateAccountingLedgerRebuildReport::incrementPrivateAccountingRecordCount() {
    ++m_privateAccountingRecordCount;
}

void PrivateAccountingLedgerRebuildReport::setLedgerMetrics(
    const PrivateAccountingLedger& ledger
) {
    m_acceptedPrivateRecordCount = ledger.size();
    m_nullifierCount = ledger.nullifierSet().size();
    m_commitmentCount = ledger.registeredCommitmentCount();
    m_privateMintedSupply = ledger.privateMintedSupply();
    m_privateBurnedSupply = ledger.privateBurnedSupply();
    m_outstandingPrivateSupply = ledger.outstandingPrivateSupply();
}

std::string PrivateAccountingLedgerRebuildReport::serialize() const {
    std::ostringstream oss;

    oss << "PrivateAccountingLedgerRebuildReport{"
        << "success=" << (m_success ? "true" : "false")
        << ";failureReason=" << m_failureReason
        << ";blockCount=" << m_blockCount
        << ";ledgerRecordCount=" << m_ledgerRecordCount
        << ";privateAccountingRecordCount=" << m_privateAccountingRecordCount
        << ";acceptedPrivateRecordCount=" << m_acceptedPrivateRecordCount
        << ";nullifierCount=" << m_nullifierCount
        << ";commitmentCount=" << m_commitmentCount
        << ";privateMintedSupply=" << m_privateMintedSupply.toString()
        << ";privateBurnedSupply=" << m_privateBurnedSupply.toString()
        << ";outstandingPrivateSupply=" << m_outstandingPrivateSupply.toString()
        << "}";

    return oss.str();
}

PrivateAccountingLedgerRebuildReport
PrivateAccountingLedgerRebuilder::auditBlockchain(
    const core::Blockchain& blockchain
) {
    PrivateAccountingLedgerRebuildReport report;

    try {
        if (blockchain.empty()) {
            report.markFailure("Blockchain is empty.");
            return report;
        }

        if (!blockchain.isValid()) {
            report.markFailure("Blockchain validation failed.");
            return report;
        }

        report.setBlockCount(blockchain.size());

        for (const auto& block : blockchain.blocks()) {
            if (!block.isValid()) {
                report.markFailure("Invalid block found during private ledger audit.");
                return report;
            }

            for (const auto& record : block.records()) {
                if (!record.isValid()) {
                    report.markFailure("Invalid LedgerRecord found during private ledger audit.");
                    return report;
                }

                report.incrementLedgerRecordCount();

                if (record.type() == core::LedgerRecordType::PRIVATE_ACCOUNTING) {
                    report.incrementPrivateAccountingRecordCount();
                }
            }
        }

        PrivateAccountingLedger ledger = rebuildPrivateLedgerOrThrow(blockchain);
        report.setLedgerMetrics(ledger);

        return report;
    } catch (const std::exception& error) {
        report.markFailure(error.what());
        return report;
    }
}

PrivateAccountingLedger PrivateAccountingLedgerRebuilder::rebuildFromBlockchain(
    const core::Blockchain& blockchain
) {
    return rebuildPrivateLedgerOrThrow(blockchain);
}

} // namespace nodo::privacy