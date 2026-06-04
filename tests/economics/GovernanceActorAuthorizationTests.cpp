#include "economics/GovernanceLifecycleTransitionAudit.hpp"

#include "economics/GovernanceLifecycleState.hpp"
#include "economics/GovernanceLifecycleTransition.hpp"
#include "economics/GovernanceTransitionProof.hpp"

#include <cassert>
#include <set>
#include <string>
#include <vector>

namespace {

using nodo::economics::GovernanceLifecycleState;
using nodo::economics::GovernanceLifecycleTransition;
using nodo::economics::GovernanceLifecycleTransitionAudit;
using nodo::economics::GovernanceTransitionProof;

const std::string kProposalId = "gov-prop-actor-001";
const std::string kPolicyVersion = "governance-v1";

GovernanceLifecycleTransition makeT(
    const std::string& id,
    GovernanceLifecycleState from,
    GovernanceLifecycleState to,
    std::uint64_t block,
    const std::string& actor,
    const std::string& reason = ""
) {
    const std::string proof = GovernanceTransitionProof::build(
        kProposalId, id, from, to, block, actor, kPolicyVersion
    );
    return GovernanceLifecycleTransition(
        id, kProposalId, from, to, block, actor, reason, proof, kPolicyVersion
    );
}

std::vector<GovernanceLifecycleTransition> singleTransitionHistory(
    const std::string& actor
) {
    using S = GovernanceLifecycleState;
    return {makeT("t-001", S::DRAFT, S::SUBMITTED, 5, actor)};
}

void testAuthorizedActorPasses() {
    const std::set<std::string> authorized = {"governance-node"};
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        singleTransitionHistory("governance-node"),
        kProposalId, kPolicyVersion,
        authorized
    );
    assert(result.accepted() && "Authorized actor must pass");
}

void testUnauthorizedActorFails() {
    const std::set<std::string> authorized = {"governance-node"};
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        singleTransitionHistory("unknown-actor"),
        kProposalId, kPolicyVersion,
        authorized
    );
    assert(!result.accepted() && "Unauthorized actor must fail");
}

void testEmptyAuthorizedSetSkipsActorCheck() {
    const std::set<std::string> empty;
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        singleTransitionHistory("any-actor"),
        kProposalId, kPolicyVersion,
        empty
    );
    assert(result.accepted() && "Empty authorized set must not enforce actor check");
}

void testMultipleAuthorizedActorsAllPass() {
    using S = GovernanceLifecycleState;
    const std::set<std::string> authorized = {"node-a", "node-b"};

    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5, "node-a"),
        makeT("t-002", S::SUBMITTED, S::REVIEW,    6, "node-b"),
    };

    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion, authorized
    );
    assert(result.accepted() && "All actors from authorized set must pass");
}

void testMixedActorsOneUnauthorizedFails() {
    using S = GovernanceLifecycleState;
    const std::set<std::string> authorized = {"node-a"};

    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5, "node-a"),
        makeT("t-002", S::SUBMITTED, S::REVIEW,    6, "node-b"),
    };

    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion, authorized
    );
    assert(!result.accepted() && "A single unauthorized actor must fail the whole audit");
}

void testCancelledTransitionRequiresAuthorizedActor() {
    using S = GovernanceLifecycleState;
    const std::set<std::string> authorized = {"governance-node"};

    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5, "governance-node"),
        makeT("t-002", S::SUBMITTED, S::CANCELLED, 6, "unauthorized-node", "test cancel"),
    };

    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion, authorized
    );
    assert(!result.accepted() && "Cancelled transition with unauthorized actor must fail");
}

} // namespace

int main() {
    testAuthorizedActorPasses();
    testUnauthorizedActorFails();
    testEmptyAuthorizedSetSkipsActorCheck();
    testMultipleAuthorizedActorsAllPass();
    testMixedActorsOneUnauthorizedFails();
    testCancelledTransitionRequiresAuthorizedActor();
    return 0;
}
