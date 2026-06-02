#include "node/GovernanceLifecycleCodec.hpp"

#include "economics/GovernanceDecisionBuilder.hpp"

#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

const std::string kSchemaId = "NODO_GOVERNANCE_LIFECYCLE";

std::uint64_t parseU64(
    const std::string& value,
    const std::string& field
) {
    try {
        return std::stoull(value);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "GovernanceLifecycleCodec: field '" + field +
            "' is not a valid uint64: " + value
        );
    }
}

std::int64_t parseI64(
    const std::string& value,
    const std::string& field
) {
    try {
        return std::stoll(value);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "GovernanceLifecycleCodec: field '" + field +
            "' is not a valid int64: " + value
        );
    }
}

bool parseBool(
    const std::string& value,
    const std::string& field
) {
    if (value == "1") {
        return true;
    }
    if (value == "0") {
        return false;
    }
    throw std::runtime_error(
        "GovernanceLifecycleCodec: field '" + field +
        "' is not a valid bool: " + value
    );
}

void addTreasuryProposalAllowed(
    const std::string& prefix,
    std::set<std::string>& allowed
) {
    allowed.insert(prefix + "proposalId");
    allowed.insert(prefix + "recipientAddress");
    allowed.insert(prefix + "amountRawUnits");
    allowed.insert(prefix + "purpose");
    allowed.insert(prefix + "createdAtBlock");
    allowed.insert(prefix + "requestedEpoch");
    allowed.insert(prefix + "proposer");
}

void appendTreasuryProposalFields(
    const economics::TreasuryProposal& proposal,
    const std::string& prefix,
    GovernanceLifecycleCodec::FieldList& fields
) {
    fields.emplace_back(prefix + "proposalId", proposal.proposalId());
    fields.emplace_back(prefix + "recipientAddress", proposal.recipientAddress());
    fields.emplace_back(
        prefix + "amountRawUnits",
        std::to_string(proposal.amount().rawUnits())
    );
    fields.emplace_back(prefix + "purpose", proposal.purpose());
    fields.emplace_back(
        prefix + "createdAtBlock",
        std::to_string(proposal.createdAtBlock())
    );
    fields.emplace_back(
        prefix + "requestedEpoch",
        std::to_string(proposal.requestedEpoch())
    );
    fields.emplace_back(prefix + "proposer", proposal.proposer());
}

economics::TreasuryProposal decodeTreasuryProposal(
    const serialization::KeyValueFileDocument& doc,
    const std::string& prefix
) {
    return economics::TreasuryProposal(
        doc.requireField(prefix + "proposalId"),
        doc.requireField(prefix + "recipientAddress"),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(prefix + "amountRawUnits"),
            prefix + "amountRawUnits"
        )),
        doc.requireField(prefix + "purpose"),
        parseU64(
            doc.requireField(prefix + "createdAtBlock"),
            prefix + "createdAtBlock"
        ),
        parseU64(
            doc.requireField(prefix + "requestedEpoch"),
            prefix + "requestedEpoch"
        ),
        doc.requireField(prefix + "proposer")
    );
}

