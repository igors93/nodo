#include "node/DataAvailabilityEvidence.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::node::DataAvailabilityAttestation;
using nodo::node::DataAvailabilityChallenge;
using nodo::node::DataAvailabilityFailureEvidence;
using nodo::node::DataAvailabilityResponse;

void testChallengeValid() {
    const DataAvailabilityChallenge c(
        "ch-001", 100, "blockhash-abc", "artifact-digest-xyz", "challenger-node", 1900000000
    );
    assert(c.isValid());
    assert(c.challengeId() == "ch-001");
    assert(c.blockHeight() == 100);
    assert(c.blockHash() == "blockhash-abc");
    assert(c.artifactDigest() == "artifact-digest-xyz");
}

void testChallengeEmptyFieldsRejected() {
    const DataAvailabilityChallenge empty_id("", 1, "hash", "digest", "challenger", 0);
    assert(!empty_id.isValid());

    const DataAvailabilityChallenge empty_hash("ch-001", 1, "", "digest", "challenger", 0);
    assert(!empty_hash.isValid());

    const DataAvailabilityChallenge empty_digest("ch-001", 1, "hash", "", "challenger", 0);
    assert(!empty_digest.isValid());

    const DataAvailabilityChallenge empty_challenger("ch-001", 1, "hash", "digest", "", 0);
    assert(!empty_challenger.isValid());
}

void testResponseValid() {
    const DataAvailabilityResponse r(
        "resp-001", "ch-001", "server-node", "artifact-digest-xyz", 1900000001
    );
    assert(r.isValid());
    assert(r.responseId() == "resp-001");
    assert(r.challengeId() == "ch-001");
}

void testResponseEmptyFieldsRejected() {
    const DataAvailabilityResponse r1("", "ch-001", "server", "digest", 0);
    assert(!r1.isValid());

    const DataAvailabilityResponse r2("resp", "", "server", "digest", 0);
    assert(!r2.isValid());

    const DataAvailabilityResponse r3("resp", "ch", "", "digest", 0);
    assert(!r3.isValid());

    const DataAvailabilityResponse r4("resp", "ch", "server", "", 0);
    assert(!r4.isValid());
}

void testAttestationValid() {
    const DataAvailabilityAttestation a(
        "att-001", 100, "artifact-digest-xyz", "attestor-node", 1900000000
    );
    assert(a.isValid());
    assert(a.attestationId() == "att-001");
    assert(a.blockHeight() == 100);
}

void testFailureEvidenceValid() {
    const DataAvailabilityFailureEvidence f(
        "fev-001", "ch-001", 100, "bad-node",
        "Node did not respond to challenge within timeout.", 1900000005
    );
    assert(f.isValid());
    assert(f.evidenceId() == "fev-001");
    assert(f.challengeId() == "ch-001");
}

void testFailureEvidenceEmptyFieldsRejected() {
    const DataAvailabilityFailureEvidence f1("", "ch", 1, "node", "reason", 0);
    assert(!f1.isValid());

    const DataAvailabilityFailureEvidence f2("id", "", 1, "node", "reason", 0);
    assert(!f2.isValid());

    const DataAvailabilityFailureEvidence f3("id", "ch", 1, "", "reason", 0);
    assert(!f3.isValid());

    const DataAvailabilityFailureEvidence f4("id", "ch", 1, "node", "", 0);
    assert(!f4.isValid());
}

void testSerialize() {
    const DataAvailabilityChallenge c(
        "ch-001", 100, "blockhash", "digest", "challenger", 1900000000
    );
    assert(c.isValid());
    assert(!c.serialize().empty());

    const DataAvailabilityAttestation a("att-001", 200, "digest-abc", "attestor", 1900000001);
    assert(a.isValid());
    assert(!a.serialize().empty());
}

} // namespace

int main() {
    testChallengeValid();
    testChallengeEmptyFieldsRejected();
    testResponseValid();
    testResponseEmptyFieldsRejected();
    testAttestationValid();
    testFailureEvidenceValid();
    testFailureEvidenceEmptyFieldsRejected();
    testSerialize();
    return 0;
}
