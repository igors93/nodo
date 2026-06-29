#include "consensus/QuorumCertificate.hpp"

#include <limits>
#include <map>
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

std::vector<std::string> splitTopLevel(
    const std::string& value,
    char separator
) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    int braceDepth = 0;
    int bracketDepth = 0;

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current =
            value[index];

        if (current == '{') {
            ++braceDepth;
        } else if (current == '}') {
            --braceDepth;
        } else if (current == '[') {
            ++bracketDepth;
        } else if (current == ']') {
            --bracketDepth;
        }

        if (braceDepth < 0 || bracketDepth < 0) {
            throw std::invalid_argument("Malformed nested serialized value.");
        }

        if (current == separator &&
            braceDepth == 0 &&
            bracketDepth == 0) {
            if (index == start) {
                throw std::invalid_argument("Empty serialized field.");
            }

            parts.push_back(value.substr(start, index - start));
            start = index + 1;
        }
    }

    if (braceDepth != 0 || bracketDepth != 0) {
        throw std::invalid_argument("Unbalanced nested serialized value.");
    }

    if (start >= value.size()) {
        throw std::invalid_argument("Serialized value has a trailing separator.");
    }

    parts.push_back(value.substr(start));
    return parts;
}

std::map<std::string, std::string> parseObjectFields(
    const std::string& serialized,
    const std::string& typeName
) {
    const std::string prefix =
        typeName + "{";

    if (serialized.rfind(prefix, 0) != 0 ||
        serialized.size() <= prefix.size() ||
        serialized.back() != '}') {
        throw std::invalid_argument("Serialized data is not a " + typeName + ".");
    }

    const std::string body =
        serialized.substr(
            prefix.size(),
            serialized.size() - prefix.size() - 1
        );

    if (body.empty()) {
        throw std::invalid_argument("Serialized " + typeName + " is empty.");
    }

    std::map<std::string, std::string> fields;

    for (const std::string& part : splitTopLevel(body, ';')) {
        const std::size_t separator =
            part.find('=');

        if (separator == std::string::npos ||
            separator == 0 ||
            separator + 1 >= part.size()) {
            throw std::invalid_argument("Malformed serialized " + typeName + " field.");
        }

        const std::string key =
            part.substr(0, separator);

        const std::string value =
            part.substr(separator + 1);

        if (!fields.emplace(key, value).second) {
            throw std::invalid_argument("Duplicate serialized " + typeName + " field: " + key);
        }
    }

    return fields;
}

void requireExactFields(
    const std::map<std::string, std::string>& fields,
    const std::set<std::string>& expected,
    const std::string& typeName
) {
    for (const std::string& key : expected) {
        if (fields.find(key) == fields.end()) {
            throw std::invalid_argument("Missing serialized " + typeName + " field: " + key);
        }
    }

    for (const auto& [key, ignored] : fields) {
        (void)ignored;

        if (expected.find(key) == expected.end()) {
            throw std::invalid_argument("Unknown serialized " + typeName + " field: " + key);
        }
    }
}

std::string requireField(
    const std::map<std::string, std::string>& fields,
    const std::string& key,
    const std::string& typeName
) {
    const auto found =
        fields.find(key);

    if (found == fields.end()) {
        throw std::invalid_argument("Missing serialized " + typeName + " field: " + key);
    }

    return found->second;
}

