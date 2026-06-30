#include "consensus/EvidencePool.hpp"
#include "consensus/SlashingEvidence.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {

nodo::consensus::ProposerEquivocationEvidence makeEvidence(
    const std::string& firstHash,
    const std::string& secondHash
) {
    return nodo::consensus::ProposerEquivocationEvidence(
        "SignedBlockProposalMessage{schema=NODO_BLOCK_PROPOSAL_V1;blockHash=" + firstHash + "}\nBlock{hash=" + firstHash + "}",
        "SignedBlockProposalMessage{schema=NODO_BLOCK_PROPOSAL_V1;blockHash=" + secondHash + "}\nBlock{hash=" + secondHash + "}",
        "validator-proposer-alpha",
        7,
        2,
        firstHash,
        secondHash,
        200
    );
}

class TestPersistence final : public nodo::consensus::EvidencePoolPersistence {
public:
    std::size_t persisted = 0;
    std::size_t erased = 0;

    void persist(const nodo::consensus::DoubleVoteEvidence&) override {
        ++persisted;
    }

    void persist(const nodo::consensus::ProposerEquivocationEvidence&) override {
        ++persisted;
    }

    bool erase(const std::string&) override {
        ++erased;
        return true;
    }
};

} // namespace

int main() {
    const auto evidence = makeEvidence("block-hash-a", "block-hash-b");
    assert(evidence.isConflictPair());
    assert(evidence.proposerAddress() == "validator-proposer-alpha");
    assert(evidence.blockIndex() == 7);
    assert(evidence.round() == 2);
    assert(evidence.toRecord().type() == nodo::consensus::SlashingEvidenceType::EQUIVOCATION);
    assert(evidence.toRecord().severity() == nodo::consensus::SlashingEvidenceSeverity::SLASHABLE);

    const auto reversed = makeEvidence("block-hash-b", "block-hash-a");
    assert(reversed.evidenceId() == evidence.evidenceId());
    assert(reversed.serialize() == evidence.serialize());

    const auto structural =
        nodo::consensus::SlashingEvidenceVerifier::validateProposerEquivocationStructure(evidence);
    assert(structural.accepted());

    const auto restored = nodo::consensus::ProposerEquivocationEvidence::deserialize(
        evidence.serialize()
    );
    assert(restored.serialize() == evidence.serialize());

    nodo::consensus::EvidencePool pool;
    TestPersistence persistence;
    pool.setPersistence(&persistence);
    const auto accepted = pool.submitProposerEquivocationEvidence(evidence);
    assert(accepted.accepted());
    assert(pool.size() == 1);
    assert(persistence.persisted == 1);
    assert(pool.contains(evidence.evidenceId()));
    assert(pool.proposerEquivocationEvidenceById(evidence.evidenceId()) != nullptr);
    assert(pool.allProposerEquivocationEvidence().size() == 1);
    assert(pool.allEvidence().size() == 1);

    const auto duplicate = pool.submitProposerEquivocationEvidence(evidence);
    assert(duplicate.duplicate());
    assert(pool.removeEvidence(evidence.evidenceId()));
    assert(persistence.erased == 1);
    assert(pool.size() == 0);
    assert(pool.isValid());

    std::cout << "proposer equivocation evidence tests passed\n";
    return 0;
}
