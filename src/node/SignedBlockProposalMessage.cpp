#include "node/SignedBlockProposalMessage.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SigningDomain.hpp"
#include "serialization/BlockCodec.hpp"

#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nodo::node {

namespace {

constexpr const char* kPayloadVersion = "NODO_BLOCK_PROPOSAL_V1";
constexpr std::size_t kMaxSerializedBlockBytes = 2 * 1024 * 1024;

bool isSafeScalar(const std::string& s, std::size_t maxLen = 240) {
    if (s.empty() || s.size() > maxLen) return false;
    for (const char c : s) {
        const bool ok =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
            c == ':' || c == '/';
        if (!ok) return false;
    }
    return true;
}

std::vector<std::string> splitTopLevel(const std::string& body, char sep) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    int depth = 0;
    for (std::size_t i = 0; i < body.size(); ++i) {
        const char c = body[i];
        if (c == '{') ++depth;
        else if (c == '}') --depth;
        if (depth < 0) throw std::invalid_argument("Unbalanced braces in SignedBlockProposalMessage.");
        if (depth == 0 && c == sep) {
            if (i > start) parts.push_back(body.substr(start, i - start));
            start = i + 1;
        }
    }
    if (depth != 0) throw std::invalid_argument("Unbalanced braces in message body.");
    if (start < body.size()) parts.push_back(body.substr(start));
    return parts;
}

std::map<std::string, std::string> parseFields(const std::string& body) {
    std::map<std::string, std::string> result;
    for (const auto& token : splitTopLevel(body, ';')) {
        if (token.empty()) continue;
        const auto eq = token.find('=');
        if (eq == std::string::npos || eq == 0)
            throw std::invalid_argument("Malformed field: " + token);
        const std::string key = token.substr(0, eq);
        const std::string val = token.substr(eq + 1);
        if (!result.emplace(key, val).second)
            throw std::invalid_argument("Duplicate field: " + key);
    }
    return result;
}

std::string requireField(const std::map<std::string, std::string>& fields,
                         const std::string& key) {
    const auto it = fields.find(key);
    if (it == fields.end()) throw std::invalid_argument("Missing field: " + key);
    return it->second;
}

std::uint64_t parseU64(const std::string& v) {
    if (v.empty()) throw std::invalid_argument("Empty uint64 field.");
    std::size_t n = 0;
    const unsigned long long parsed = std::stoull(v, &n);
    if (n != v.size()) throw std::invalid_argument("Non-numeric uint64: " + v);
    return static_cast<std::uint64_t>(parsed);
}

std::int64_t parseI64(const std::string& v) {
    if (v.empty()) throw std::invalid_argument("Empty int64 field.");
    std::size_t n = 0;
    const long long parsed = std::stoll(v, &n);
    if (n != v.size()) throw std::invalid_argument("Non-numeric int64: " + v);
    return static_cast<std::int64_t>(parsed);
}

crypto::PublicKey parsePublicKey(const std::string& serialized) {
    const std::string prefix = "PublicKey{";
    const std::string suffix = "}";
    if (serialized.size() <= prefix.size() ||
        serialized.compare(0, prefix.size(), prefix) != 0 ||
        serialized.back() != '}') {
        throw std::invalid_argument("Not a serialized PublicKey.");
    }
    const std::string body = serialized.substr(prefix.size(),
        serialized.size() - prefix.size() - 1);
    const auto fields = parseFields(body);
    const crypto::CryptoAlgorithm alg =
        crypto::cryptoAlgorithmFromString(requireField(fields, "algorithm"));
    return crypto::PublicKey(alg, requireField(fields, "keyMaterial"));
}

} // namespace

SignedBlockProposalMessage::SignedBlockProposalMessage()
    : m_proposerAddress(),
      m_proposerPublicKey(),
      m_blockIndex(0),
      m_round(0),
      m_blockHash(),
      m_serializedBlock(),
      m_proposedAt(0),
      m_signatureBundle() {}

