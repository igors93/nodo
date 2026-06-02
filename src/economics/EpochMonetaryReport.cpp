#include "economics/EpochMonetaryReport.hpp"

#include "economics/SupplyAudit.hpp"

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
      m_mintRecordCount(0),
      m_burnRecordCount(0),
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

    // For an empty sequence use the policy's initial supply as the baseline.
    report.m_startingSupply = policy.initialSupply();

    const SupplySequenceAuditResult supplyAudit =
        SupplyAudit::auditDeltaSequence(policy, deltas);

    if (!supplyAudit.isValid()) {
        report.m_rejectionReason = "EpochMonetaryReport: " +
                                    supplyAudit.reason();
        return report;
    }

    report.m_endingSupply = supplyAudit.finalSupply();

    if (deltas.empty()) {
        report.m_valid = true;
        report.m_rejectionReason = "";
        return report;
    }

    // For a non-empty validated sequence, startingSupply comes from the first
    // delta's supplyBefore, not from the policy constant. This is the canonical
    // supply at the start of the reporting window.
    report.m_startingSupply = deltas.front().supplyBefore();

    std::int64_t totalMintedRaw = 0;
    std::int64_t totalBurnedRaw = 0;

    for (const auto& delta : deltas) {
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
        report.m_mintRecordCount += delta.mintRecords().size();
        report.m_burnRecordCount += delta.burnRecords().size();
    }

    report.m_totalMinted = utils::Amount::fromRawUnits(totalMintedRaw);
    report.m_totalBurned = utils::Amount::fromRawUnits(totalBurnedRaw);
    report.m_valid = true;
    report.m_rejectionReason = "";
    return report;
}

EpochMonetaryReport EpochMonetaryReport::fromStoredFields(
    const MonetaryPolicy& policy,
    const std::string& storedPolicyVersion,
    std::uint64_t epoch,
    std::uint64_t startBlock,
    std::uint64_t endBlock,
    utils::Amount startingSupply,
    utils::Amount endingSupply,
    utils::Amount totalMinted,
    utils::Amount totalBurned,
    std::size_t deltaCount,
    std::size_t mintRecordCount,
    std::size_t burnRecordCount
) {
    EpochMonetaryReport report;

    if (!policy.isValid()) {
        report.m_rejectionReason = "EpochMonetaryReport::fromStoredFields: invalid policy: " +
                                    policy.rejectionReason();
        return report;
    }

    if (storedPolicyVersion != policy.policyVersion()) {
        report.m_rejectionReason =
            "EpochMonetaryReport::fromStoredFields: policyVersion mismatch: "
            "stored=" + storedPolicyVersion +
            " expected=" + policy.policyVersion();
        return report;
    }

    if (startBlock > endBlock) {
        report.m_rejectionReason =
            "EpochMonetaryReport::fromStoredFields: startBlock (" +
            std::to_string(startBlock) + ") > endBlock (" +
            std::to_string(endBlock) + ").";
        return report;
    }

    // startingSupply + totalMinted - totalBurned must equal endingSupply.
    const std::int64_t computedEnding =
        startingSupply.rawUnits() + totalMinted.rawUnits() - totalBurned.rawUnits();
    if (computedEnding != endingSupply.rawUnits()) {
        report.m_rejectionReason =
            "EpochMonetaryReport::fromStoredFields: arithmetic mismatch: "
            "starting(" + std::to_string(startingSupply.rawUnits()) +
            ") + minted(" + std::to_string(totalMinted.rawUnits()) +
            ") - burned(" + std::to_string(totalBurned.rawUnits()) +
            ") = " + std::to_string(computedEnding) +
            " != ending(" + std::to_string(endingSupply.rawUnits()) + ").";
        return report;
    }

    report.m_epoch           = epoch;
    report.m_startBlock      = startBlock;
    report.m_endBlock        = endBlock;
    report.m_startingSupply  = startingSupply;
    report.m_endingSupply    = endingSupply;
    report.m_totalMinted     = totalMinted;
    report.m_totalBurned     = totalBurned;
    report.m_deltaCount      = deltaCount;
    report.m_mintRecordCount = mintRecordCount;
    report.m_burnRecordCount = burnRecordCount;
    report.m_policyVersion   = policy.policyVersion();
    report.m_valid           = true;
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
std::size_t EpochMonetaryReport::mintRecordCount() const { return m_mintRecordCount; }
std::size_t EpochMonetaryReport::burnRecordCount() const { return m_burnRecordCount; }
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
        << ";mintRecordCount=" << m_mintRecordCount
        << ";burnRecordCount=" << m_burnRecordCount
        << ";policyVersion=" << m_policyVersion
        << ";valid=" << (m_valid ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
