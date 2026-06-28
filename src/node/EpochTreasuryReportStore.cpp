#include "node/EpochTreasuryReportStore.hpp"

#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

const std::string kSchemaId = "NODO_EPOCH_TREASURY_REPORT";

const std::set<std::string> kAllowedFields = {
    "epoch",
    "treasurySpendTotalRawUnits",
    "spendRecordCount",
    "spendRecordsDigest"
};

std::string readFile(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error(
            "EpochTreasuryReportStore: cannot open file: " + path.string()
        );
    }
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

void writeFile(const std::filesystem::path& path, const std::string& contents) {
    storage::AtomicFile::writeTextFile(path, contents);
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
            "EpochTreasuryReportStore: field '" + key +
            "' is not a valid unsigned integer: " + raw
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
            "EpochTreasuryReportStore: field '" + key +
            "' is not a valid integer: " + raw
        );
    }
}

} // namespace

const std::string& EpochTreasuryReportStore::schemaId() {
    return kSchemaId;
}

std::string EpochTreasuryReportStore::encode(
    const economics::EpochTreasuryReport& report
) {
    if (!report.isValid()) {
        throw std::invalid_argument(
            "EpochTreasuryReportStore: cannot encode invalid report: " +
            report.rejectionReason()
        );
    }

    std::vector<std::pair<std::string, std::string>> fields;
    fields.emplace_back("epoch",                     std::to_string(report.epoch()));
    fields.emplace_back("treasurySpendTotalRawUnits",
                        std::to_string(report.treasurySpendTotal().rawUnits()));
    fields.emplace_back("spendRecordCount",          std::to_string(report.spendRecordCount()));
    fields.emplace_back("spendRecordsDigest",
                        report.spendRecordsDigest().empty() ? "none" : report.spendRecordsDigest());

    return serialization::KeyValueFileCodec::serialize(kSchemaId, fields);
}

economics::EpochTreasuryReport EpochTreasuryReportStore::decode(
    const std::string& contents
) {
    const serialization::KeyValueFileDocument doc =
        serialization::KeyValueFileCodec::parse(contents, kSchemaId);

    doc.requireOnlyFields(kAllowedFields);

    const std::uint64_t epoch      = requireUint64(doc, "epoch");
    const std::int64_t  totalRaw   = requireInt64(doc,  "treasurySpendTotalRawUnits");
    const std::uint64_t count      = requireUint64(doc, "spendRecordCount");

    if (totalRaw < 0) {
        throw std::runtime_error(
            "EpochTreasuryReportStore: treasurySpendTotalRawUnits must not be negative"
        );
    }
    if (count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(
            "EpochTreasuryReportStore: spendRecordCount exceeds size_t range"
        );
    }

    // spendRecordsDigest is optional for backwards compatibility with reports
    // written before the digest field was introduced.
    std::string digest;
    try {
        const std::string& raw = doc.requireField("spendRecordsDigest");
        digest = (raw == "none") ? "" : raw;
    } catch (...) {
        // Field absent in old format — leave digest empty.
    }

    const auto report = economics::EpochTreasuryReport::fromStoredFields(
        epoch,
        utils::Amount::fromRawUnits(totalRaw),
        static_cast<std::size_t>(count),
        std::move(digest)
    );

    if (!report.isValid()) {
        throw std::runtime_error(
            "EpochTreasuryReportStore: decoded report is invalid: " +
            report.rejectionReason()
        );
    }

    return report;
}

void EpochTreasuryReportStore::write(
    const std::filesystem::path& filePath,
    const economics::EpochTreasuryReport& report
) {
    writeFile(filePath, encode(report));
}

economics::EpochTreasuryReport EpochTreasuryReportStore::read(
    const std::filesystem::path& filePath
) {
    return decode(readFile(filePath));
}

} // namespace nodo::node
