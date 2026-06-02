#include "node/FinalizedTreasurySectionCodec.hpp"

#include "economics/TreasurySpendRecord.hpp"
#include "node/GovernanceLifecycleCodec.hpp"
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

bool parseBool(const std::string& value, const std::string& field) {
    if (value == "1") return true;
    if (value == "0") return false;
    throw std::runtime_error(
        "FinalizedTreasurySectionCodec: field '" + field +
        "' is not a valid bool (expected 0 or 1): " + value
    );
}

// ---- Spend record codec helpers ----

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

// ---- Evidence codec helpers ----

economics::TreasuryExecutionEvidence decodeEvidence(
    const serialization::KeyValueFileDocument& doc,
    std::size_t index,
    const std::string& prefix
) {
    const std::string p = prefix + std::to_string(index) + ".";

    const std::string evidenceId = doc.requireField(p + "evidenceId");
    const std::uint64_t blockHeight = parseU64(
        doc.requireField(p + "currentBlockHeight"), p + "currentBlockHeight");
    const std::int64_t epochSpentRaw = parseI64(
        doc.requireField(p + "epochSpentSoFarRawUnits"), p + "epochSpentSoFarRawUnits");
    const std::int64_t createdAt = parseI64(
        doc.requireField(p + "createdAt"), p + "createdAt");

    // Proposal fields.
    const std::string pp = p + "proposal.";
    economics::TreasuryProposal proposal(
        doc.requireField(pp + "proposalId"),
        doc.requireField(pp + "recipientAddress"),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(pp + "amountRawUnits"), pp + "amountRawUnits")),
        doc.requireField(pp + "purpose"),
        parseU64(doc.requireField(pp + "createdAtBlock"), pp + "createdAtBlock"),
        parseU64(doc.requireField(pp + "requestedEpoch"), pp + "requestedEpoch"),
        doc.requireField(pp + "proposer")
    );

    // Approval fields.
    const std::string ap = p + "approval.";
    economics::TreasuryApproval approval(
        doc.requireField(ap + "approvalId"),
        doc.requireField(ap + "proposalId"),
        parseU64(doc.requireField(ap + "approvedAtBlock"), ap + "approvedAtBlock"),
        doc.requireField(ap + "approver"),
        doc.requireField(ap + "approvalProof")
    );

    // Policy fields.
    const std::string polp = p + "policy.";
    economics::TreasuryPolicy policy(
        doc.requireField(polp + "policyVersion"),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(polp + "maxSpendPerEpochRawUnits"),
            polp + "maxSpendPerEpochRawUnits")),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(polp + "maxSpendPerProposalRawUnits"),
            polp + "maxSpendPerProposalRawUnits")),
        parseU64(doc.requireField(polp + "timelockBlocks"), polp + "timelockBlocks"),
        parseBool(doc.requireField(polp + "requireApproval"), polp + "requireApproval"),
        parseBool(doc.requireField(polp + "allowSpendingWhenLocked"),
                  polp + "allowSpendingWhenLocked")
    );

    // Treasury account before.
    const std::string tap = p + "treasuryAccountBefore.";
    economics::TreasuryAccount treasuryAccountBefore(
        doc.requireField(tap + "treasuryId"),
        doc.requireField(tap + "accountAddress"),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(tap + "balanceRawUnits"), tap + "balanceRawUnits")),
        parseU64(doc.requireField(tap + "epoch"), tap + "epoch"),
        parseBool(doc.requireField(tap + "locked"), tap + "locked"),
        doc.requireField(tap + "lockReason")
    );

    // Spend record: decode sub-fields with explicit spend sub-path.
    const std::string sp = p + "spend.";
    economics::TreasurySpendRecord spend(
        doc.requireField(sp + "spendId"),
        doc.requireField(sp + "proposalId"),
        doc.requireField(sp + "recipientAddress"),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(sp + "amountRawUnits"), sp + "amountRawUnits")),
        doc.requireField(sp + "purpose"),
        parseU64(doc.requireField(sp + "executedAtBlock"), sp + "executedAtBlock"),
        parseU64(doc.requireField(sp + "epoch"), sp + "epoch"),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(sp + "balanceBeforeRawUnits"), sp + "balanceBeforeRawUnits")),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(sp + "balanceAfterRawUnits"), sp + "balanceAfterRawUnits"))
    );

    economics::GovernanceApprovalContext governanceContext;
    governanceContext.governanceLifecycle =
        GovernanceLifecycleCodec::decodeFromDocument(
            doc,
            p + "governanceLifecycle."
        );

    economics::TreasuryExecutionEvidence evidence(
        evidenceId,
        std::move(proposal),
        std::move(approval),
        std::move(policy),
        std::move(treasuryAccountBefore),
        blockHeight,
        utils::Amount::fromRawUnits(epochSpentRaw),
        std::move(spend),
        createdAt,
        std::move(governanceContext)
    );

    if (!evidence.isValid()) {
        throw std::runtime_error(
            "FinalizedTreasurySectionCodec: decoded evidence at index " +
            std::to_string(index) + " is invalid: " + evidence.rejectionReason()
        );
    }
    return evidence;
}