void addLifecycleAllowed(
    const serialization::KeyValueFileDocument& doc,
    const std::string& prefix,
    std::set<std::string>& allowed
) {
    allowed.insert(prefix + "lifecycleId");
    allowed.insert(prefix + "createdAtBlock");
    allowed.insert(prefix + "finalizedAtBlock");

    const std::string gp = prefix + "governancePolicy.";
    allowed.insert(gp + "policyVersion");
    allowed.insert(gp + "reviewPeriodBlocks");
    allowed.insert(gp + "decisionTimelockBlocks");
    allowed.insert(gp + "requireDecisionProof");
    allowed.insert(gp + "allowEmergencyApproval");

    const std::string vp = prefix + "votingPolicy.";
    allowed.insert(vp + "policyVersion");
    allowed.insert(vp + "quorumThresholdPower");
    allowed.insert(vp + "approvalThresholdPower");
    allowed.insert(vp + "allowAbstain");
    allowed.insert(vp + "requireVoteProof");

    const std::string ep = prefix + "proposalEnvelope.";
    allowed.insert(ep + "governanceProposalId");
    allowed.insert(ep + "proposalType");
    allowed.insert(ep + "submittedAtBlock");
    allowed.insert(ep + "submittedBy");
    allowed.insert(ep + "governancePolicyVersion");
    allowed.insert(ep + "summaryHash");
    addTreasuryProposalAllowed(ep + "treasuryProposal.", allowed);

    allowed.insert(prefix + "voteCount");
    const std::size_t voteCount = static_cast<std::size_t>(
        parseU64(doc.requireField(prefix + "voteCount"), prefix + "voteCount")
    );
    for (std::size_t i = 0; i < voteCount; ++i) {
        const std::string votePrefix =
            prefix + "vote." + std::to_string(i) + ".";
        allowed.insert(votePrefix + "voteId");
        allowed.insert(votePrefix + "governanceProposalId");
        allowed.insert(votePrefix + "voterId");
        allowed.insert(votePrefix + "choice");
        allowed.insert(votePrefix + "votingPower");
        allowed.insert(votePrefix + "votedAtBlock");
        allowed.insert(votePrefix + "policyVersion");
        allowed.insert(votePrefix + "voteProof");
    }

    const std::string tp = prefix + "tally.";
    allowed.insert(tp + "governanceProposalId");
    allowed.insert(tp + "policyVersion");
    allowed.insert(tp + "totalVotingPower");
    allowed.insert(tp + "yesVotingPower");
    allowed.insert(tp + "noVotingPower");
    allowed.insert(tp + "abstainVotingPower");
    allowed.insert(tp + "yesVoteCount");
    allowed.insert(tp + "noVoteCount");
    allowed.insert(tp + "abstainVoteCount");
    allowed.insert(tp + "quorumMet");
    allowed.insert(tp + "approvalThresholdMet");
    allowed.insert(tp + "approved");
    allowed.insert(tp + "tallyProof");

    const std::string dp = prefix + "decision.";
    allowed.insert(dp + "decisionId");
    allowed.insert(dp + "governanceProposalId");
    allowed.insert(dp + "proposalType");
    allowed.insert(dp + "decisionStatus");
    allowed.insert(dp + "decidedAtBlock");
    allowed.insert(dp + "decisionMaker");
    allowed.insert(dp + "decisionProof");
    allowed.insert(dp + "policyVersion");
}

} // namespace

const std::string& GovernanceLifecycleCodec::schemaId() {
    return kSchemaId;
}

std::string GovernanceLifecycleCodec::encode(
    const economics::GovernanceLifecycleRecord& lifecycle
) {
    if (!lifecycle.isValid()) {
        throw std::invalid_argument(
            "GovernanceLifecycleCodec: cannot encode invalid lifecycle: " +
            lifecycle.rejectionReason()
        );
    }

    FieldList fields;
    appendFields(lifecycle, "", fields);
    return serialization::KeyValueFileCodec::serialize(kSchemaId, fields);
}

economics::GovernanceLifecycleRecord GovernanceLifecycleCodec::decode(
    const std::string& contents
) {
    const serialization::KeyValueFileDocument doc =
        serialization::KeyValueFileCodec::parse(contents, kSchemaId);

    std::set<std::string> allowed;
    addLifecycleAllowed(doc, "", allowed);
    doc.requireOnlyFields(allowed);

    economics::GovernanceLifecycleRecord lifecycle =
        decodeFromDocument(doc, "");

    if (!lifecycle.isValid()) {
        throw std::runtime_error(
            "GovernanceLifecycleCodec: decoded lifecycle is invalid: " +
            lifecycle.rejectionReason()
        );
    }

    if (contents != encode(lifecycle)) {
        throw std::runtime_error(
            "GovernanceLifecycleCodec: lifecycle file is not canonical."
        );
    }

    return lifecycle;
}

