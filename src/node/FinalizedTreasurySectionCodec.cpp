#include "node/FinalizedTreasurySectionCodec.hpp"

#include "economics/TreasurySpendRecord.hpp"
#include "serialization/KeyValueFileCodec.hpp"

#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

const std::string kSchemaId = "NODO_FINALIZED_TREASURY_SECTION";

// Prefix used when treasury section is embedded inside a larger artifact document.
// Chosen to avoid conflict with existing "treasury.xxx" fields (GenesisTreasurySnapshot).
const std::string kEmbedPrefix = "treasurySection.";

std::uint64_t parseU64(const std::string& value, const std::string& field) {
    try {
        return std::stoull(value);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "FinalizedTreasurySectionCodec: field '" + field +
            "' is not a valid uint64: " + value
        );
    }
}

std::int64_t parseI64(const std::string& value, const std::string& field) {
    try {
        return std::stoll(value);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "FinalizedTreasurySectionCodec: field '" + field +
            "' is not a valid int64: " + value
        );
    }
}

// Shared record read logic — used in both standalone and embedded decode.
economics::TreasurySpendRecord decodeRecord(
    const serialization::KeyValueFileDocument& doc,
    std::size_t index,
    const std::string& prefix
) {
    const std::string p = prefix + std::to_string(index) + ".";
    const std::string spendId      = doc.requireField(p + "spendId");
    const std::string proposalId   = doc.requireField(p + "proposalId");
    const std::string recipient    = doc.requireField(p + "recipientAddress");
    const std::int64_t amountRaw   = parseI64(doc.requireField(p + "amountRawUnits"),
                                              p + "amountRawUnits");
    const std::string purpose      = doc.requireField(p + "purpose");
    const std::uint64_t execBlock  = parseU64(doc.requireField(p + "executedAtBlock"),
                                              p + "executedAtBlock");
    const std::uint64_t epoch      = parseU64(doc.requireField(p + "epoch"),
                                              p + "epoch");
    const std::int64_t beforeRaw   = parseI64(doc.requireField(p + "balanceBeforeRawUnits"),
                                              p + "balanceBeforeRawUnits");
    const std::int64_t afterRaw    = parseI64(doc.requireField(p + "balanceAfterRawUnits"),
                                              p + "balanceAfterRawUnits");

    economics::TreasurySpendRecord rec(
        spendId, proposalId, recipient,
        utils::Amount::fromRawUnits(amountRaw),
        purpose, execBlock, epoch,
        utils::Amount::fromRawUnits(beforeRaw),
        utils::Amount::fromRawUnits(afterRaw)
    );

    if (!rec.isValid()) {
        throw std::runtime_error(
            "FinalizedTreasurySectionCodec: decoded spend record at index " +
            std::to_string(index) + " is invalid: " + rec.rejectionReason()
        );
    }
    return rec;
}

// Register the per-record field names for one index.
void addRecordFields(
    std::set<std::string>& allowed,
    std::size_t index,
    const std::string& prefix
) {
    const std::string p = prefix + std::to_string(index) + ".";
    allowed.insert(p + "spendId");
    allowed.insert(p + "proposalId");
    allowed.insert(p + "recipientAddress");
    allowed.insert(p + "amountRawUnits");
    allowed.insert(p + "purpose");
    allowed.insert(p + "executedAtBlock");
    allowed.insert(p + "epoch");
    allowed.insert(p + "balanceBeforeRawUnits");
    allowed.insert(p + "balanceAfterRawUnits");
}

// Append per-record fields to a field list.
void appendRecordFields(
    const economics::TreasurySpendRecord& rec,
    std::size_t index,
    const std::string& prefix,
    std::vector<std::pair<std::string, std::string>>& fields
) {
    const std::string p = prefix + std::to_string(index) + ".";
    fields.emplace_back(p + "spendId",              rec.spendId());
    fields.emplace_back(p + "proposalId",           rec.proposalId());
    fields.emplace_back(p + "recipientAddress",     rec.recipientAddress());
    fields.emplace_back(p + "amountRawUnits",       std::to_string(rec.amount().rawUnits()));
    fields.emplace_back(p + "purpose",              rec.purpose());
    fields.emplace_back(p + "executedAtBlock",      std::to_string(rec.executedAtBlock()));
    fields.emplace_back(p + "epoch",                std::to_string(rec.epoch()));
    fields.emplace_back(p + "balanceBeforeRawUnits",
                        std::to_string(rec.treasuryBalanceBefore().rawUnits()));
    fields.emplace_back(p + "balanceAfterRawUnits",
                        std::to_string(rec.treasuryBalanceAfter().rawUnits()));
}

} // namespace

