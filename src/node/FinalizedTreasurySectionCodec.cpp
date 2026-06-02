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

} // namespace

const std::string& FinalizedTreasurySectionCodec::schemaId() {
    return kSchemaId;
}

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
        const auto& rec = section.spendRecords()[i];
        const std::string p = "spend." + std::to_string(i) + ".";
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

    // Build the allowed-field set dynamically.
    std::set<std::string> allowed;
    allowed.insert("spendRecordCount");
    for (std::size_t i = 0; i < count; ++i) {
        const std::string p = "spend." + std::to_string(i) + ".";
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
    doc.requireOnlyFields(allowed);

    std::vector<economics::TreasurySpendRecord> records;
    records.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const std::string p = "spend." + std::to_string(i) + ".";
        const std::string spendId          = doc.requireField(p + "spendId");
        const std::string proposalId       = doc.requireField(p + "proposalId");
        const std::string recipient        = doc.requireField(p + "recipientAddress");
        const std::int64_t amountRaw       = parseI64(doc.requireField(p + "amountRawUnits"),
                                                      p + "amountRawUnits");
        const std::string purpose          = doc.requireField(p + "purpose");
        const std::uint64_t execBlock      = parseU64(doc.requireField(p + "executedAtBlock"),
                                                      p + "executedAtBlock");
        const std::uint64_t epoch          = parseU64(doc.requireField(p + "epoch"),
                                                      p + "epoch");
        const std::int64_t beforeRaw       = parseI64(doc.requireField(p + "balanceBeforeRawUnits"),
                                                      p + "balanceBeforeRawUnits");
        const std::int64_t afterRaw        = parseI64(doc.requireField(p + "balanceAfterRawUnits"),
                                                      p + "balanceAfterRawUnits");

        records.emplace_back(
            spendId,
            proposalId,
            recipient,
            utils::Amount::fromRawUnits(amountRaw),
            purpose,
            execBlock,
            epoch,
            utils::Amount::fromRawUnits(beforeRaw),
            utils::Amount::fromRawUnits(afterRaw)
        );

        if (!records.back().isValid()) {
            throw std::runtime_error(
                "FinalizedTreasurySectionCodec: decoded spend record at index " +
                std::to_string(i) + " is invalid: " + records.back().rejectionReason()
            );
        }
    }

    return FinalizedTreasurySection(std::move(records));
}

} // namespace nodo::node