std::uint64_t parseU64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (const char current : value) {
        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::uint64_t parsed =
        static_cast<std::uint64_t>(
            std::stoull(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

std::vector<ValidatorVoteRecord> parseVotes(
    const std::string& serializedVotes
) {
    if (serializedVotes.size() < 2 ||
        serializedVotes.front() != '[' ||
        serializedVotes.back() != ']') {
        throw std::invalid_argument("Serialized quorum vote list is malformed.");
    }

    const std::string body =
        serializedVotes.substr(
            1,
            serializedVotes.size() - 2
        );

    if (body.empty()) {
        return {};
    }

    std::vector<ValidatorVoteRecord> votes;

    for (const std::string& serializedVote : splitTopLevel(body, ',')) {
        votes.push_back(
            ValidatorVoteRecord::deserialize(serializedVote)
        );
    }

    return votes;
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
      m_requiredVotingWeight(0),
      m_totalVotingWeight(0),
      m_signedVotingWeight(0),
      m_validatorSetRoot(""),
      m_votes() {}

QuorumCertificate::QuorumCertificate(
    std::uint64_t blockIndex,
    std::string blockHash,
    std::string previousHash,
    std::uint64_t round,
    std::uint64_t requiredVotingWeight,
    std::uint64_t totalVotingWeight,
    std::uint64_t signedVotingWeight,
    std::string validatorSetRoot,
    std::vector<ValidatorVoteRecord> votes
)
    : m_blockIndex(blockIndex),
      m_blockHash(std::move(blockHash)),
      m_previousHash(std::move(previousHash)),
      m_round(round),
      m_requiredVotingWeight(requiredVotingWeight),
      m_totalVotingWeight(totalVotingWeight),
      m_signedVotingWeight(signedVotingWeight),
      m_validatorSetRoot(std::move(validatorSetRoot)),
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

std::uint64_t QuorumCertificate::requiredVotingWeight() const {
    return m_requiredVotingWeight;
}

std::uint64_t QuorumCertificate::totalVotingWeight() const {
    return m_totalVotingWeight;
}

std::uint64_t QuorumCertificate::signedVotingWeight() const {
    return m_signedVotingWeight;
}

const std::string& QuorumCertificate::validatorSetRoot() const {
    return m_validatorSetRoot;
}

std::uint64_t QuorumCertificate::requiredVoteCount() const {
    return m_requiredVotingWeight;
}

std::uint64_t QuorumCertificate::activeValidatorCount() const {
    return m_totalVotingWeight;
}

const std::vector<ValidatorVoteRecord>& QuorumCertificate::votes() const {
    return m_votes;
}

std::size_t QuorumCertificate::voteCount() const {
    return m_votes.size();
}

/**
 * Verifies the structural integrity of the quorum certificate.
 * Checks for required vote counts, prevents duplicate voters, and ensures all votes
 * correctly target the identical block index, block hash, and consensus round.
 */
bool QuorumCertificate::isStructurallyValid() const {
    if (m_blockIndex == 0 ||
        m_round == 0 ||
        m_requiredVotingWeight == 0 ||
        m_totalVotingWeight == 0 ||
        m_signedVotingWeight == 0) {
        return false;
    }

    if (!isSafeScalar(m_blockHash) ||
        !isSafeScalar(m_previousHash) ||
        !isSafeScalar(m_validatorSetRoot)) {
        return false;
    }

    if (m_requiredVotingWeight > m_totalVotingWeight ||
        m_signedVotingWeight > m_totalVotingWeight ||
        m_signedVotingWeight < m_requiredVotingWeight) {
        return false;
    }

    if (m_votes.empty()) {
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

        if (vote.decision() != ValidatorVoteDecision::APPROVE &&
            vote.decision() != ValidatorVoteDecision::PRECOMMIT) {
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

    if (validatorRegistry.totalConsensusWeight() != m_totalVotingWeight) {
        return false;
    }

    if (validatorRegistry.validatorSetRoot() != m_validatorSetRoot) {
        return false;
    }

    std::uint64_t signedWeight = 0;

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

        const std::uint64_t voterWeight =
            validatorRegistry.consensusWeightFor(vote.validatorAddress());
        if (voterWeight == 0 ||
            std::numeric_limits<std::uint64_t>::max() - signedWeight < voterWeight) {
            return false;
        }
        signedWeight += voterWeight;
    }

    return signedWeight == m_signedVotingWeight &&
           signedWeight >= m_requiredVotingWeight;
}

std::string QuorumCertificate::serialize() const {
    std::ostringstream oss;

    oss << "QuorumCertificate{"
        << "blockIndex=" << m_blockIndex
        << ";blockHash=" << m_blockHash
        << ";previousHash=" << m_previousHash
        << ";round=" << m_round
        << ";requiredVotingWeight=" << m_requiredVotingWeight
        << ";totalVotingWeight=" << m_totalVotingWeight
        << ";signedVotingWeight=" << m_signedVotingWeight
        << ";validatorSetRoot=" << m_validatorSetRoot
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

QuorumCertificate QuorumCertificate::deserialize(
    const std::string& serialized
) {
    const std::map<std::string, std::string> fields =
        parseObjectFields(
            serialized,
            "QuorumCertificate"
        );

    requireExactFields(
        fields,
        {
            "blockIndex",
            "blockHash",
            "previousHash",
            "round",
            "requiredVotingWeight",
            "totalVotingWeight",
            "signedVotingWeight",
            "validatorSetRoot",
            "votes"
        },
        "QuorumCertificate"
    );

    QuorumCertificate certificate(
        parseU64Strict(
            requireField(fields, "blockIndex", "QuorumCertificate"),
            "QuorumCertificate.blockIndex"
        ),
        requireField(fields, "blockHash", "QuorumCertificate"),
        requireField(fields, "previousHash", "QuorumCertificate"),
        parseU64Strict(
            requireField(fields, "round", "QuorumCertificate"),
            "QuorumCertificate.round"
        ),
        parseU64Strict(
            requireField(fields, "requiredVotingWeight", "QuorumCertificate"),
            "QuorumCertificate.requiredVotingWeight"
        ),
        parseU64Strict(
            requireField(fields, "totalVotingWeight", "QuorumCertificate"),
            "QuorumCertificate.totalVotingWeight"
        ),
        parseU64Strict(
            requireField(fields, "signedVotingWeight", "QuorumCertificate"),
            "QuorumCertificate.signedVotingWeight"
        ),
        requireField(fields, "validatorSetRoot", "QuorumCertificate"),
        parseVotes(
            requireField(fields, "votes", "QuorumCertificate")
        )
    );

    if (!certificate.isStructurallyValid()) {
        throw std::invalid_argument("Serialized QuorumCertificate is structurally invalid.");
    }

    if (certificate.serialize() != serialized) {
        throw std::invalid_argument("Serialized QuorumCertificate is non-canonical.");
    }

    return certificate;
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
    return requiredVotingWeight(
        activeValidatorCount,
        thresholdNumerator,
        thresholdDenominator
    );
}

std::uint64_t QuorumCertificateBuilder::requiredVotingWeight(
    std::uint64_t totalVotingWeight,
    std::uint64_t thresholdNumerator,
    std::uint64_t thresholdDenominator
) {
    if (totalVotingWeight == 0 ||
        thresholdNumerator == 0 ||
        thresholdDenominator == 0 ||
        thresholdNumerator > thresholdDenominator) {
        throw std::invalid_argument("Invalid quorum threshold parameters.");
    }

    const unsigned __int128 scaledVotes =
        static_cast<unsigned __int128>(totalVotingWeight) *
        static_cast<unsigned __int128>(thresholdNumerator);

    const unsigned __int128 roundedVotes =
        scaledVotes +
        static_cast<unsigned __int128>(thresholdDenominator) -
        1;

    const unsigned __int128 requiredVotes =
        roundedVotes /
        static_cast<unsigned __int128>(thresholdDenominator);

    return static_cast<std::uint64_t>(requiredVotes);
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
        validatorRegistry.totalConsensusWeight() == 0) {
        return QuorumCertificateBuildResult::rejected(
            QuorumCertificateBuildStatus::INVALID_VALIDATOR_REGISTRY,
            "Validator registry is invalid or has no weighted consensus validators."
        );
    }

    const std::uint64_t totalVotingWeight =
        validatorRegistry.totalConsensusWeight();
    std::uint64_t requiredWeight = 0;

    try {
        requiredWeight = requiredVotingWeight(
            totalVotingWeight,
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
    std::uint64_t signedWeight = 0;

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

        if (vote.decision() != ValidatorVoteDecision::APPROVE &&
            vote.decision() != ValidatorVoteDecision::PRECOMMIT) {
            return QuorumCertificateBuildResult::rejected(
                QuorumCertificateBuildStatus::CONFLICTING_VOTE,
                "Only APPROVE or PRECOMMIT votes can build a quorum certificate."
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

        const std::uint64_t voterWeight =
            validatorRegistry.consensusWeightFor(vote.validatorAddress());
        if (voterWeight == 0) {
            return QuorumCertificateBuildResult::rejected(
                QuorumCertificateBuildStatus::UNREGISTERED_OR_INACTIVE_VOTER,
                "Vote signer has no consensus voting weight."
            );
        }

        if (std::numeric_limits<std::uint64_t>::max() - signedWeight < voterWeight) {
            return QuorumCertificateBuildResult::rejected(
                QuorumCertificateBuildStatus::INVALID_INPUT,
                "Signed voting weight overflow."
            );
        }

        signedWeight += voterWeight;
        acceptedVotes.push_back(vote);
    }

    if (signedWeight < requiredWeight) {
        return QuorumCertificateBuildResult::rejected(
            QuorumCertificateBuildStatus::NOT_ENOUGH_VALID_VOTES,
            "Not enough signed validator weight to certify block."
        );
    }

    QuorumCertificate certificate(
        blockIndex,
        blockHash,
        previousHash,
        round,
        requiredWeight,
        totalVotingWeight,
        signedWeight,
        validatorRegistry.validatorSetRoot(),
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
