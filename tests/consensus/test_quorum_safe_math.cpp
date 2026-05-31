#include "consensus/QuorumCertificate.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>

int main() {
    using nodo::consensus::QuorumCertificateBuilder;

    assert(QuorumCertificateBuilder::requiredVoteCount(1, 2, 3) == 1);
    assert(QuorumCertificateBuilder::requiredVoteCount(2, 2, 3) == 2);
    assert(QuorumCertificateBuilder::requiredVoteCount(3, 2, 3) == 2);
    assert(QuorumCertificateBuilder::requiredVoteCount(4, 2, 3) == 3);
    assert(QuorumCertificateBuilder::requiredVoteCount(10, 1, 2) == 5);

    bool rejected = false;

    try {
        (void)QuorumCertificateBuilder::requiredVoteCount(0, 2, 3);
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

    const std::uint64_t all =
        QuorumCertificateBuilder::requiredVoteCount(
            std::numeric_limits<std::uint64_t>::max(),
            1,
            1
        );

    assert(all == std::numeric_limits<std::uint64_t>::max());

    return 0;
}