const std::string& FinalizedTreasurySectionCodec::schemaId() {
    return kSchemaId;
}

// ---- Standalone mode ----

std::string FinalizedTreasurySectionCodec::encode(
    const FinalizedTreasurySection& section
) {
    if (!section.isValid()) {
        throw std::invalid_argument(
            "FinalizedTreasurySectionCodec: cannot encode invalid section: " +
            section.rejectionReason()
        );
    }

    std::vector<std::pair<std::string, std::string>> fields;
    fields.emplace_back("spendRecordCount",
                        std::to_string(section.spendRecordCount()));

    for (std::size_t i = 0; i < section.spendRecords().size(); ++i) {
        appendRecordFields(section.spendRecords()[i], i, "spend.", fields);
    }

    return serialization::KeyValueFileCodec::serialize(kSchemaId, fields);
}

FinalizedTreasurySection FinalizedTreasurySectionCodec::decode(
    const std::string& contents
) {
    const serialization::KeyValueFileDocument doc =
        serialization::KeyValueFileCodec::parse(contents, kSchemaId);

    const std::size_t count = static_cast<std::size_t>(
        parseU64(doc.requireField("spendRecordCount"), "spendRecordCount")
    );

    std::set<std::string> allowed;
    allowed.insert("spendRecordCount");
    for (std::size_t i = 0; i < count; ++i) {
        addRecordFields(allowed, i, "spend.");
    }
    doc.requireOnlyFields(allowed);

    std::vector<economics::TreasurySpendRecord> records;
    records.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        records.push_back(decodeRecord(doc, i, "spend."));
    }

    return FinalizedTreasurySection(std::move(records));
}

// ---- Embedded mode ----

std::size_t FinalizedTreasurySectionCodec::spendCountFromDocument(
    const serialization::KeyValueFileDocument& doc
) {
    const std::string fieldName = kEmbedPrefix + "spendCount";
    const std::string& raw = doc.requireField(fieldName);
    return static_cast<std::size_t>(parseU64(raw, fieldName));
}

void FinalizedTreasurySectionCodec::addAllowedFields(
    std::set<std::string>& allowed,
    std::size_t spendCount
) {
    allowed.insert(kEmbedPrefix + "spendCount");
    for (std::size_t i = 0; i < spendCount; ++i) {
        addRecordFields(allowed, i, kEmbedPrefix + "spend.");
    }
}

FinalizedTreasurySection FinalizedTreasurySectionCodec::decodeFromDocument(
    const serialization::KeyValueFileDocument& doc,
    std::size_t spendCount
) {
    std::vector<economics::TreasurySpendRecord> records;
    records.reserve(spendCount);
    for (std::size_t i = 0; i < spendCount; ++i) {
        records.push_back(decodeRecord(doc, i, kEmbedPrefix + "spend."));
    }
    return FinalizedTreasurySection(std::move(records));
}

void FinalizedTreasurySectionCodec::appendFields(
    const FinalizedTreasurySection& section,
    FieldList& fields
) {
    if (!section.isValid()) {
        throw std::invalid_argument(
            "FinalizedTreasurySectionCodec::appendFields: invalid section: " +
            section.rejectionReason()
        );
    }
    fields.emplace_back(kEmbedPrefix + "spendCount",
                        std::to_string(section.spendRecordCount()));
    for (std::size_t i = 0; i < section.spendRecords().size(); ++i) {
        appendRecordFields(section.spendRecords()[i], i, kEmbedPrefix + "spend.", fields);
    }
}

} // namespace nodo::node
