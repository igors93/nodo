#include "node/EpochMonetaryReportStore.hpp"

#include "serialization/KeyValueFileCodec.hpp"

#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

const std::string kSchemaId = "NODO_EPOCH_MONETARY_REPORT";

// All fields present in every persisted report.
const std::set<std::string> kAllowedFields = {
    "epoch",
    "startBlock",
    "endBlock",
    "startingSupplyRawUnits",
    "endingSupplyRawUnits",
    "totalMintedRawUnits",
    "totalBurnedRawUnits",
    "deltaCount",
    "mintRecordCount",
    "burnRecordCount",
    "policyVersion"
};

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: cannot open file: " + path.string()
        );
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

void writeFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: cannot write file: " + path.string()
        );
    }
    file << contents;
    if (!file.good()) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: write error for file: " + path.string()
        );
    }
}

std::int64_t requireInt64(
    const serialization::KeyValueFileDocument& doc,
    const std::string& key
) {
    const std::string& raw = doc.requireField(key);
    try {
        return std::stoll(raw);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: field '" + key + "' is not a valid integer: " + raw
        );
    }
}

std::uint64_t requireUint64(
    const serialization::KeyValueFileDocument& doc,
    const std::string& key
) {
    const std::string& raw = doc.requireField(key);
    try {
        return std::stoull(raw);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: field '" + key + "' is not a valid unsigned integer: " + raw
        );
    }
}

} // namespace

const std::string& EpochMonetaryReportStore::schemaId() {
    return kSchemaId;
}

std::string EpochMonetaryReportStore::encode(
    const economics::EpochMonetaryReport& report
) {
    if (!report.isValid()) {
        throw std::invalid_argument(
            "EpochMonetaryReportStore: cannot encode invalid report: " +
            report.rejectionReason()
        );
    }

    std::vector<std::pair<std::string, std::string>> fields;
    fields.emplace_back("epoch",                    std::to_string(report.epoch()));
    fields.emplace_back("startBlock",               std::to_string(report.startBlock()));
    fields.emplace_back("endBlock",                 std::to_string(report.endBlock()));
    fields.emplace_back("startingSupplyRawUnits",   std::to_string(report.startingSupply().rawUnits()));
    fields.emplace_back("endingSupplyRawUnits",     std::to_string(report.endingSupply().rawUnits()));
    fields.emplace_back("totalMintedRawUnits",      std::to_string(report.totalMinted().rawUnits()));
    fields.emplace_back("totalBurnedRawUnits",      std::to_string(report.totalBurned().rawUnits()));
    fields.emplace_back("deltaCount",               std::to_string(report.deltaCount()));
    fields.emplace_back("mintRecordCount",          std::to_string(report.mintRecordCount()));
    fields.emplace_back("burnRecordCount",          std::to_string(report.burnRecordCount()));
    fields.emplace_back("policyVersion",            report.policyVersion());

    return serialization::KeyValueFileCodec::serialize(kSchemaId, fields);
}

economics::EpochMonetaryReport EpochMonetaryReportStore::decode(
    const std::string& contents,
    const economics::MonetaryPolicy& policy
) {
    const serialization::KeyValueFileDocument doc =
        serialization::KeyValueFileCodec::parse(contents, kSchemaId);

    doc.requireOnlyFields(kAllowedFields);

    const std::uint64_t epoch      = requireUint64(doc, "epoch");
    const std::uint64_t startBlock = requireUint64(doc, "startBlock");
    const std::uint64_t endBlock   = requireUint64(doc, "endBlock");
    const std::int64_t  startingRaw = requireInt64(doc, "startingSupplyRawUnits");
    const std::int64_t  endingRaw   = requireInt64(doc, "endingSupplyRawUnits");
    const std::int64_t  mintedRaw   = requireInt64(doc, "totalMintedRawUnits");
    const std::int64_t  burnedRaw   = requireInt64(doc, "totalBurnedRawUnits");
    const std::uint64_t deltaCount  = requireUint64(doc, "deltaCount");
    const std::uint64_t mintCount   = requireUint64(doc, "mintRecordCount");
    const std::uint64_t burnCount   = requireUint64(doc, "burnRecordCount");
    const std::string   policyVer   = doc.requireField("policyVersion");

    // Rebuild a consistent report from the fields. The arithmetic invariant:
    // startingSupply + totalMinted - totalBurned == endingSupply
    // is verified here so a tampered file is detected immediately.
    const std::int64_t computedEnding = startingRaw + mintedRaw - burnedRaw;
    if (computedEnding != endingRaw) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: persisted report fails arithmetic check: "
            "startingSupply(" + std::to_string(startingRaw) +
            ") + minted(" + std::to_string(mintedRaw) +
            ") - burned(" + std::to_string(burnedRaw) +
            ") = " + std::to_string(computedEnding) +
            " != endingSupply(" + std::to_string(endingRaw) + ")."
        );
    }

    if (policyVer != policy.policyVersion()) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: policyVersion mismatch: "
            "stored=" + policyVer + " expected=" + policy.policyVersion()
        );
    }

    // Reconstruct the report. We do not call fromDeltas (deltas are not
    // persisted in this file) — instead we rebuild from the stored totals.
    // The caller must verify this report against rebuilt deltas separately
    // (ChainAuditor does this).
    economics::EpochMonetaryReport report =
        economics::EpochMonetaryReport::fromStoredFields(
            policy,
            epoch,
            startBlock,
            endBlock,
            utils::Amount::fromRawUnits(startingRaw),
            utils::Amount::fromRawUnits(endingRaw),
            utils::Amount::fromRawUnits(mintedRaw),
            utils::Amount::fromRawUnits(burnedRaw),
            static_cast<std::size_t>(deltaCount),
            static_cast<std::size_t>(mintCount),
            static_cast<std::size_t>(burnCount)
        );

    return report;
}

void EpochMonetaryReportStore::write(
    const std::filesystem::path& filePath,
    const economics::EpochMonetaryReport& report
) {
    writeFile(filePath, encode(report));
}

economics::EpochMonetaryReport EpochMonetaryReportStore::read(
    const std::filesystem::path& filePath,
    const economics::MonetaryPolicy& policy
) {
    return decode(readFile(filePath), policy);
}

} // namespace nodo::node
