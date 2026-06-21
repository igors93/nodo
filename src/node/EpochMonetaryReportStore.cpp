#include "node/EpochMonetaryReportStore.hpp"

#include "serialization/KeyValueFileCodec.hpp"

#include <fstream>
#include <limits>
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
        if (raw.empty()) {
            throw std::invalid_argument("empty");
        }
        for (std::size_t index = 0; index < raw.size(); ++index) {
            const char c = raw[index];
            if (c == '-' && index == 0 && raw.size() > 1) {
                continue;
            }
            if (c < '0' || c > '9') {
                throw std::invalid_argument("malformed");
            }
        }
        std::size_t parsedCharacters = 0;
        const long long parsed = std::stoll(raw, &parsedCharacters);
        if (parsedCharacters != raw.size()) {
            throw std::invalid_argument("malformed");
        }
        return static_cast<std::int64_t>(parsed);
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
        if (raw.empty()) {
            throw std::invalid_argument("empty");
        }
        for (const char c : raw) {
            if (c < '0' || c > '9') {
                throw std::invalid_argument("malformed");
            }
        }
        std::size_t parsedCharacters = 0;
        const unsigned long long parsed = std::stoull(raw, &parsedCharacters);
        if (parsedCharacters != raw.size() ||
            parsed > static_cast<unsigned long long>(
                std::numeric_limits<std::uint64_t>::max()
            )) {
            throw std::invalid_argument("malformed");
        }
        return static_cast<std::uint64_t>(parsed);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: field '" + key + "' is not a valid unsigned integer: " + raw
        );
    }
}

void requireNonNegativeAmountRaw(
    std::int64_t value,
    const std::string& key
) {
    if (value < 0) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: field '" + key + "' must not be negative"
        );
    }
}

std::int64_t checkedEndingSupply(
    std::int64_t startingRaw,
    std::int64_t mintedRaw,
    std::int64_t burnedRaw
) {
    if (mintedRaw > std::numeric_limits<std::int64_t>::max() - startingRaw) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: persisted report arithmetic overflow"
        );
    }

    const std::int64_t mintedSupply = startingRaw + mintedRaw;
    return mintedSupply - burnedRaw;
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

    requireNonNegativeAmountRaw(startingRaw, "startingSupplyRawUnits");
    requireNonNegativeAmountRaw(endingRaw, "endingSupplyRawUnits");
    requireNonNegativeAmountRaw(mintedRaw, "totalMintedRawUnits");
    requireNonNegativeAmountRaw(burnedRaw, "totalBurnedRawUnits");

    // Rebuild a consistent report from the fields. The arithmetic invariant:
    // startingSupply + totalMinted - totalBurned == endingSupply
    // is verified here so a tampered file is detected immediately.
    const std::int64_t computedEnding = checkedEndingSupply(
        startingRaw,
        mintedRaw,
        burnedRaw
    );
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

    if (deltaCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        mintCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        burnCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: persisted count exceeds size_t range"
        );
    }

    // Reconstruct the report. We do not call fromDeltas (deltas are not
    // persisted in this file) — instead we rebuild from the stored totals.
    // fromStoredFields performs its own defensive validation (arithmetic,
    // policy version, block range). The caller must verify this report against
    // rebuilt deltas separately (ChainAuditor does this).
    economics::EpochMonetaryReport report =
        economics::EpochMonetaryReport::fromStoredFields(
            policy,
            policyVer,
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

    if (!report.isValid()) {
        throw std::runtime_error(
            "EpochMonetaryReportStore: fromStoredFields rejected stored data: " +
            report.rejectionReason()
        );
    }

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
