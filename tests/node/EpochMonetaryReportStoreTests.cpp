#include "node/EpochMonetaryReportStore.hpp"
#include "economics/EpochMonetaryReport.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"

#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

nodo::economics::MonetaryPolicy testPolicy() {
    return nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-report-store-test", nodo::utils::Amount::fromRawUnits(500000)
    );
}

nodo::economics::SupplyDelta makeBurnDelta(
    std::uint64_t h,
    const std::string& hash,
    nodo::utils::Amount supplyBefore,
    std::int64_t burnRaw
) {
    const nodo::economics::BurnRecord burn(
        "burn-" + hash, h, 0, "fee_pool",
        nodo::utils::Amount::fromRawUnits(burnRaw),
        "fee burn", nodo::economics::BurnType::FEE_BURN
    );
    return nodo::economics::SupplyDelta(
        h, hash, 0,
        supplyBefore,
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(burnRaw),
        nodo::utils::Amount::fromRawUnits(supplyBefore.rawUnits() - burnRaw),
        {}, {burn}
    );
}

nodo::economics::EpochMonetaryReport buildReport() {
    const auto policy = testPolicy();
    const auto s0 = policy.initialSupply();
    const auto s1 = nodo::utils::Amount::fromRawUnits(s0.rawUnits() - 100);
    std::vector<nodo::economics::SupplyDelta> deltas = {
        makeBurnDelta(1, "hash-store-1", s0, 100),
        makeBurnDelta(2, "hash-store-2", s1, 50)
    };
    return nodo::economics::EpochMonetaryReport::fromDeltas(policy, 0, 1, 2, deltas);
}

std::filesystem::path tempPath() {
    return std::filesystem::temp_directory_path() /
           "nodo_report_store_test.txt";
}

// Schema id must be the versionless constant.
void testSchemaIdHasNoVersionSuffix() {
    const std::string& id = nodo::node::EpochMonetaryReportStore::schemaId();
    assert(id == "NODO_EPOCH_MONETARY_REPORT");
    // Must not end with _V<digit>
    assert(id.find("_V") == std::string::npos);
}

// Encode then decode round-trips all fields.
void testRoundTrip() {
    const auto policy = testPolicy();
    const auto report = buildReport();
    assert(report.isValid());

    const std::string encoded = nodo::node::EpochMonetaryReportStore::encode(report);
    assert(!encoded.empty());

    const auto decoded = nodo::node::EpochMonetaryReportStore::decode(encoded, policy);
    assert(decoded.isValid());
    assert(decoded.epoch()          == report.epoch());
    assert(decoded.startBlock()     == report.startBlock());
    assert(decoded.endBlock()       == report.endBlock());
    assert(decoded.startingSupply() == report.startingSupply());
    assert(decoded.endingSupply()   == report.endingSupply());
    assert(decoded.totalMinted()    == report.totalMinted());
    assert(decoded.totalBurned()    == report.totalBurned());
    assert(decoded.deltaCount()     == report.deltaCount());
    assert(decoded.mintRecordCount() == report.mintRecordCount());
    assert(decoded.burnRecordCount() == report.burnRecordCount());
    assert(decoded.policyVersion()  == report.policyVersion());
}

// Write to a file and read back.
void testWriteAndRead() {
    const auto policy = testPolicy();
    const auto report = buildReport();
    assert(report.isValid());

    const auto path = tempPath();
    nodo::node::EpochMonetaryReportStore::write(path, report);

    const auto loaded = nodo::node::EpochMonetaryReportStore::read(path, policy);
    assert(loaded.isValid());
    assert(loaded.totalBurned() == report.totalBurned());
    assert(loaded.endingSupply() == report.endingSupply());

    std::filesystem::remove(path);
}

// Encoding an invalid report throws.
void testEncodeInvalidReportThrows() {
    const nodo::economics::EpochMonetaryReport invalid;
    bool threw = false;
    try {
        nodo::node::EpochMonetaryReportStore::encode(invalid);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// Wrong schema id in file content causes parse failure.
void testWrongSchemaRejected() {
    const auto policy = testPolicy();
    const std::string bad = "NODO_EPOCH_MONETARY_REPORT_V1\n"
                            "epoch=0\n"
                            "startBlock=1\n";
    bool threw = false;
    try {
        nodo::node::EpochMonetaryReportStore::decode(bad, policy);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

// Tampered totalBurned fails arithmetic check.
void testTamperedBurnedRejected() {
    const auto policy = testPolicy();
    const auto report = buildReport();
    assert(report.isValid());

    std::string encoded = nodo::node::EpochMonetaryReportStore::encode(report);

    // Replace the totalBurnedRawUnits line with a fake value.
    const std::string target = "totalBurnedRawUnits=150";
    const std::string replacement = "totalBurnedRawUnits=999999";
    const auto pos = encoded.find(target);
    assert(pos != std::string::npos);
    encoded.replace(pos, target.size(), replacement);

    bool threw = false;
    try {
        nodo::node::EpochMonetaryReportStore::decode(encoded, policy);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

// Missing field causes decode failure.
void testMissingFieldRejected() {
    const auto policy = testPolicy();
    const auto report = buildReport();
    assert(report.isValid());

    std::string encoded = nodo::node::EpochMonetaryReportStore::encode(report);

    // Remove the policyVersion line.
    const auto pos = encoded.find("\npolicyVersion=");
    assert(pos != std::string::npos);
    const auto end = encoded.find('\n', pos + 1);
    encoded.erase(pos, (end == std::string::npos ? encoded.size() : end) - pos);

    bool threw = false;
    try {
        nodo::node::EpochMonetaryReportStore::decode(encoded, policy);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

} // namespace

int main() {
    testSchemaIdHasNoVersionSuffix();
    testRoundTrip();
    testWriteAndRead();
    testEncodeInvalidReportThrows();
    testWrongSchemaRejected();
    testTamperedBurnedRejected();
    testMissingFieldRejected();
    return 0;
}
