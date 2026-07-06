#ifndef NODO_NODE_EPOCH_TREASURY_REPORT_STORE_HPP
#define NODO_NODE_EPOCH_TREASURY_REPORT_STORE_HPP

#include "economics/EpochTreasuryReport.hpp"

#include <filesystem>
#include <string>

namespace nodo::node {

/*
 * EpochTreasuryReportStore persists and reloads EpochTreasuryReport data.
 *
 * Schema id: NODO_EPOCH_TREASURY_REPORT (versionless)
 *
 * Security principle:
 * A persisted treasury report must be verifiable against the canonical
 * TreasurySpendRecord sequence. The store enforces schema id, field presence,
 * and basic non-negative constraints on reload so that tampered files are
 * detected before audit comparison.
 */
class EpochTreasuryReportStore {
public:
  static const std::string &schemaId();

  static void write(const std::filesystem::path &filePath,
                    const economics::EpochTreasuryReport &report);

  static economics::EpochTreasuryReport
  read(const std::filesystem::path &filePath);

  static std::string encode(const economics::EpochTreasuryReport &report);

  static economics::EpochTreasuryReport decode(const std::string &contents);
};

} // namespace nodo::node

#endif
