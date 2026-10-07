#include "consensus/QuorumCertificate.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

int main() {
    using nodo::consensus::QuorumCertificateBuilder;

    assert(QuorumCertificateBuilder::requiredVoteCount(1, 2, 3) == 1);
    assert(QuorumCertificateBuilder::requiredVoteCount(2, 2, 3) == 2);
    assert(QuorumCertificateBuilder::requiredVoteCount(3, 2, 3) == 3);
    assert(QuorumCertificateBuilder::requiredVoteCount(4, 2, 3) == 3);
    assert(QuorumCertificateBuilder::requiredVoteCount(6, 2, 3) == 5);
    assert(QuorumCertificateBuilder::requiredVoteCount(10, 2, 3) == 7);

    bool rejected = false;

    try {
        (void)QuorumCertificateBuilder::requiredVoteCount(0, 2, 3);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);

    rejected = false;
    try {
        (void)QuorumCertificateBuilder::requiredVoteCount(10, 1, 2);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;

    try {
        (void)QuorumCertificateBuilder::requiredVoteCount(10, 4, 3);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);

    for (const auto [numerator, denominator] :
         {std::pair<std::uint64_t, std::uint64_t>{3, 4},
          {4, 6}, {1, 1}, {0, 3}}) {
        rejected = false;
        try {
            (void)QuorumCertificateBuilder::requiredVotingWeight(
                10, numerator, denominator);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    const std::uint64_t maxValidators =
        std::numeric_limits<std::uint64_t>::max();

    const std::uint64_t twoThirdsOfMax =
        QuorumCertificateBuilder::requiredVoteCount(
            maxValidators,
            2,
            3
        );

    const std::uint64_t expectedTwoThirdsOfMax =
        maxValidators - ((maxValidators - 1) / 3);

    assert(twoThirdsOfMax == expectedTwoThirdsOfMax);

    // Every allowed Byzantine weight leaves enough honest weight to form a
    // quorum, and two quorums must intersect in more than Byzantine weight.
    for (std::uint64_t total = 1; total <= 300; ++total) {
        const std::uint64_t quorum =
            QuorumCertificateBuilder::requiredVotingWeight(total, 2, 3);
        assert(3 * quorum > 2 * total);
        for (std::uint64_t byzantine = 0; 3 * byzantine < total;
             ++byzantine) {
            assert(total - byzantine >= quorum);
            assert(2 * quorum - total > byzantine);
        }
    }

    return 0;
}