SignedBlockProposalMessage::SignedBlockProposalMessage(
    std::string proposerAddress,
    crypto::PublicKey proposerPublicKey,
    std::uint64_t blockIndex,
    std::uint64_t round,
    std::string blockHash,
    std::string serializedBlock,
    std::int64_t proposedAt,
    crypto::SignatureBundle signatureBundle
)
    : m_proposerAddress(std::move(proposerAddress)),
      m_proposerPublicKey(std::move(proposerPublicKey)),
      m_blockIndex(blockIndex),
      m_round(round),
      m_blockHash(std::move(blockHash)),
      m_serializedBlock(std::move(serializedBlock)),
      m_proposedAt(proposedAt),
      m_signatureBundle(std::move(signatureBundle)) {}

const std::string& SignedBlockProposalMessage::proposerAddress() const { return m_proposerAddress; }
const crypto::PublicKey& SignedBlockProposalMessage::proposerPublicKey() const { return m_proposerPublicKey; }
std::uint64_t SignedBlockProposalMessage::blockIndex() const { return m_blockIndex; }
std::uint64_t SignedBlockProposalMessage::round() const { return m_round; }
const std::string& SignedBlockProposalMessage::blockHash() const { return m_blockHash; }
const std::string& SignedBlockProposalMessage::serializedBlock() const { return m_serializedBlock; }
std::int64_t SignedBlockProposalMessage::proposedAt() const { return m_proposedAt; }
const crypto::SignatureBundle& SignedBlockProposalMessage::signatureBundle() const { return m_signatureBundle; }

bool SignedBlockProposalMessage::isValid() const {
    return isSafeScalar(m_proposerAddress) &&
           m_proposerPublicKey.isValid() &&
           m_blockIndex > 0 &&
           m_round > 0 &&
           isSafeScalar(m_blockHash) &&
           !m_serializedBlock.empty() &&
           m_serializedBlock.size() <= kMaxSerializedBlockBytes &&
           m_proposedAt > 0 &&
           !m_signatureBundle.empty();
}

// static
std::string SignedBlockProposalMessage::buildSigningPayload(
    const std::string& proposerAddress,
    const crypto::PublicKey& proposerPublicKey,
    const std::string& blockHash,
    std::uint64_t blockIndex,
    std::uint64_t round,
    std::int64_t proposedAt
) {
    std::ostringstream oss;
    oss << "BlockProposalSigningPayload{"
        << "version=" << kPayloadVersion
        << ";proposerAddress=" << proposerAddress
        << ";proposerPublicKey=" << proposerPublicKey.serialize()
        << ";proposerPublicKeyFingerprint=" << proposerPublicKey.fingerprint()
        << ";blockHash=" << blockHash
        << ";blockIndex=" << blockIndex
        << ";round=" << round
        << ";proposedAt=" << proposedAt
        << "}";
    return oss.str();
}

bool SignedBlockProposalMessage::verify(
    const std::string& expectedProposer,
    const core::ValidatorRegistry& validatorRegistry,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) const {
    if (!isValid()) return false;

    if (m_proposerAddress != expectedProposer) return false;

    const core::ValidatorRegistryEntry* entry =
        validatorRegistry.entryForAddress(m_proposerAddress);
    if (entry == nullptr) return false;

    if (!entry->active()) return false;

    const crypto::PublicKey& registeredKey =
        entry->registrationRecord().validatorPublicKey();

    if (!registeredKey.isValid()) return false;
    if (registeredKey.keyMaterial() != m_proposerPublicKey.keyMaterial() ||
        registeredKey.algorithm() != m_proposerPublicKey.algorithm()) {
        return false;
    }

    const std::string payload = buildSigningPayload(
        m_proposerAddress,
        m_proposerPublicKey,
        m_blockHash,
        m_blockIndex,
        m_round,
        m_proposedAt
    );

    return m_signatureBundle.verifyForPolicy(
        payload,
        policy,
        crypto::SecurityContext::VALIDATOR_OPERATION,
        provider
    );
}

std::string SignedBlockProposalMessage::serialize() const {
    std::ostringstream oss;
    oss << "SignedBlockProposalMessage{"
        << "schema=" << SCHEMA_ID
        << ";proposerAddress=" << m_proposerAddress
        << ";proposerPublicKey=" << m_proposerPublicKey.serialize()
        << ";proposerPublicKeyFingerprint=" << m_proposerPublicKey.fingerprint()
        << ";blockIndex=" << m_blockIndex
        << ";round=" << m_round
        << ";blockHash=" << m_blockHash
        << ";proposedAt=" << m_proposedAt
        << ";serializedBlockBytes=" << m_serializedBlock.size()
        << ";signatureBundle=" << m_signatureBundle.serialize()
        << "}\n"
        << m_serializedBlock;
    return oss.str();
}

