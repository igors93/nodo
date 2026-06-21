#include "node/GovernanceLifecycleCodec.hpp"

#include "economics/GovernanceDecisionBuilder.hpp"
#include "economics/GovernanceLifecycleState.hpp"
#include "economics/GovernanceTransitionProof.hpp"

#include <limits>
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
        if (value.empty()) {
            throw std::invalid_argument("empty");
        }
        for (const char c : value) {
            if (c < '0' || c > '9') {
                throw std::invalid_argument("malformed");
            }
        }
        std::size_t parsedCharacters = 0;
        const unsigned long long parsed = std::stoull(value, &parsedCharacters);
        if (parsedCharacters != value.size()) {
            throw std::invalid_argument("malformed");
        }
        return static_cast<std::uint64_t>(parsed);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "GovernanceLifecycleCodec: field '" + field +
            "' is not a valid uint64: " + value
        );
    }
}

std::uint32_t parseU32(
    const std::string& value,
    const std::string& field
) {
    const std::uint64_t parsed = parseU64(value, field);
    if (parsed > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(
            "GovernanceLifecycleCodec: field '" + field +
            "' exceeds uint32: " + value
        );
    }
    return static_cast<std::uint32_t>(parsed);
}

std::int64_t parseI64(
    const std::string& value,
    const std::string& field
) {
    try {
        if (value.empty()) {
            throw std::invalid_argument("empty");
        }
        for (std::size_t index = 0; index < value.size(); ++index) {
            const char c = value[index];
            if (c == '-' && index == 0 && value.size() > 1) {
                continue;
            }
            if (c < '0' || c > '9') {
                throw std::invalid_argument("malformed");
            }
        }
        std::size_t parsedCharacters = 0;
        const long long parsed = std::stoll(value, &parsedCharacters);
        if (parsedCharacters != value.size()) {
            throw std::invalid_argument("malformed");
        }
        return static_cast<std::int64_t>(parsed);
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
    allowed.insert(prefix + "declaredCurrentState");

    const std::string gp = prefix + "governancePolicy.";
    allowed.insert(gp + "policyVersion");
    allowed.insert(gp + "reviewPeriodBlocks");
    allowed.insert(gp + "decisionTimelockBlocks");
    allowed.insert(gp + "requireDecisionProof");
    allowed.insert(gp + "allowEmergencyApproval");

    const std::string vp = prefix + "votingPolicy.";
    allowed.insert(vp + "policyVersion");
    allowed.insert(vp + "quorumVotingPowerRawUnits");
    allowed.insert(vp + "approvalThresholdBasisPoints");
    allowed.insert(vp + "minimumVotingPowerRawUnits");
    allowed.insert(vp + "allowAbstain");
    allowed.insert(vp + "allowVoteReplacement");

    const std::string ep = prefix + "proposalEnvelope.";
    allowed.insert(ep + "governanceProposalId");
    allowed.insert(ep + "proposalType");
    allowed.insert(ep + "submittedAtBlock");
    allowed.insert(ep + "submittedBy");
    allowed.insert(ep + "governancePolicyVersion");
    allowed.insert(ep + "summaryHash");
    addTreasuryProposalAllowed(ep + "treasuryProposal.", allowed);

    // Transition history.
    allowed.insert(prefix + "transitionCount");
    const std::size_t transitionCount = static_cast<std::size_t>(
        parseU64(
            doc.requireField(prefix + "transitionCount"),
            prefix + "transitionCount"
        )
    );
    for (std::size_t i = 0; i < transitionCount; ++i) {
        const std::string tp = prefix + "transition." + std::to_string(i) + ".";
        allowed.insert(tp + "transitionId");
        allowed.insert(tp + "governanceProposalId");
        allowed.insert(tp + "fromState");
        allowed.insert(tp + "toState");
        allowed.insert(tp + "transitionBlock");
        allowed.insert(tp + "actorId");
        allowed.insert(tp + "reason");
        allowed.insert(tp + "transitionProof");
        allowed.insert(tp + "policyVersion");
    }

    allowed.insert(prefix + "voteCount");
    const std::size_t voteCount = static_cast<std::size_t>(
        parseU64(doc.requireField(prefix + "voteCount"), prefix + "voteCount")
    );
    for (std::size_t i = 0; i < voteCount; ++i) {
        const std::string votePrefix =
            prefix + "vote." + std::to_string(i) + ".";
        allowed.insert(votePrefix + "evidenceId");
        allowed.insert(votePrefix + "voteId");
        allowed.insert(votePrefix + "governanceProposalId");
        allowed.insert(votePrefix + "voterId");
        allowed.insert(votePrefix + "choice");
        allowed.insert(votePrefix + "votingPowerRawUnits");
        allowed.insert(votePrefix + "castAtBlock");
        allowed.insert(votePrefix + "votingPowerSource");
        allowed.insert(votePrefix + "policyVersion");
        allowed.insert(votePrefix + "voteProof");
    }

    const std::string tallyp = prefix + "tally.";
    allowed.insert(tallyp + "governanceProposalId");
    allowed.insert(tallyp + "policyVersion");
    allowed.insert(tallyp + "totalVotingPowerRawUnits");
    allowed.insert(tallyp + "yesVotingPowerRawUnits");
    allowed.insert(tallyp + "noVotingPowerRawUnits");
    allowed.insert(tallyp + "abstainVotingPowerRawUnits");
    allowed.insert(tallyp + "yesCount");
    allowed.insert(tallyp + "noCount");
    allowed.insert(tallyp + "abstainCount");
    allowed.insert(tallyp + "quorumMet");
    allowed.insert(tallyp + "approvalThresholdMet");
    allowed.insert(tallyp + "approved");
    allowed.insert(tallyp + "tallyProof");

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
    fields.emplace_back(
        prefix + "declaredCurrentState",
        economics::governanceLifecycleStateToString(lifecycle.declaredCurrentState())
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
        vpp + "quorumVotingPowerRawUnits",
        std::to_string(vp.quorumVotingPower().rawUnits())
    );
    fields.emplace_back(
        vpp + "approvalThresholdBasisPoints",
        std::to_string(vp.approvalThresholdBasisPoints())
    );
    fields.emplace_back(
        vpp + "minimumVotingPowerRawUnits",
        std::to_string(vp.minimumVotingPower().rawUnits())
    );
    fields.emplace_back(vpp + "allowAbstain", vp.allowAbstain() ? "1" : "0");
    fields.emplace_back(
        vpp + "allowVoteReplacement",
        vp.allowVoteReplacement() ? "1" : "0"
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

    // Transition history.
    const auto& transitions = lifecycle.transitionHistory();
    fields.emplace_back(
        prefix + "transitionCount",
        std::to_string(transitions.size())
    );
    for (std::size_t i = 0; i < transitions.size(); ++i) {
        const economics::GovernanceLifecycleTransition& t = transitions[i];
        const std::string tp = prefix + "transition." + std::to_string(i) + ".";
        fields.emplace_back(tp + "transitionId", t.transitionId());
        fields.emplace_back(tp + "governanceProposalId", t.governanceProposalId());
        fields.emplace_back(
            tp + "fromState",
            economics::governanceLifecycleStateToString(t.fromState())
        );
        fields.emplace_back(
            tp + "toState",
            economics::governanceLifecycleStateToString(t.toState())
        );
        fields.emplace_back(
            tp + "transitionBlock",
            std::to_string(t.transitionBlock())
        );
        fields.emplace_back(tp + "actorId", t.actorId());
        // Empty reason is encoded as "-" because the codec rejects empty field values.
        fields.emplace_back(tp + "reason", t.reason().empty() ? "-" : t.reason());
        fields.emplace_back(tp + "transitionProof", t.transitionProof());
        fields.emplace_back(tp + "policyVersion", t.policyVersion());
    }

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
        fields.emplace_back(votePrefix + "evidenceId", evidence.evidenceId());
        fields.emplace_back(votePrefix + "voteId", vote.voteId());
        fields.emplace_back(
            votePrefix + "governanceProposalId",
            vote.governanceProposalId()
        );
        fields.emplace_back(votePrefix + "voterId", vote.voterId());
        fields.emplace_back(
            votePrefix + "choice",
            economics::governanceVoteChoiceToString(vote.voteChoice())
        );
        fields.emplace_back(
            votePrefix + "votingPowerRawUnits",
            std::to_string(vote.votingPower().rawUnits())
        );
        fields.emplace_back(
            votePrefix + "castAtBlock",
            std::to_string(vote.castAtBlock())
        );
        fields.emplace_back(
            votePrefix + "votingPowerSource",
            vote.votingPowerSource()
        );
        fields.emplace_back(votePrefix + "policyVersion", vote.policyVersion());
        fields.emplace_back(votePrefix + "voteProof", vote.voteProof());
    }

    const economics::GovernanceTallyReport& tally = lifecycle.tallyReport();
    const std::string tallyp = prefix + "tally.";
    fields.emplace_back(tallyp + "governanceProposalId", tally.governanceProposalId());
    fields.emplace_back(tallyp + "policyVersion", tally.policyVersion());
    fields.emplace_back(
        tallyp + "totalVotingPowerRawUnits",
        std::to_string(tally.totalVotingPower().rawUnits())
    );
    fields.emplace_back(
        tallyp + "yesVotingPowerRawUnits",
        std::to_string(tally.yesVotingPower().rawUnits())
    );
    fields.emplace_back(
        tallyp + "noVotingPowerRawUnits",
        std::to_string(tally.noVotingPower().rawUnits())
    );
    fields.emplace_back(
        tallyp + "abstainVotingPowerRawUnits",
        std::to_string(tally.abstainVotingPower().rawUnits())
    );
    fields.emplace_back(tallyp + "yesCount", std::to_string(tally.yesCount()));
    fields.emplace_back(tallyp + "noCount", std::to_string(tally.noCount()));
    fields.emplace_back(tallyp + "abstainCount", std::to_string(tally.abstainCount()));
    fields.emplace_back(tallyp + "quorumMet", tally.quorumMet() ? "1" : "0");
    fields.emplace_back(
        tallyp + "approvalThresholdMet",
        tally.approvalThresholdMet() ? "1" : "0"
    );
    fields.emplace_back(tallyp + "approved", tally.approved() ? "1" : "0");
    fields.emplace_back(tallyp + "tallyProof", tally.tallyProof());

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
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(prefix + "votingPolicy.quorumVotingPowerRawUnits"),
            prefix + "votingPolicy.quorumVotingPowerRawUnits"
        )),
        parseU32(
            doc.requireField(prefix + "votingPolicy.approvalThresholdBasisPoints"),
            prefix + "votingPolicy.approvalThresholdBasisPoints"
        ),
        utils::Amount::fromRawUnits(parseI64(
            doc.requireField(prefix + "votingPolicy.minimumVotingPowerRawUnits"),
            prefix + "votingPolicy.minimumVotingPowerRawUnits"
        )),
        parseBool(
            doc.requireField(prefix + "votingPolicy.allowAbstain"),
            prefix + "votingPolicy.allowAbstain"
        ),
        parseBool(
            doc.requireField(prefix + "votingPolicy.allowVoteReplacement"),
            prefix + "votingPolicy.allowVoteReplacement"
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

    // Decode declared current state.
    economics::GovernanceLifecycleState declaredCurrentState =
        economics::GovernanceLifecycleState::DRAFT;
    if (!economics::governanceLifecycleStateFromString(
            doc.requireField(prefix + "declaredCurrentState"),
            declaredCurrentState)) {
        throw std::runtime_error(
            "GovernanceLifecycleCodec: invalid declaredCurrentState."
        );
    }

    // Decode transition history.
    const std::size_t transitionCount = static_cast<std::size_t>(
        parseU64(
            doc.requireField(prefix + "transitionCount"),
            prefix + "transitionCount"
        )
    );
    std::vector<economics::GovernanceLifecycleTransition> transitions;
    transitions.reserve(transitionCount);
    for (std::size_t i = 0; i < transitionCount; ++i) {
        const std::string tp = prefix + "transition." + std::to_string(i) + ".";

        economics::GovernanceLifecycleState fromState =
            economics::GovernanceLifecycleState::DRAFT;
        if (!economics::governanceLifecycleStateFromString(
                doc.requireField(tp + "fromState"),
                fromState)) {
            throw std::runtime_error(
                "GovernanceLifecycleCodec: invalid fromState at transition[" +
                std::to_string(i) + "]."
            );
        }

        economics::GovernanceLifecycleState toState =
            economics::GovernanceLifecycleState::DRAFT;
        if (!economics::governanceLifecycleStateFromString(
                doc.requireField(tp + "toState"),
                toState)) {
            throw std::runtime_error(
                "GovernanceLifecycleCodec: invalid toState at transition[" +
                std::to_string(i) + "]."
            );
        }

        const std::string reasonRaw = doc.requireField(tp + "reason");
        const std::string reason = (reasonRaw == "-") ? "" : reasonRaw;
        transitions.emplace_back(
            doc.requireField(tp + "transitionId"),
            doc.requireField(tp + "governanceProposalId"),
            fromState,
            toState,
            parseU64(doc.requireField(tp + "transitionBlock"), tp + "transitionBlock"),
            doc.requireField(tp + "actorId"),
            reason,
            doc.requireField(tp + "transitionProof"),
            doc.requireField(tp + "policyVersion")
        );
    }

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
            utils::Amount::fromRawUnits(parseI64(
                doc.requireField(votePrefix + "votingPowerRawUnits"),
                votePrefix + "votingPowerRawUnits"
            )),
            parseU64(
                doc.requireField(votePrefix + "castAtBlock"),
                votePrefix + "castAtBlock"
            ),
            doc.requireField(votePrefix + "votingPowerSource"),
            doc.requireField(votePrefix + "voteProof"),
            doc.requireField(votePrefix + "policyVersion")
        );
        votes.emplace_back(
            doc.requireField(votePrefix + "evidenceId"),
            envelope,
            votingPolicy,
            std::move(record)
        );
    }

    const std::string tallyp = prefix + "tally.";
    economics::GovernanceTallyReport tally(
        doc.requireField(tallyp + "governanceProposalId"),
        doc.requireField(tallyp + "policyVersion"),
        parseU64(
            doc.requireField(tallyp + "totalVotingPowerRawUnits"),
            tallyp + "totalVotingPowerRawUnits"
        ),
        parseU64(
            doc.requireField(tallyp + "yesVotingPowerRawUnits"),
            tallyp + "yesVotingPowerRawUnits"
        ),
        parseU64(
            doc.requireField(tallyp + "noVotingPowerRawUnits"),
            tallyp + "noVotingPowerRawUnits"
        ),
        parseU64(
            doc.requireField(tallyp + "abstainVotingPowerRawUnits"),
            tallyp + "abstainVotingPowerRawUnits"
        ),
        parseU64(doc.requireField(tallyp + "yesCount"), tallyp + "yesCount"),
        parseU64(doc.requireField(tallyp + "noCount"), tallyp + "noCount"),
        parseU64(doc.requireField(tallyp + "abstainCount"), tallyp + "abstainCount"),
        parseBool(doc.requireField(tallyp + "quorumMet"), tallyp + "quorumMet"),
        parseBool(
            doc.requireField(tallyp + "approvalThresholdMet"),
            tallyp + "approvalThresholdMet"
        ),
        parseBool(doc.requireField(tallyp + "approved"), tallyp + "approved"),
        doc.requireField(tallyp + "tallyProof")
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
                 prefix + "finalizedAtBlock"),
        declaredCurrentState,
        std::move(transitions)
    );
}

} // namespace nodo::node
