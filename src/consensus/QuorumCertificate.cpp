#include "consensus/QuorumCertificate.hpp"

#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::consensus {

namespace {

bool isSafeScalar(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        if (character == ';' ||
            character == '{' ||
            character == '}' ||
            character == '[' ||
            character == ']' ||
            character == '\n' ||
            character == '\r' ||
            character == '\t') {
            return false;
        }
    }

    return true;
}

bool hasDuplicateVoter(
    const std::vector<ValidatorVoteRecord>& votes
) {
    std::set<std::string> voters;

    for (const auto& vote : votes) {
        if (!voters.insert(vote.validatorAddress()).second) {
            return true;
        }
    }

    return false;
}

} // namespace

std::string quorumCertificateBuildStatusToString(
    QuorumCertificateBuildStatus status
) {
    switch (status) {
        case QuorumCertificateBuildStatus::CERTIFIED:
            return "CERTIFIED";
        case QuorumCertificateBuildStatus::INVALID_INPUT:
            return "INVALID_INPUT";
        case QuorumCertificateBuildStatus::INVALID_VALIDATOR_REGISTRY:
            return "INVALID_VALIDATOR_REGISTRY";
        case QuorumCertificateBuildStatus::INVALID_VOTE:
            return "INVALID_VOTE";
        case QuorumCertificateBuildStatus::UNREGISTERED_OR_INACTIVE_VOTER:
            return "UNREGISTERED_OR_INACTIVE_VOTER";
        case QuorumCertificateBuildStatus::DUPLICATE_VOTER:
            return "DUPLICATE_VOTER";
        case QuorumCertificateBuildStatus::CONFLICTING_VOTE:
            return "CONFLICTING_VOTE";
        case QuorumCertificateBuildStatus::NOT_ENOUGH_VALID_VOTES:
            return "NOT_ENOUGH_VALID_VOTES";
        default:
            return "INVALID_INPUT";
    }
}

QuorumCertificate::QuorumCertificate()
    : m_blockIndex(0),
      m_blockHash(""),
      m_previousHash(""),
      m_round(0),
      m_requiredVoteCount(0),
      m_activeValidatorCount(0),
      m_votes() {}

QuorumCertificate::QuorumCertificate(
    std::uint64_t blockIndex,
    std::string blockHash,
    std::string previousHash,
    std::uint64_t round,
    std::uint64_t requiredVoteCount,
    std::uint64_t activeValidatorCount,
    std::vector<ValidatorVoteRecord> votes
)
    : m_blockIndex(blockIndex),
      m_blockHash(std::move(blockHash)),
      m_previousHash(std::move(previousHash)),
      m_round(round),
      m_requiredVoteCount(requiredVoteCount),
      m_activeValidatorCount(activeValidatorCount),
      m_votes(std::move(votes)) {}

std::uint64_t QuorumCertificate::blockIndex() const {
    return m_blockIndex;
}

const std::string& QuorumCertificate::blockHash() const {
    return m_blockHash;
}

const std::string& QuorumCertificate::previousHash() const {
    return m_previousHash;
}

std::uint64_t QuorumCertificate::round() const {
    return m_round;
}

std::uint64_t QuorumCertificate::requiredVoteCount() const {
    return m_requiredVoteCount;
}

std::uint64_t QuorumCertificate::activeValidatorCount() const {
    return m_activeValidatorCount;
}

const std::vector<ValidatorVoteRecord>& QuorumCertificate::votes() const {
    return m_votes;
}

std::size_t QuorumCertificate::voteCount() const {
    return m_votes.size();
}