void GovernanceLifecycleCodec::appendFields(
    const economics::GovernanceLifecycleRecord& lifecycle,
    const std::string& prefix,
    FieldList& fields
) {
    if (!lifecycle.isValid()) {
        throw std::invalid_argument(
            "GovernanceLifecycleCodec: cannot append invalid lifecycle: " +
            lifecycle.rejectionReason()
        );
    }

    fields.emplace_back(prefix + "lifecycleId", lifecycle.lifecycleId());
    fields.emplace_back(
        prefix + "createdAtBlock",
        std::to_string(lifecycle.createdAtBlock())
    );
    fields.emplace_back(
        prefix + "finalizedAtBlock",
        std::to_string(lifecycle.finalizedAtBlock())
    );

    const economics::GovernancePolicy& gp = lifecycle.governancePolicy();
    const std::string gpp = prefix + "governancePolicy.";
    fields.emplace_back(gpp + "policyVersion", gp.policyVersion());
    fields.emplace_back(
        gpp + "reviewPeriodBlocks",
        std::to_string(gp.reviewPeriodBlocks())
    );
    fields.emplace_back(
        gpp + "decisionTimelockBlocks",
        std::to_string(gp.decisionTimelockBlocks())
    );
    fields.emplace_back(
        gpp + "requireDecisionProof",
        gp.requireDecisionProof() ? "1" : "0"
    );
    fields.emplace_back(
        gpp + "allowEmergencyApproval",
        gp.allowEmergencyApproval() ? "1" : "0"
    );

    const economics::GovernanceVotingPolicy& vp = lifecycle.votingPolicy();
    const std::string vpp = prefix + "votingPolicy.";
    fields.emplace_back(vpp + "policyVersion", vp.policyVersion());
    fields.emplace_back(
        vpp + "quorumThresholdPower",
        std::to_string(vp.quorumThresholdPower())
    );
    fields.emplace_back(
        vpp + "approvalThresholdPower",
        std::to_string(vp.approvalThresholdPower())
    );
    fields.emplace_back(vpp + "allowAbstain", vp.allowAbstain() ? "1" : "0");
    fields.emplace_back(
        vpp + "requireVoteProof",
        vp.requireVoteProof() ? "1" : "0"
    );

    const economics::GovernanceProposalEnvelope& envelope =
        lifecycle.proposalEnvelope();
    const std::string ep = prefix + "proposalEnvelope.";
    fields.emplace_back(ep + "governanceProposalId", envelope.governanceProposalId());
    fields.emplace_back(ep + "proposalType", envelope.proposalType());
    fields.emplace_back(
        ep + "submittedAtBlock",
        std::to_string(envelope.submittedAtBlock())
    );
    fields.emplace_back(ep + "submittedBy", envelope.submittedBy());
    fields.emplace_back(
        ep + "governancePolicyVersion",
        envelope.governancePolicyVersion()
    );
    fields.emplace_back(ep + "summaryHash", envelope.summaryHash());
    appendTreasuryProposalFields(
        envelope.treasuryProposal(),
        ep + "treasuryProposal.",
        fields
    );

    fields.emplace_back(
        prefix + "voteCount",
        std::to_string(lifecycle.voteEvidenceList().size())
    );
    for (std::size_t i = 0; i < lifecycle.voteEvidenceList().size(); ++i) {
        const economics::GovernanceVoteEvidence& evidence =
            lifecycle.voteEvidenceList()[i];
        const economics::GovernanceVoteRecord& vote = evidence.voteRecord();
        const std::string votePrefix =
            prefix + "vote." + std::to_string(i) + ".";
        fields.emplace_back(votePrefix + "voteId", vote.voteId());
        fields.emplace_back(
            votePrefix + "governanceProposalId",
            vote.governanceProposalId()
        );
        fields.emplace_back(votePrefix + "voterId", vote.voterId());
        fields.emplace_back(
            votePrefix + "choice",
            economics::governanceVoteChoiceToString(vote.choice())
        );
        fields.emplace_back(
            votePrefix + "votingPower",
            std::to_string(vote.votingPower())
        );
        fields.emplace_back(
            votePrefix + "votedAtBlock",
            std::to_string(vote.votedAtBlock())
        );
        fields.emplace_back(votePrefix + "policyVersion", vote.policyVersion());
        fields.emplace_back(votePrefix + "voteProof", evidence.voteProof());
    }

    const economics::GovernanceTallyReport& tally = lifecycle.tallyReport();
    const std::string tp = prefix + "tally.";
    fields.emplace_back(tp + "governanceProposalId", tally.governanceProposalId());
    fields.emplace_back(tp + "policyVersion", tally.policyVersion());
    fields.emplace_back(
        tp + "totalVotingPower",
        std::to_string(tally.totalVotingPower())
    );
    fields.emplace_back(
        tp + "yesVotingPower",
        std::to_string(tally.yesVotingPower())
    );
    fields.emplace_back(
        tp + "noVotingPower",
        std::to_string(tally.noVotingPower())
    );
    fields.emplace_back(
        tp + "abstainVotingPower",
        std::to_string(tally.abstainVotingPower())
    );
    fields.emplace_back(tp + "yesVoteCount", std::to_string(tally.yesVoteCount()));
    fields.emplace_back(tp + "noVoteCount", std::to_string(tally.noVoteCount()));
    fields.emplace_back(
        tp + "abstainVoteCount",
        std::to_string(tally.abstainVoteCount())
    );
    fields.emplace_back(tp + "quorumMet", tally.quorumMet() ? "1" : "0");
    fields.emplace_back(
        tp + "approvalThresholdMet",
        tally.approvalThresholdMet() ? "1" : "0"
    );
    fields.emplace_back(tp + "approved", tally.approved() ? "1" : "0");
    fields.emplace_back(tp + "tallyProof", tally.tallyProof());

    const economics::GovernanceDecisionRecord& decision =
        lifecycle.decisionRecord();
    const std::string dp = prefix + "decision.";
    fields.emplace_back(dp + "decisionId", decision.decisionId());
    fields.emplace_back(
        dp + "governanceProposalId",
        decision.governanceProposalId()
    );
    fields.emplace_back(dp + "proposalType", decision.proposalType());
    fields.emplace_back(
        dp + "decisionStatus",
        economics::governanceDecisionStatusToString(decision.decisionStatus())
    );
    fields.emplace_back(
        dp + "decidedAtBlock",
        std::to_string(decision.decidedAtBlock())
    );
    fields.emplace_back(dp + "decisionMaker", decision.decisionMaker());
    fields.emplace_back(dp + "decisionProof", decision.decisionProof());
    fields.emplace_back(dp + "policyVersion", decision.policyVersion());
}

