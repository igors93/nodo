#ifndef NODO_NODE_EPOCH_MONETARY_REPORT_STORE_HPP
#define NODO_NODE_EPOCH_MONETARY_REPORT_STORE_HPP

#include "economics/EpochMonetaryReport.hpp"

#include <filesystem>
#include <string>

namespace nodo::node {

/*
 * EpochMonetaryReportStore persists and reloads EpochMonetaryReport data.
 *
 * Security principle:
 * A persisted report must be verifiable against finalized SupplyDeltas. The
 * store enforces the schema id and rejects malformed or missing fields so that
 * a tampered file is detected at load time before any audit comparison runs.
 */
class EpochMonetaryReportStore {
public:
    static const std::string& schemaId();

    static void write(
        const std::filesystem::path& filePath,
        const economics::EpochMonetaryReport& report
    );

    static economics::EpochMonetaryReport read(
        const std::filesystem::path& filePath,
        const economics::MonetaryPolicy& policy
    );

    static std::string encode(
        const economics::EpochMonetaryReport& report
    );

    static economics::EpochMonetaryReport decode(
        const std::string& contents,
        const economics::MonetaryPolicy& policy
    );
};

} // namespace nodo::node

#endif