void addEvidenceFields(
    const serialization::KeyValueFileDocument& doc,
    std::set<std::string>& allowed,
    std::size_t index,
    const std::string& prefix
) {
    const std::string p = prefix + std::to_string(index) + ".";
    allowed.insert(p + "evidenceId");
    allowed.insert(p + "currentBlockHeight");
    allowed.insert(p + "epochSpentSoFarRawUnits");
    allowed.insert(p + "createdAt");

    // Proposal sub-fields.
    const std::string pp = p + "proposal.";
    allowed.insert(pp + "proposalId");
    allowed.insert(pp + "recipientAddress");
    allowed.insert(pp + "amountRawUnits");
    allowed.insert(pp + "purpose");
    allowed.insert(pp + "createdAtBlock");
    allowed.insert(pp + "requestedEpoch");
    allowed.insert(pp + "proposer");

    // Approval sub-fields.
    const std::string ap = p + "approval.";
    allowed.insert(ap + "approvalId");
    allowed.insert(ap + "proposalId");
    allowed.insert(ap + "approvedAtBlock");
    allowed.insert(ap + "approver");
    allowed.insert(ap + "approvalProof");

    // Policy sub-fields.
    const std::string polp = p + "policy.";
    allowed.insert(polp + "policyVersion");
    allowed.insert(polp + "maxSpendPerEpochRawUnits");
    allowed.insert(polp + "maxSpendPerProposalRawUnits");
    allowed.insert(polp + "timelockBlocks");
    allowed.insert(polp + "requireApproval");
    allowed.insert(polp + "allowSpendingWhenLocked");

    // Treasury account sub-fields.
    const std::string tap = p + "treasuryAccountBefore.";
    allowed.insert(tap + "treasuryId");
    allowed.insert(tap + "accountAddress");
    allowed.insert(tap + "balanceRawUnits");
    allowed.insert(tap + "epoch");
    allowed.insert(tap + "locked");
    allowed.insert(tap + "lockReason");

    // Spend sub-fields.
    const std::string sp = p + "spend.";
    allowed.insert(sp + "spendId");
    allowed.insert(sp + "proposalId");
    allowed.insert(sp + "recipientAddress");
    allowed.insert(sp + "amountRawUnits");
    allowed.insert(sp + "purpose");
    allowed.insert(sp + "executedAtBlock");
    allowed.insert(sp + "epoch");
    allowed.insert(sp + "balanceBeforeRawUnits");
    allowed.insert(sp + "balanceAfterRawUnits");

    GovernanceLifecycleCodec::addAllowedFields(
        doc,
        p + "governanceLifecycle.",
        allowed
    );
}

