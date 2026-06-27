#include "node/SlashingEvidenceMessages.hpp"

#include <cassert>
#include <iostream>

int main() {
    const nodo::consensus::SlashingEvidenceRecord record(
        "evidence-alpha",
        nodo::consensus::SlashingEvidenceType::DOUBLE_VOTE,
        "validator-alpha",
        "payload-hash-alpha",
        nodo::consensus::SlashingEvidenceSeverity::SLASHABLE,
        100
    );

    const nodo::node::SlashingEvidenceAnnouncement announcement(
        "localnet",
        "chain-alpha",
        "node-alpha",
        record,
        200
    );

    assert(announcement.isValid());
    assert(announcement.record().evidenceId() == "evidence-alpha");

    const nodo::node::SlashingEvidenceRequest request(
        "node-beta",
        "evidence-alpha",
        201
    );

    assert(request.isValid());
    assert(request.evidenceId() == "evidence-alpha");

    std::cout << "slashing evidence messages tests passed\n";
    return 0;
}
