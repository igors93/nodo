#include "serialization/ConsensusCanonicalCodec.hpp"

#include "serialization/CanonicalHash.hpp"
#include "serialization/CanonicalWriter.hpp"

namespace nodo::serialization {

namespace {

constexpr const char* CONSENSUS_CODEC_VERSION =
    "NODO_CANONICAL_CONSENSUS_V1";

void writeHeader(
    CanonicalWriter& writer,
    const std::string& typeName
) {
    writer.writeString(CONSENSUS_CODEC_VERSION);
    writer.writeString(typeName);
}

} // namespace

std::vector<unsigned char> ConsensusCanonicalCodec::encodeValidatorVoteRecord(
    const consensus::ValidatorVoteRecord& vote
) {
    CanonicalWriter writer;
    writeHeader(writer, "ValidatorVoteRecord");
    writer.writeString(vote.validatorAddress());
    writer.writeString(vote.validatorPublicKey().serialize());
    writer.writeUInt64(vote.blockIndex());
    writer.writeString(vote.blockHash());
    writer.writeString(vote.previousHash());
    writer.writeUInt64(vote.round());
    writer.writeString(consensus::validatorVoteDecisionToString(vote.decision()));
    writer.writeString(vote.reasonHash());
    writer.writeInt64(vote.createdAt());
    writer.writeString(vote.signatureBundle().serialize());
    return writer.bytes();
}

std::string ConsensusCanonicalCodec::hashValidatorVoteRecord(
    const consensus::ValidatorVoteRecord& vote
) {
    return CanonicalHash::hashBytes(
        encodeValidatorVoteRecord(vote),
        "NODO_VALIDATOR_VOTE_CANONICAL_HASH_V1"
    );
}

std::vector<unsigned char> ConsensusCanonicalCodec::encodeQuorumCertificate(
    const consensus::QuorumCertificate& certificate
) {
    CanonicalWriter writer;
    writeHeader(writer, "QuorumCertificate");
    writer.writeUInt64(certificate.blockIndex());
    writer.writeString(certificate.blockHash());
    writer.writeString(certificate.previousHash());
    writer.writeUInt64(certificate.round());
    writer.writeUInt64(certificate.requiredVoteCount());
    writer.writeUInt64(certificate.activeValidatorCount());
    writer.writeUInt32(static_cast<std::uint32_t>(certificate.votes().size()));

    for (const auto& vote : certificate.votes()) {
        writer.writeBytes(encodeValidatorVoteRecord(vote));
    }

    return writer.bytes();
}

std::string ConsensusCanonicalCodec::hashQuorumCertificate(
    const consensus::QuorumCertificate& certificate
) {
    return CanonicalHash::hashBytes(
        encodeQuorumCertificate(certificate),
        "NODO_QUORUM_CERTIFICATE_CANONICAL_HASH_V1"
    );
}

} // namespace nodo::serialization
