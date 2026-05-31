#include "consensus/ValidatorVoteRecord.hpp"

#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"

#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::consensus {

namespace {

constexpr const char* VOTE_PAYLOAD_VERSION =
    "NODO_VALIDATOR_VOTE_PAYLOAD_V1";

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

bool isValidatorAddressBoundToPublicKey(
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey
) {
    const crypto::Address address =
        crypto::Address::fromString(validatorAddress);

    if (!address.isValid()) {
        return false;
    }

    return crypto::AddressDerivation::verifyAddressForPublicKey(
        address,
        validatorPublicKey
    );
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

std::int64_t parseI64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current =
            value[index];

        if (current == '-' && index == 0 && value.size() > 1) {
            continue;
        }

        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::int64_t parsed =
        static_cast<std::int64_t>(
            std::stoll(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

crypto::PublicKey parsePublicKey(
    const std::string& serialized
) {
    const std::map<std::string, std::string> fields =
        parseObjectFields(
            serialized,
            "PublicKey"
        );

    requireExactFields(
        fields,
        {
            "algorithm",
            "keyMaterial"
        },
        "PublicKey"
    );

    const crypto::CryptoAlgorithm algorithm =
        crypto::cryptoAlgorithmFromString(
            requireField(fields, "algorithm", "PublicKey")
        );

    if (crypto::cryptoAlgorithmToString(algorithm) !=
        requireField(fields, "algorithm", "PublicKey")) {
        throw std::invalid_argument("Unknown serialized PublicKey algorithm.");
    }

    crypto::PublicKey publicKey(
        algorithm,
        requireField(fields, "keyMaterial", "PublicKey")
    );

    if (!publicKey.isValid() ||
        publicKey.serialize() != serialized) {
        throw std::invalid_argument("Serialized PublicKey is invalid or non-canonical.");
    }

    return publicKey;
}

ValidatorVoteDecision parseVoteDecision(
    const std::string& value
) {
    if (value == "APPROVE") {
        return ValidatorVoteDecision::APPROVE;
    }

    if (value == "REJECT") {
        return ValidatorVoteDecision::REJECT;
    }

    throw std::invalid_argument("Unknown validator vote decision: " + value);
}

crypto::Signature parseSignature(
    const std::string& serialized,
    const crypto::PublicKey& expectedPublicKey
) {
    const std::map<std::string, std::string> fields =
        parseObjectFields(
            serialized,
            "Signature"
        );

    requireExactFields(
        fields,
        {
            "suite",
            "domain",
            "algorithm",
            "publicKeyFingerprint",
            "signatureHex",
            "createdAt"
        },
        "Signature"
    );

    const crypto::CryptoAlgorithm algorithm =
        crypto::cryptoAlgorithmFromString(
            requireField(fields, "algorithm", "Signature")
        );

    if (crypto::cryptoAlgorithmToString(algorithm) !=
        requireField(fields, "algorithm", "Signature")) {
        throw std::invalid_argument("Unknown serialized Signature algorithm.");
    }

    const crypto::CryptoSuiteId suite =
        crypto::cryptoSuiteIdFromString(
            requireField(fields, "suite", "Signature")
        );

    if (!crypto::isSupportedCryptoSuite(suite)) {
        throw std::invalid_argument("Unknown serialized Signature suite.");
    }

    const crypto::SigningDomain domain =
        crypto::signingDomainFromString(
            requireField(fields, "domain", "Signature")
        );

    if (domain == crypto::SigningDomain::UNKNOWN) {
        throw std::invalid_argument("Unknown serialized Signature domain.");
    }

    if (algorithm != expectedPublicKey.algorithm()) {
        throw std::invalid_argument("Serialized Signature algorithm does not match vote public key.");
    }

    if (requireField(fields, "publicKeyFingerprint", "Signature") !=
        expectedPublicKey.fingerprint()) {
        throw std::invalid_argument("Serialized Signature public key fingerprint does not match vote public key.");
    }

    crypto::Signature signature(
        suite,
        domain,
        algorithm,
        expectedPublicKey,
        requireField(fields, "signatureHex", "Signature"),
        parseI64Strict(
            requireField(fields, "createdAt", "Signature"),
            "Signature.createdAt"
        )
    );

    if (!signature.isValid() ||
        signature.serialize() != serialized) {
        throw std::invalid_argument("Serialized Signature is invalid or non-canonical.");
    }

    return signature;
}

crypto::SignatureBundle parseSignatureBundle(
    const std::string& serialized,
    const crypto::PublicKey& expectedPublicKey
) {
    const std::string prefix =
        "SignatureBundle{";

    if (serialized.rfind(prefix, 0) != 0 ||
        serialized.size() <= prefix.size() ||
        serialized.back() != '}') {
        throw std::invalid_argument("Serialized data is not a SignatureBundle.");
    }

    const std::string body =
        serialized.substr(
            prefix.size(),
            serialized.size() - prefix.size() - 1
        );

    if (body.empty()) {
        throw std::invalid_argument("Serialized SignatureBundle is empty.");
    }

    crypto::SignatureBundle bundle;

    for (const std::string& serializedSignature : splitTopLevel(body, ';')) {
        bundle.addSignature(
            parseSignature(
                serializedSignature,
                expectedPublicKey
            )
        );
    }

    if (bundle.serialize() != serialized) {
        throw std::invalid_argument("Serialized SignatureBundle is non-canonical.");
    }

    return bundle;
}

} // namespace

std::string validatorVoteDecisionToString(
    ValidatorVoteDecision decision
) {
    switch (decision) {
        case ValidatorVoteDecision::APPROVE:
            return "APPROVE";
        case ValidatorVoteDecision::REJECT:
            return "REJECT";
        case ValidatorVoteDecision::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

ValidatorVoteRecord::ValidatorVoteRecord()
    : m_validatorAddress(""),
      m_validatorPublicKey(),
      m_blockIndex(0),
      m_blockHash(""),
      m_previousHash(""),
      m_round(0),
      m_decision(ValidatorVoteDecision::UNKNOWN),
      m_reasonHash(""),
      m_createdAt(0),
      m_signatureBundle() {}

ValidatorVoteRecord::ValidatorVoteRecord(
    std::string validatorAddress,
    crypto::PublicKey validatorPublicKey,
    std::uint64_t blockIndex,
    std::string blockHash,
    std::string previousHash,
    std::uint64_t round,
    ValidatorVoteDecision decision,
    std::string reasonHash,
    std::int64_t createdAt,
    crypto::SignatureBundle signatureBundle
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_validatorPublicKey(std::move(validatorPublicKey)),
      m_blockIndex(blockIndex),
      m_blockHash(std::move(blockHash)),
      m_previousHash(std::move(previousHash)),
      m_round(round),
      m_decision(decision),
      m_reasonHash(std::move(reasonHash)),
      m_createdAt(createdAt),
      m_signatureBundle(std::move(signatureBundle)) {}

const std::string& ValidatorVoteRecord::validatorAddress() const {
    return m_validatorAddress;
}

const crypto::PublicKey& ValidatorVoteRecord::validatorPublicKey() const {
    return m_validatorPublicKey;
}

std::uint64_t ValidatorVoteRecord::blockIndex() const {
    return m_blockIndex;
}

const std::string& ValidatorVoteRecord::blockHash() const {
    return m_blockHash;
}

const std::string& ValidatorVoteRecord::previousHash() const {
    return m_previousHash;
}

std::uint64_t ValidatorVoteRecord::round() const {
    return m_round;
}

ValidatorVoteDecision ValidatorVoteRecord::decision() const {
    return m_decision;
}

const std::string& ValidatorVoteRecord::reasonHash() const {
    return m_reasonHash;
}

std::int64_t ValidatorVoteRecord::createdAt() const {
    return m_createdAt;
}

const crypto::SignatureBundle& ValidatorVoteRecord::signatureBundle() const {
    return m_signatureBundle;
}

std::string ValidatorVoteRecord::signingPayload() const {
    return buildSigningPayload(
        m_validatorAddress,
        m_validatorPublicKey,
        m_blockIndex,
        m_blockHash,
        m_previousHash,
        m_round,
        m_decision,
        m_reasonHash,
        m_createdAt
    );
}

bool ValidatorVoteRecord::matchesBlock(
    std::uint64_t blockIndex,
    const std::string& blockHash,
    std::uint64_t round
) const {
    return m_blockIndex == blockIndex &&
           m_blockHash == blockHash &&
           m_round == round;
}

bool ValidatorVoteRecord::isStructurallyValid(
    const crypto::CryptoPolicy& policy
) const {
    if (!isSafeScalar(m_validatorAddress) ||
        !isSafeScalar(m_blockHash) ||
        !isSafeScalar(m_previousHash) ||
        !isSafeScalar(m_reasonHash)) {
        return false;
    }

    if (!m_validatorPublicKey.isValid()) {
        return false;
    }

    if (!isValidatorAddressBoundToPublicKey(
            m_validatorAddress,
            m_validatorPublicKey
        )) {
        return false;
    }

    if (m_blockIndex == 0 ||
        m_round == 0 ||
        m_createdAt <= 0) {
        return false;
    }

    if (m_decision == ValidatorVoteDecision::UNKNOWN) {
        return false;
    }

    if (m_signatureBundle.empty()) {
        return false;
    }

    if (!m_signatureBundle.isValidForPolicy(
            policy,
            crypto::SecurityContext::VALIDATOR_OPERATION
        )) {
        return false;
    }

    return true;
}

bool ValidatorVoteRecord::verify(
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) const {
    if (!isStructurallyValid(policy)) {
        return false;
    }

    return m_signatureBundle.verifyForPolicy(
        signingPayload(),
        policy,
        crypto::SecurityContext::VALIDATOR_OPERATION,
        provider
    );
}

std::string ValidatorVoteRecord::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorVoteRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";validatorPublicKey=" << m_validatorPublicKey.serialize()
        << ";validatorPublicKeyFingerprint=" << m_validatorPublicKey.fingerprint()
        << ";blockIndex=" << m_blockIndex
        << ";blockHash=" << m_blockHash
        << ";previousHash=" << m_previousHash
        << ";round=" << m_round
        << ";decision=" << validatorVoteDecisionToString(m_decision)
        << ";reasonHash=" << m_reasonHash
        << ";createdAt=" << m_createdAt
        << ";signatureBundle=" << m_signatureBundle.serialize()
        << "}";

    return oss.str();
}

std::string ValidatorVoteRecord::buildSigningPayload(
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey,
    std::uint64_t blockIndex,
    const std::string& blockHash,
    const std::string& previousHash,
    std::uint64_t round,
    ValidatorVoteDecision decision,
    const std::string& reasonHash,
    std::int64_t createdAt
) {
    if (!isSafeScalar(validatorAddress) ||
        !isSafeScalar(blockHash) ||
        !isSafeScalar(previousHash) ||
        !isSafeScalar(reasonHash)) {
        throw std::invalid_argument("Unsafe scalar rejected by ValidatorVoteRecord.");
    }

    if (!validatorPublicKey.isValid()) {
        throw std::invalid_argument("Validator vote public key is invalid.");
    }

    if (!isValidatorAddressBoundToPublicKey(
            validatorAddress,
            validatorPublicKey
        )) {
        throw std::invalid_argument("Validator vote address is not bound to public key.");
    }

    if (blockIndex == 0 ||
        round == 0 ||
        createdAt <= 0) {
        throw std::invalid_argument("Validator vote numeric fields are invalid.");
    }

    if (decision == ValidatorVoteDecision::UNKNOWN) {
        throw std::invalid_argument("Validator vote decision is UNKNOWN.");
    }

    std::ostringstream oss;

    /*
     * Versioned canonical payload.
     *
     * This is what validators actually sign. Keep the field order stable.
     */
    oss << "ValidatorVoteSigningPayload{"
        << "version=" << VOTE_PAYLOAD_VERSION
        << ";validatorAddress=" << validatorAddress
        << ";validatorPublicKey=" << validatorPublicKey.serialize()
        << ";validatorPublicKeyFingerprint=" << validatorPublicKey.fingerprint()
        << ";blockIndex=" << blockIndex
        << ";blockHash=" << blockHash
        << ";previousHash=" << previousHash
        << ";round=" << round
        << ";decision=" << validatorVoteDecisionToString(decision)
        << ";reasonHash=" << reasonHash
        << ";createdAt=" << createdAt
        << "}";

    return oss.str();
}

ValidatorVoteRecord ValidatorVoteRecord::createVote(
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey,
    const crypto::PrivateKey& validatorPrivateKey,
    std::uint64_t blockIndex,
    const std::string& blockHash,
    const std::string& previousHash,
    std::uint64_t round,
    ValidatorVoteDecision decision,
    const std::string& reasonHash,
    std::int64_t createdAt,
    const crypto::SignatureProvider& provider
) {
    const std::string payload =
        buildSigningPayload(
            validatorAddress,
            validatorPublicKey,
            blockIndex,
            blockHash,
            previousHash,
            round,
            decision,
            reasonHash,
            createdAt
        );

    crypto::SignatureBundle signatureBundle =
        crypto::SignatureBundle::createSignature(
            payload,
            validatorPublicKey,
            validatorPrivateKey,
            createdAt,
            provider,
            crypto::SigningDomain::VALIDATOR_VOTE
        );

    return ValidatorVoteRecord(
        validatorAddress,
        validatorPublicKey,
        blockIndex,
        blockHash,
        previousHash,
        round,
        decision,
        reasonHash,
        createdAt,
        signatureBundle
    );
}

ValidatorVoteRecord ValidatorVoteRecord::deserialize(
    const std::string& serialized
) {
    const std::map<std::string, std::string> fields =
        parseObjectFields(
            serialized,
            "ValidatorVoteRecord"
        );

    requireExactFields(
        fields,
        {
            "validatorAddress",
            "validatorPublicKey",
            "validatorPublicKeyFingerprint",
            "blockIndex",
            "blockHash",
            "previousHash",
            "round",
            "decision",
            "reasonHash",
            "createdAt",
            "signatureBundle"
        },
        "ValidatorVoteRecord"
    );

    const crypto::PublicKey validatorPublicKey =
        parsePublicKey(
            requireField(fields, "validatorPublicKey", "ValidatorVoteRecord")
        );

    if (requireField(fields, "validatorPublicKeyFingerprint", "ValidatorVoteRecord") !=
        validatorPublicKey.fingerprint()) {
        throw std::invalid_argument("Serialized validator public key fingerprint does not match public key.");
    }

    ValidatorVoteRecord vote(
        requireField(fields, "validatorAddress", "ValidatorVoteRecord"),
        validatorPublicKey,
        parseU64Strict(
            requireField(fields, "blockIndex", "ValidatorVoteRecord"),
            "ValidatorVoteRecord.blockIndex"
        ),
        requireField(fields, "blockHash", "ValidatorVoteRecord"),
        requireField(fields, "previousHash", "ValidatorVoteRecord"),
        parseU64Strict(
            requireField(fields, "round", "ValidatorVoteRecord"),
            "ValidatorVoteRecord.round"
        ),
        parseVoteDecision(
            requireField(fields, "decision", "ValidatorVoteRecord")
        ),
        requireField(fields, "reasonHash", "ValidatorVoteRecord"),
        parseI64Strict(
            requireField(fields, "createdAt", "ValidatorVoteRecord"),
            "ValidatorVoteRecord.createdAt"
        ),
        parseSignatureBundle(
            requireField(fields, "signatureBundle", "ValidatorVoteRecord"),
            validatorPublicKey
        )
    );

    if (vote.serialize() != serialized) {
        throw std::invalid_argument("Serialized ValidatorVoteRecord is non-canonical.");
    }

    return vote;
}

} // namespace nodo::consensus
