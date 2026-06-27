#include "consensus/ValidatorPenaltyApplication.hpp"
#include "node/ValidatorPenaltyMessages.hpp"

#include <cassert>
#include <iostream>

using namespace nodo::consensus;
using namespace nodo::node;

namespace {

std::string hash64(char value) {
    return std::string(64, value);
}

ValidatorPenaltyDecision makeDecision() {
    const SlashingEvidenceRecord evidence(
        hash64('a'),
        SlashingEvidenceType::DOUBLE_VOTE,
        "validator-message",
        hash64('b'),
        SlashingEvidenceSeverity::SLASHABLE,
        1700000000
    );

    return ValidatorPenaltyDecision::create(
        evidence,
        ValidatorPenaltyPolicy::conservativeTestnetPolicy(),
        1800000000
    );
}

} // namespace

int main() {
    const auto decision = makeDecision();

    ValidatorPenaltyAnnouncement announcement(
        "localnet",
        "nodo-chain",
        "node-alpha",
        decision,
        1800000001
    );

    assert(announcement.isValid());
    assert(announcement.decision().penaltyId() == decision.penaltyId());
    assert(announcement.serialize().find("ValidatorPenaltyAnnouncement") != std::string::npos);

    ValidatorPenaltyRequest request(
        "node-beta",
        decision.penaltyId(),
        1800000002
    );

    assert(request.isValid());
    assert(request.penaltyId() == decision.penaltyId());
    assert(request.serialize().find("ValidatorPenaltyRequest") != std::string::npos);

    ValidatorPenaltyRequest invalidRequest(
        "node-beta",
        "bad;id",
        1800000002
    );
    assert(!invalidRequest.isValid());

    std::cout << "Validator penalty message tests passed.\n";
    return 0;
}