bool QuorumCertificate::isStructurallyValid() const {
    if (m_blockIndex == 0 ||
        m_round == 0 ||
        m_requiredVoteCount == 0 ||
        m_activeValidatorCount == 0) {
        return false;
    }

    if (!isSafeScalar(m_blockHash) ||
        !isSafeScalar(m_previousHash)) {
        return false;
    }

    if (m_requiredVoteCount > m_activeValidatorCount) {
        return false;
    }

    if (m_votes.size() < m_requiredVoteCount) {
        return false;
    }

    if (hasDuplicateVoter(m_votes)) {
        return false;
    }

    for (const auto& vote : m_votes) {
        if (!vote.matchesBlock(
                m_blockIndex,
                m_blockHash,
                m_round
            )) {
            return false;
        }

        if (vote.previousHash() != m_previousHash) {
            return false;
        }

        if (vote.decision() != ValidatorVoteDecision::APPROVE) {
            return false;
        }
    }

    return true;
}

bool QuorumCertificate::verify(
    const core::ValidatorRegistry& validatorRegistry,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) const {
    if (!isStructurallyValid()) {
        return false;
    }

    if (!validatorRegistry.isValid()) {
        return false;
    }

    if (validatorRegistry.activeCount() != m_activeValidatorCount) {
        return false;
    }

    for (const auto& vote : m_votes) {
        if (!validatorRegistry.verifyValidatorIdentity(
                vote.validatorAddress(),
                vote.validatorPublicKey()
            )) {
            return false;
        }

        if (!vote.verify(policy, provider)) {
            return false;
        }
    }

    return true;
}

std::string QuorumCertificate::serialize() const {
    std::ostringstream oss;

    oss << "QuorumCertificate{"
        << "blockIndex=" << m_blockIndex
        << ";blockHash=" << m_blockHash
        << ";previousHash=" << m_previousHash
        << ";round=" << m_round
        << ";requiredVoteCount=" << m_requiredVoteCount
        << ";activeValidatorCount=" << m_activeValidatorCount
        << ";votes=[";

    for (std::size_t index = 0; index < m_votes.size(); ++index) {
        if (index != 0) {
            oss << ",";
        }

        oss << m_votes[index].serialize();
    }

    oss << "]}";

    return oss.str();
}

QuorumCertificateBuildResult::QuorumCertificateBuildResult()
    : m_status(QuorumCertificateBuildStatus::INVALID_INPUT),
      m_reason("Uninitialized quorum certificate build result."),
      m_certificate() {}

QuorumCertificateBuildResult QuorumCertificateBuildResult::certified(
    QuorumCertificate certificate
) {
    QuorumCertificateBuildResult result;
    result.m_status = QuorumCertificateBuildStatus::CERTIFIED;
    result.m_reason = "";
    result.m_certificate = std::move(certificate);
    return result;
}