// static
SignedBlockProposalMessage SignedBlockProposalMessage::deserialize(const std::string& text) {
    const auto newlinePos = text.find('\n');
    if (newlinePos == std::string::npos || newlinePos + 1 >= text.size()) {
        throw std::invalid_argument("SignedBlockProposalMessage: missing block payload separator.");
    }

    const std::string header = text.substr(0, newlinePos);
    const std::string blockPayload = text.substr(newlinePos + 1);

    const std::string prefix = "SignedBlockProposalMessage{";
    if (header.size() <= prefix.size() ||
        header.compare(0, prefix.size(), prefix) != 0 ||
        header.back() != '}') {
        throw std::invalid_argument("SignedBlockProposalMessage: malformed header.");
    }

    const std::string body = header.substr(prefix.size(), header.size() - prefix.size() - 1);
    const auto fields = parseFields(body);

    if (requireField(fields, "schema") != std::string(SCHEMA_ID)) {
        throw std::invalid_argument("SignedBlockProposalMessage: unknown schema.");
    }

    const crypto::PublicKey proposerPublicKey =
        parsePublicKey(requireField(fields, "proposerPublicKey"));

    if (requireField(fields, "proposerPublicKeyFingerprint") != proposerPublicKey.fingerprint()) {
        throw std::invalid_argument("SignedBlockProposalMessage: public key fingerprint mismatch.");
    }

    const std::uint64_t declaredBlockBytes = parseU64(requireField(fields, "serializedBlockBytes"));
    if (blockPayload.size() != declaredBlockBytes) {
        throw std::invalid_argument("SignedBlockProposalMessage: block payload length mismatch.");
    }

    const crypto::SignatureBundle bundle = crypto::SignatureBundle::deserialize(
        requireField(fields, "signatureBundle")
    );

    for (const crypto::Signature& signature : bundle.signatures()) {
        if (signature.publicKey().serialize() != proposerPublicKey.serialize()) {
            throw std::invalid_argument(
                "SignedBlockProposalMessage: signature key does not match proposer key."
            );
        }
    }

    return SignedBlockProposalMessage(
        requireField(fields, "proposerAddress"),
        proposerPublicKey,
        parseU64(requireField(fields, "blockIndex")),
        parseU64(requireField(fields, "round")),
        requireField(fields, "blockHash"),
        blockPayload,
        parseI64(requireField(fields, "proposedAt")),
        bundle
    );
}

// static
SignedBlockProposalMessage SignedBlockProposalMessage::sign(
    const core::Block& block,
    const std::string& proposerAddress,
    const crypto::PublicKey& proposerPublicKey,
    const crypto::PrivateKey& proposerPrivateKey,
    std::uint64_t round,
    std::int64_t proposedAt,
    const crypto::SignatureProvider& provider
) {
    if (!block.isValid() || proposerAddress.empty() || !proposerPublicKey.isValid() ||
        round == 0 || proposedAt <= 0) {
        throw std::invalid_argument("SignedBlockProposalMessage::sign: invalid inputs.");
    }

    const std::string serializedBlock = block.serialize();
    const std::string blockHash = block.hash();

    const std::string payload = buildSigningPayload(
        proposerAddress,
        proposerPublicKey,
        blockHash,
        block.index(),
        round,
        proposedAt
    );

    const crypto::SignatureBundle bundle = crypto::SignatureBundle::createSignature(
        payload,
        proposerPublicKey,
        proposerPrivateKey,
        proposedAt,
        provider,
        crypto::SigningDomain::VALIDATOR_BLOCK_PROPOSAL
    );

    return SignedBlockProposalMessage(
        proposerAddress,
        proposerPublicKey,
        block.index(),
        round,
        blockHash,
        std::move(serializedBlock),
        proposedAt,
        std::move(bundle)
    );
}

} // namespace nodo::node