void appendEvidenceFields(
    const economics::TreasuryExecutionEvidence& ev,
    std::size_t index,
    const std::string& prefix,
    std::vector<std::pair<std::string, std::string>>& fields
) {
    const std::string p = prefix + std::to_string(index) + ".";
    fields.emplace_back(p + "evidenceId", ev.evidenceId());
    fields.emplace_back(p + "currentBlockHeight",
                        std::to_string(ev.currentBlockHeight()));
    fields.emplace_back(p + "epochSpentSoFarRawUnits",
                        std::to_string(ev.epochSpentSoFar().rawUnits()));
    fields.emplace_back(p + "createdAt", std::to_string(ev.createdAt()));

    const std::string pp = p + "proposal.";
    fields.emplace_back(pp + "proposalId", ev.proposal().proposalId());
    fields.emplace_back(pp + "recipientAddress", ev.proposal().recipientAddress());
    fields.emplace_back(pp + "amountRawUnits",
                        std::to_string(ev.proposal().amount().rawUnits()));
    fields.emplace_back(pp + "purpose", ev.proposal().purpose());
    fields.emplace_back(pp + "createdAtBlock",
                        std::to_string(ev.proposal().createdAtBlock()));
    fields.emplace_back(pp + "requestedEpoch",
                        std::to_string(ev.proposal().requestedEpoch()));
    fields.emplace_back(pp + "proposer", ev.proposal().proposer());

    const std::string ap = p + "approval.";
    fields.emplace_back(ap + "approvalId", ev.approval().approvalId());
    fields.emplace_back(ap + "proposalId", ev.approval().proposalId());
    fields.emplace_back(ap + "approvedAtBlock",
                        std::to_string(ev.approval().approvedAtBlock()));
    fields.emplace_back(ap + "approver", ev.approval().approver());
    fields.emplace_back(ap + "approvalProof", ev.approval().approvalProof());

    const std::string polp = p + "policy.";
    fields.emplace_back(polp + "policyVersion", ev.policy().policyVersion());
    fields.emplace_back(polp + "maxSpendPerEpochRawUnits",
                        std::to_string(ev.policy().maxSpendPerEpoch().rawUnits()));
    fields.emplace_back(polp + "maxSpendPerProposalRawUnits",
                        std::to_string(ev.policy().maxSpendPerProposal().rawUnits()));
    fields.emplace_back(polp + "timelockBlocks",
                        std::to_string(ev.policy().timelockBlocks()));
    fields.emplace_back(polp + "requireApproval",
                        ev.policy().requireApproval() ? "1" : "0");
    fields.emplace_back(polp + "allowSpendingWhenLocked",
                        ev.policy().allowSpendingWhenLocked() ? "1" : "0");

    const std::string tap = p + "treasuryAccountBefore.";
    fields.emplace_back(tap + "treasuryId", ev.treasuryAccountBefore().treasuryId());
    fields.emplace_back(tap + "accountAddress",
                        ev.treasuryAccountBefore().accountAddress());
    fields.emplace_back(tap + "balanceRawUnits",
                        std::to_string(ev.treasuryAccountBefore().balance().rawUnits()));
    fields.emplace_back(tap + "epoch",
                        std::to_string(ev.treasuryAccountBefore().epoch()));
    fields.emplace_back(tap + "locked",
                        ev.treasuryAccountBefore().isLocked() ? "1" : "0");
    fields.emplace_back(tap + "lockReason",
                        ev.treasuryAccountBefore().lockReason());

    const std::string sp = p + "spend.";
    fields.emplace_back(sp + "spendId", ev.spendRecord().spendId());
    fields.emplace_back(sp + "proposalId", ev.spendRecord().proposalId());
    fields.emplace_back(sp + "recipientAddress", ev.spendRecord().recipientAddress());
    fields.emplace_back(sp + "amountRawUnits",
                        std::to_string(ev.spendRecord().amount().rawUnits()));
    fields.emplace_back(sp + "purpose", ev.spendRecord().purpose());
    fields.emplace_back(sp + "executedAtBlock",
                        std::to_string(ev.spendRecord().executedAtBlock()));
    fields.emplace_back(sp + "epoch", std::to_string(ev.spendRecord().epoch()));
    fields.emplace_back(sp + "balanceBeforeRawUnits",
                        std::to_string(ev.spendRecord().treasuryBalanceBefore().rawUnits()));
    fields.emplace_back(sp + "balanceAfterRawUnits",
                        std::to_string(ev.spendRecord().treasuryBalanceAfter().rawUnits()));

    if (!ev.hasGovernanceContext()) {
        throw std::invalid_argument(
            "FinalizedTreasurySectionCodec: evidence must carry governance lifecycle context."
        );
    }

    GovernanceLifecycleCodec::appendFields(
        ev.governanceContext().governanceLifecycle,
        p + "governanceLifecycle.",
        fields
    );
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

    if (section.hasEvidence()) {
        fields.emplace_back("evidenceCount",
                            std::to_string(section.evidenceCount()));
        for (std::size_t i = 0; i < section.executionEvidence().size(); ++i) {
            appendEvidenceFields(section.executionEvidence()[i], i, "evidence.", fields);
        }
    } else {
        // Legacy: encode only spend records (no evidence).
        fields.emplace_back("spendRecordCount",
                            std::to_string(section.spendRecordCount()));
        for (std::size_t i = 0; i < section.spendRecords().size(); ++i) {
            appendRecordFields(section.spendRecords()[i], i, "spend.", fields);
        }
    }

    return serialization::KeyValueFileCodec::serialize(kSchemaId, fields);
}

FinalizedTreasurySection FinalizedTreasurySectionCodec::decode(
    const std::string& contents
) {
    const serialization::KeyValueFileDocument doc =
        serialization::KeyValueFileCodec::parse(contents, kSchemaId);

    // Determine whether this is an evidence-based or legacy section.
    const bool hasEvidence = doc.hasField("evidenceCount");

    if (hasEvidence) {
        const std::size_t count = static_cast<std::size_t>(
            parseU64(doc.requireField("evidenceCount"), "evidenceCount")
        );

        std::set<std::string> allowed;
        allowed.insert("evidenceCount");
        for (std::size_t i = 0; i < count; ++i) {
            addEvidenceFields(doc, allowed, i, "evidence.");
        }
        doc.requireOnlyFields(allowed);

        std::vector<economics::TreasuryExecutionEvidence> evidence;
        evidence.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            evidence.push_back(decodeEvidence(doc, i, "evidence."));
        }
        return FinalizedTreasurySection(std::move(evidence));
    }

    // Legacy path: decode spend records only.
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