QuorumCertificateBuildResult QuorumCertificateBuildResult::rejected(
    QuorumCertificateBuildStatus status,
    std::string reason
) {
    QuorumCertificateBuildResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

QuorumCertificateBuildStatus QuorumCertificateBuildResult::status() const {
    return m_status;
}

const std::string& QuorumCertificateBuildResult::reason() const {
    return m_reason;
}

const QuorumCertificate& QuorumCertificateBuildResult::certificate() const {
    return m_certificate;
}

bool QuorumCertificateBuildResult::certified() const {
    return m_status == QuorumCertificateBuildStatus::CERTIFIED;
}

std::string QuorumCertificateBuildResult::serialize() const {
    std::ostringstream oss;

    oss << "QuorumCertificateBuildResult{"
        << "status=" << quorumCertificateBuildStatusToString(m_status)
        << ";reason=" << m_reason
        << ";certificate="
        << (m_certificate.isStructurallyValid() ? m_certificate.serialize() : "NONE")
        << "}";

    return oss.str();
}

std::uint64_t QuorumCertificateBuilder::requiredVoteCount(
    std::uint64_t activeValidatorCount,
    std::uint64_t thresholdNumerator,
    std::uint64_t thresholdDenominator
) {
    if (activeValidatorCount == 0 ||
        thresholdNumerator == 0 ||
        thresholdDenominator == 0 ||
        thresholdNumerator > thresholdDenominator) {
        throw std::invalid_argument("Invalid quorum threshold parameters.");
    }

    return (
        activeValidatorCount * thresholdNumerator +
        thresholdDenominator - 1
    ) / thresholdDenominator;
}

QuorumCertificateBuildResult QuorumCertificateBuilder::buildFromVotes(
    std::uint64_t blockIndex,
    const std::string& blockHash,
    const std::string& previousHash,
    std::uint64_t round,
    const std::vector<ValidatorVoteRecord>& candidateVotes,
    const core::ValidatorRegistry& validatorRegistry,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    std::uint64_t thresholdNumerator,
    std::uint64_t thresholdDenominator
) {
    if (blockIndex == 0 ||
        round == 0 ||
        !isSafeScalar(blockHash) ||
        !isSafeScalar(previousHash) ||
        candidateVotes.empty()) {
        return QuorumCertificateBuildResult::rejected(
            QuorumCertificateBuildStatus::INVALID_INPUT,
            "Invalid quorum certificate input."
        );
    }

    if (!validatorRegistry.isValid() ||
        validatorRegistry.activeCount() == 0) {
        return QuorumCertificateBuildResult::rejected(
            QuorumCertificateBuildStatus::INVALID_VALIDATOR_REGISTRY,
            "Validator registry is invalid or has no active validators."
        );
    }

    std::uint64_t requiredVotes = 0;

    try {
        requiredVotes = requiredVoteCount(
            validatorRegistry.activeCount(),
            thresholdNumerator,
            thresholdDenominator
        );
    } catch (const std::exception& error) {
        return QuorumCertificateBuildResult::rejected(
            QuorumCertificateBuildStatus::INVALID_INPUT,
            error.what()
        );
    }

    std::vector<ValidatorVoteRecord> acceptedVotes;
    std::set<std::string> seenVoters;

    for (const auto& vote : candidateVotes) {
        if (!vote.verify(policy, provider)) {
            return QuorumCertificateBuildResult::rejected(
                QuorumCertificateBuildStatus::INVALID_VOTE,
                "Invalid vote signature or structure."
            );
        }

        if (!vote.matchesBlock(
                blockIndex,
                blockHash,
                round
            ) ||
            vote.previousHash() != previousHash) {
            return QuorumCertificateBuildResult::rejected(
                QuorumCertificateBuildStatus::CONFLICTING_VOTE,
                "Vote targets a different block or round."
            );
        }

        if (vote.decision() != ValidatorVoteDecision::APPROVE) {
            return QuorumCertificateBuildResult::rejected(
                QuorumCertificateBuildStatus::CONFLICTING_VOTE,
                "Only APPROVE votes can build a quorum certificate."
            );
        }

        if (!validatorRegistry.verifyValidatorIdentity(
                vote.validatorAddress(),
                vote.validatorPublicKey()
            )) {
            return QuorumCertificateBuildResult::rejected(
                QuorumCertificateBuildStatus::UNREGISTERED_OR_INACTIVE_VOTER,
                "Vote signer is not an active registered validator."
            );
        }

        if (!seenVoters.insert(vote.validatorAddress()).second) {
            return QuorumCertificateBuildResult::rejected(
                QuorumCertificateBuildStatus::DUPLICATE_VOTER,
                "Duplicate validator vote rejected."
            );
        }

        acceptedVotes.push_back(vote);
    }

    if (acceptedVotes.size() < requiredVotes) {
        return QuorumCertificateBuildResult::rejected(
            QuorumCertificateBuildStatus::NOT_ENOUGH_VALID_VOTES,
            "Not enough valid votes to certify block."
        );
    }

    QuorumCertificate certificate(
        blockIndex,
        blockHash,
        previousHash,
        round,
        requiredVotes,
        validatorRegistry.activeCount(),
        acceptedVotes
    );

    if (!certificate.verify(
            validatorRegistry,
            policy,
            provider
        )) {
        return QuorumCertificateBuildResult::rejected(
            QuorumCertificateBuildStatus::INVALID_VOTE,
            "Built quorum certificate failed verification."
        );
    }

    return QuorumCertificateBuildResult::certified(certificate);
}

} // namespace nodo::consensus
