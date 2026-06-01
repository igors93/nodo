#ifndef NODO_SERIALIZATION_CONSENSUS_CANONICAL_CODEC_HPP
#define NODO_SERIALIZATION_CONSENSUS_CANONICAL_CODEC_HPP

#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"

#include <string>
#include <vector>

namespace nodo::serialization {

class ConsensusCanonicalCodec {
public:
    static std::vector<unsigned char> encodeValidatorVoteRecord(
        const consensus::ValidatorVoteRecord& vote
    );

    static std::string hashValidatorVoteRecord(
        const consensus::ValidatorVoteRecord& vote
    );

    static std::vector<unsigned char> encodeQuorumCertificate(
        const consensus::QuorumCertificate& certificate
    );

    static std::string hashQuorumCertificate(
        const consensus::QuorumCertificate& certificate
    );
};

} // namespace nodo::serialization

#endif
