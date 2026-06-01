#include "economics/EpochMonetaryReport.hpp"

#include <climits>
#include <sstream>
#include <utility>

namespace nodo::economics {

EpochMonetaryReport::EpochMonetaryReport()
    : m_epoch(0),
      m_startBlock(0),
      m_endBlock(0),
      m_startingSupply(utils::Amount::fromRawUnits(0)),
      m_endingSupply(utils::Amount::fromRawUnits(0)),
      m_totalMinted(utils::Amount::fromRawUnits(0)),
      m_totalBurned(utils::Amount::fromRawUnits(0)),
      m_deltaCount(0),
      m_policyVersion(""),
      m_valid(false),
      m_rejectionReason("EpochMonetaryReport: default-constructed.") {}

EpochMonetaryReport EpochMonetaryReport::fromDeltas(
    const MonetaryPolicy& policy,
    std::uint64_t epoch,
    std::uint64_t startBlock,
    std::uint64_t endBlock,
    const std::vector<SupplyDelta>& deltas
) {
    EpochMonetaryReport report;

    if (!policy.isValid()) {
        report.m_rejectionReason = "EpochMonetaryReport: invalid policy: " +
                                    policy.rejectionReason();
        return report;
    }

    if (startBlock > endBlock) {
        report.m_rejectionReason = "EpochMonetaryReport: startBlock (" +
                                    std::to_string(startBlock) + ") > endBlock (" +
                                    std::to_string(endBlock) + ").";
        return report;
    }

    report.m_epoch = epoch;
    report.m_startBlock = startBlock;
    report.m_endBlock = endBlock;
    report.m_policyVersion = policy.policyVersion();
    report.m_deltaCount = deltas.size();

    if (deltas.empty()) {
        report.m_valid = true;
        report.m_rejectionReason = "";
        return report;
    }

    // Validate that first delta is valid.
    if (!deltas.front().isValid()) {
        report.m_rejectionReason = "EpochMonetaryReport: first delta is invalid: " +
                                    deltas.front().rejectionReason();
        return report;
    }

    report.m_startingSupply = deltas.front().supplyBefore();

    std::int64_t totalMintedRaw = 0;
    std::int64_t totalBurnedRaw = 0;
    utils::Amount expectedSupply = report.m_startingSupply;

    for (const auto& delta : deltas) {
        if (!delta.isValid()) {
            report.m_rejectionReason = "EpochMonetaryReport: invalid delta at block " +
                                        std::to_string(delta.blockHeight()) + ": " +
                                        delta.rejectionReason();
            return report;
        }
        if (delta.supplyBefore() != expectedSupply) {
            report.m_rejectionReason = "EpochMonetaryReport: continuity break at block " +
                                        std::to_string(delta.blockHeight()) +
                                        " — expected supplyBefore=" +
                                        std::to_string(expectedSupply.rawUnits()) +
                                        " got=" +
                                        std::to_string(delta.supplyBefore().rawUnits()) + ".";
            return report;
        }
        // Overflow guard on accumulation.
        if (delta.mintedAmount().rawUnits() > 0 &&
            totalMintedRaw > INT64_MAX - delta.mintedAmount().rawUnits()) {
            report.m_rejectionReason = "EpochMonetaryReport: totalMinted overflow.";
            return report;
        }
        if (delta.burnedAmount().rawUnits() > 0 &&
            totalBurnedRaw > INT64_MAX - delta.burnedAmount().rawUnits()) {
            report.m_rejectionReason = "EpochMonetaryReport: totalBurned overflow.";
            return report;
        }
        totalMintedRaw += delta.mintedAmount().rawUnits();
        totalBurnedRaw += delta.burnedAmount().rawUnits();
        expectedSupply = delta.supplyAfter();
    }

    report.m_endingSupply = expectedSupply;
    report.m_totalMinted = utils::Amount::fromRawUnits(totalMintedRaw);
    report.m_totalBurned = utils::Amount::fromRawUnits(totalBurnedRaw);
    report.m_valid = true;
    report.m_rejectionReason = "";
    return report;
}

std::uint64_t EpochMonetaryReport::epoch() const { return m_epoch; }
std::uint64_t EpochMonetaryReport::startBlock() const { return m_startBlock; }
std::uint64_t EpochMonetaryReport::endBlock() const { return m_endBlock; }
utils::Amount EpochMonetaryReport::startingSupply() const { return m_startingSupply; }
utils::Amount EpochMonetaryReport::endingSupply() const { return m_endingSupply; }
utils::Amount EpochMonetaryReport::totalMinted() const { return m_totalMinted; }
utils::Amount EpochMonetaryReport::totalBurned() const { return m_totalBurned; }
std::size_t EpochMonetaryReport::deltaCount() const { return m_deltaCount; }
const std::string& EpochMonetaryReport::policyVersion() const { return m_policyVersion; }

bool EpochMonetaryReport::isValid() const { return m_valid; }
std::string EpochMonetaryReport::rejectionReason() const { return m_rejectionReason; }

std::string EpochMonetaryReport::serialize() const {
    std::ostringstream oss;
    oss << "EpochMonetaryReport{"
        << "epoch=" << m_epoch
        << ";startBlock=" << m_startBlock
        << ";endBlock=" << m_endBlock
        << ";startingSupplyRaw=" << m_startingSupply.rawUnits()
        << ";endingSupplyRaw=" << m_endingSupply.rawUnits()
        << ";totalMintedRaw=" << m_totalMinted.rawUnits()
        << ";totalBurnedRaw=" << m_totalBurned.rawUnits()
        << ";deltaCount=" << m_deltaCount
        << ";policyVersion=" << m_policyVersion
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