void GovernanceLifecycleCodec::addAllowedFields(
    const serialization::KeyValueFileDocument& doc,
    const std::string& prefix,
    std::set<std::string>& allowed
) {
    addLifecycleAllowed(doc, prefix, allowed);
}

economics::GovernanceLifecycleRecord GovernanceLifecycleCodec::decodeFromDocument(
    const serialization::KeyValueFileDocument& doc,
    const std::string& prefix
) {
    const economics::GovernancePolicy governancePolicy(
        doc.requireField(prefix + "governancePolicy.policyVersion"),
        parseU64(
            doc.requireField(prefix + "governancePolicy.reviewPeriodBlocks"),
            prefix + "governancePolicy.reviewPeriodBlocks"
        ),
        parseU64(
            doc.requireField(prefix + "governancePolicy.decisionTimelockBlocks"),
            prefix + "governancePolicy.decisionTimelockBlocks"
        ),
        parseBool(
            doc.requireField(prefix + "governancePolicy.requireDecisionProof"),
            prefix + "governancePolicy.requireDecisionProof"
        ),
        parseBool(
            doc.requireField(prefix + "governancePolicy.allowEmergencyApproval"),
            prefix + "governancePolicy.allowEmergencyApproval"
        )
    );

    const economics::GovernanceVotingPolicy votingPolicy(
        doc.requireField(prefix + "votingPolicy.policyVersion"),
        parseU64(
            doc.requireField(prefix + "votingPolicy.quorumThresholdPower"),
            prefix + "votingPolicy.quorumThresholdPower"
        ),
        parseU64(
            doc.requireField(prefix + "votingPolicy.approvalThresholdPower"),
            prefix + "votingPolicy.approvalThresholdPower"
        ),
        parseBool(
            doc.requireField(prefix + "votingPolicy.allowAbstain"),
            prefix + "votingPolicy.allowAbstain"
        ),
        parseBool(
            doc.requireField(prefix + "votingPolicy.requireVoteProof"),
            prefix + "votingPolicy.requireVoteProof"
        )
    );

    const economics::GovernanceProposalEnvelope envelope(
        doc.requireField(prefix + "proposalEnvelope.governanceProposalId"),
        doc.requireField(prefix + "proposalEnvelope.proposalType"),
        decodeTreasuryProposal(doc, prefix + "proposalEnvelope.treasuryProposal."),
        parseU64(
            doc.requireField(prefix + "proposalEnvelope.submittedAtBlock"),
            prefix + "proposalEnvelope.submittedAtBlock"
        ),
        doc.requireField(prefix + "proposalEnvelope.submittedBy"),
        doc.requireField(prefix + "proposalEnvelope.governancePolicyVersion"),
        doc.requireField(prefix + "proposalEnvelope.summaryHash")
    );

    const std::size_t voteCount = static_cast<std::size_t>(
        parseU64(doc.requireField(prefix + "voteCount"), prefix + "voteCount")
    );
    std::vector<economics::GovernanceVoteEvidence> votes;
    votes.reserve(voteCount);
    for (std::size_t i = 0; i < voteCount; ++i) {
        const std::string votePrefix =
            prefix + "vote." + std::to_string(i) + ".";
        economics::GovernanceVoteChoice choice =
            economics::GovernanceVoteChoice::ABSTAIN;
        if (!economics::governanceVoteChoiceFromString(
                doc.requireField(votePrefix + "choice"),
                choice)) {
            throw std::runtime_error(
                "GovernanceLifecycleCodec: invalid vote choice at index " +
                std::to_string(i) + "."
            );
        }

        economics::GovernanceVoteRecord record(
            doc.requireField(votePrefix + "voteId"),
            doc.requireField(votePrefix + "governanceProposalId"),
            doc.requireField(votePrefix + "voterId"),
            choice,
            parseU64(
                doc.requireField(votePrefix + "votingPower"),
                votePrefix + "votingPower"
            ),
            parseU64(
                doc.requireField(votePrefix + "votedAtBlock"),
                votePrefix + "votedAtBlock"
            ),
            doc.requireField(votePrefix + "policyVersion")
        );
        votes.emplace_back(
            std::move(record),
            doc.requireField(votePrefix + "voteProof")
        );
    }

    const std::string tp = prefix + "tally.";
    economics::GovernanceTallyReport tally(
        doc.requireField(tp + "governanceProposalId"),
        doc.requireField(tp + "policyVersion"),
        parseU64(doc.requireField(tp + "totalVotingPower"), tp + "totalVotingPower"),
        parseU64(doc.requireField(tp + "yesVotingPower"), tp + "yesVotingPower"),
        parseU64(doc.requireField(tp + "noVotingPower"), tp + "noVotingPower"),
        parseU64(
            doc.requireField(tp + "abstainVotingPower"),
            tp + "abstainVotingPower"
        ),
        parseU64(doc.requireField(tp + "yesVoteCount"), tp + "yesVoteCount"),
        parseU64(doc.requireField(tp + "noVoteCount"), tp + "noVoteCount"),
        parseU64(
            doc.requireField(tp + "abstainVoteCount"),
            tp + "abstainVoteCount"
        ),
        parseBool(doc.requireField(tp + "quorumMet"), tp + "quorumMet"),
        parseBool(
            doc.requireField(tp + "approvalThresholdMet"),
            tp + "approvalThresholdMet"
        ),
        parseBool(doc.requireField(tp + "approved"), tp + "approved"),
        doc.requireField(tp + "tallyProof")
    );

    economics::GovernanceDecisionStatus decisionStatus =
        economics::GovernanceDecisionStatus::REJECTED;
    if (!economics::governanceDecisionStatusFromString(
            doc.requireField(prefix + "decision.decisionStatus"),
            decisionStatus)) {
        throw std::runtime_error(
            "GovernanceLifecycleCodec: invalid governance decision status."
        );
    }

    economics::GovernanceDecisionRecord decision(
        doc.requireField(prefix + "decision.decisionId"),
        doc.requireField(prefix + "decision.governanceProposalId"),
        doc.requireField(prefix + "decision.proposalType"),
        decisionStatus,
        parseU64(
            doc.requireField(prefix + "decision.decidedAtBlock"),
            prefix + "decision.decidedAtBlock"
        ),
        doc.requireField(prefix + "decision.decisionMaker"),
        doc.requireField(prefix + "decision.decisionProof"),
        doc.requireField(prefix + "decision.policyVersion")
    );

    return economics::GovernanceLifecycleRecord(
        doc.requireField(prefix + "lifecycleId"),
        std::move(envelope),
        std::move(governancePolicy),
        std::move(votingPolicy),
        std::move(votes),
        std::move(tally),
        std::move(decision),
        parseU64(doc.requireField(prefix + "createdAtBlock"),
                 prefix + "createdAtBlock"),
        parseU64(doc.requireField(prefix + "finalizedAtBlock"),
                 prefix + "finalizedAtBlock")
    );
}

} // namespace nodo::node
