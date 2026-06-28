#include "p2p/PeerHandshakeReplayGuard.hpp"

#include <cassert>
#include <string>

using nodo::p2p::PeerHandshakeReplayGuard;

int main() {
    PeerHandshakeReplayGuard guard(2);

    const auto first = guard.issueChallenge("node-a", 1000, 10);
    assert(first.has_value());
    assert(PeerHandshakeReplayGuard::isValidNonce(*first));
    assert(guard.outstandingChallenge("node-a", 1000) == first);

    const std::string wrongNonce(64, 'f');
    assert(!guard.consumeChallenge("node-a", wrongNonce, 1001));
    assert(guard.outstandingChallenge("node-a", 1001) == first);
    assert(guard.consumeChallenge("node-a", *first, 1001));
    assert(guard.wasChallengeConsumed("node-a", *first, 1001));
    assert(!guard.consumeChallenge("node-a", *first, 1001));

    const auto discarded = guard.issueChallenge("node-discard", 1002, 10);
    assert(discarded.has_value());
    assert(guard.discardChallenge("node-discard", *discarded));
    assert(!guard.wasChallengeConsumed("node-discard", *discarded, 1002));

    const auto expiring = guard.issueChallenge("node-a", 1010, 10);
    assert(expiring.has_value());
    assert(*expiring != *first);
    assert(guard.outstandingChallenge("node-a", 1020).has_value());
    assert(!guard.outstandingChallenge("node-a", 1021).has_value());

    guard.prune(1041);
    assert(guard.consumedCount() == 0);

    PeerHandshakeReplayGuard bounded(1);
    const auto boundedNonce = bounded.issueChallenge("node-a", 2000, 10);
    assert(boundedNonce.has_value());
    assert(!bounded.issueChallenge("node-b", 2000, 10).has_value());
    assert(bounded.issueChallenge("node-a", 2001, 10).has_value());
    assert(bounded.outstandingCount() == 1);
    const auto replacement = bounded.outstandingChallenge("node-a", 2001);
    assert(replacement.has_value());
    assert(bounded.consumeChallenge("node-a", *replacement, 2001));
    assert(bounded.consumedCount() == 1);
    assert(!bounded.issueChallenge("node-b", 2002, 10).has_value());
    bounded.prune(2012);
    assert(bounded.issueChallenge("node-b", 2012, 10).has_value());

    return 0;
}
