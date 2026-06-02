#include "economics/GovernanceVoteProof.hpp"

#include "utils/Amount.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace {

using nodo::economics::GovernanceVoteChoice;
using nodo::economics::GovernanceVoteProof;
using nodo::economics::GovernanceVoteRecord;
using nodo::utils::Amount;

GovernanceVoteRecord makeRecord(
    GovernanceVoteChoice choice = GovernanceVoteChoice::YES,
    const std::string& voterId = "validator-a",
    std::int64_t power = 100,
    const std::string& policyVersion = "governance-v1",
    const std::string& proofOverride = ""
) {
    const std::string proof = proofOverride.empty()
        ? GovernanceVoteProof::build(
              "gov-prop-001",
              voterId,
              choice,
              Amount::fromRawUnits(power),
              12,
              policyVersion)
        : proofOverride;
    return GovernanceVoteRecord(
        "vote-001",
        "gov-prop-001",
        voterId,
        choice,
        Amount::fromRawUnits(power),
        12,
        "validator-stake",
        proof,
        policyVersion
    );
}

void testSameInputsProduceSameProof() {
    const std::string first = GovernanceVoteProof::buildFromRecord(makeRecord());
    const std::string second = GovernanceVoteProof::buildFromRecord(makeRecord());
    assert(first == second);
    assert(first.find("governance-vote-v1:") == 0);
}

void testDifferentFieldsChangeProof() {
    const std::string base = GovernanceVoteProof::buildFromRecord(makeRecord());
    assert(base != GovernanceVoteProof::buildFromRecord(
                       makeRecord(GovernanceVoteChoice::NO)));
    assert(base != GovernanceVoteProof::buildFromRecord(
                       makeRecord(GovernanceVoteChoice::YES, "validator-b")));
    assert(base != GovernanceVoteProof::buildFromRecord(
                       makeRecord(GovernanceVoteChoice::YES, "validator-a", 101)));
    assert(base != GovernanceVoteProof::buildFromRecord(
                       makeRecord(
                           GovernanceVoteChoice::YES,
                           "validator-a",
                           100,
                           "governance-v2")));
}

void testVerifyAcceptsCorrectProof() {
    assert(GovernanceVoteProof::verify(makeRecord()));
}

void testVerifyRejectsForgedProof() {
    assert(!GovernanceVoteProof::verify(
        makeRecord(
            GovernanceVoteChoice::YES,
            "validator-a",
            100,
            "governance-v1",
            "forged-proof"
        )
    ));
}

} // namespace

int main() {
    testSameInputsProduceSameProof();
    testDifferentFieldsChangeProof();
    testVerifyAcceptsCorrectProof();
    testVerifyRejectsForgedProof();
    return 0;
}
